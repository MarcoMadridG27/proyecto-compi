#!/usr/bin/env python3
"""Experimentos del proyecto (se ejecuta DENTRO del contenedor linux/amd64 mc-bench).

  rendimiento : benchmarks con y sin optimizaciones (impacto de cada pase), costo de cada
                mecanismo de seguridad y comparacion con gcc y clang
  seguridad   : programas con vulnerabilidades intencionales, ejecutados con el compilador
                desarrollado y con gcc/clang (-fstack-protector, sanitizers, -Wall)

Uso (desde la raiz del repositorio, ver benchmarks/ejecutar_experimentos.sh):
  python3 benchmarks/experimento.py --parte rendimiento|seguridad|todo [--runs 10] [--out DIR]
Cada ejecucion repite las mediciones --runs veces (mas una de calentamiento) y se guardan
todas las muestras para calcular media, desviacion estandar e intervalo de confianza.
"""
import argparse
import glob
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time

REPO = os.environ.get("REPO", "/w")
WORK = "/tmp/exp"
BUILD = "/tmp/mc_build"
RUNTIME = os.path.join(REPO, "runtime", "runtime.c")
HARDEN = "-fPIE -pie -Wl,-z,relro,-z,now,-z,noexecstack"

PASES = ["inline", "cprop", "fold", "strength", "dce", "sethi", "peephole"]
SEC = ["bounds", "null", "div", "overflow", "canary", "heap", "recursion", "harden"]


def sh(cmd, inp=None, timeout=120):
    t0 = time.perf_counter()
    try:
        p = subprocess.run(cmd, shell=True, capture_output=True, text=True, errors="replace", input=inp, timeout=timeout)
        return p.returncode, p.stdout, p.stderr, time.perf_counter() - t0
    except subprocess.TimeoutExpired:
        return 124, "", "timeout", time.perf_counter() - t0


def construir_compilador():
    shutil.rmtree(BUILD, ignore_errors=True)
    os.makedirs(BUILD)
    for ext in ("*.cpp", "*.h"):
        for f in glob.glob(os.path.join(REPO, ext)):
            shutil.copy(f, BUILD)
    shutil.copy(os.path.join(REPO, "Makefile"), BUILD)
    rc, out, err, _ = sh("cd %s && make -j4 compiler 2>&1 | tail -3" % BUILD, timeout=900)
    if not os.path.exists(os.path.join(BUILD, "compiler")):
        print("No se pudo construir el compilador:", out, err)
        sys.exit(1)


def entorno():
    info = {"arquitectura": platform.machine(), "sistema": platform.platform()}
    for nombre, cmd in [("gcc", "gcc --version | head -1"), ("clang", "clang --version | head -1"),
                        ("as", "as --version | head -1"), ("nproc", "nproc")]:
        info[nombre] = sh(cmd)[1].strip()
    cpu = sh("grep -m1 'model name' /proc/cpuinfo")[1].strip()
    info["cpu"] = cpu.split(":", 1)[-1].strip()
    return info


def configs_rendimiento():
    c = []
    c.append(("ours_O0", "ours", ["-O0"]))
    c.append(("ours_O1", "ours", ["-O1"]))
    for p in PASES:
        c.append(("ours_only_" + p, "ours", ["-O1", "--passes", p]))
    for s in SEC:
        c.append(("ours_sec_" + s, "ours", ["-O0", "--sec=" + s]))
    c.append(("ours_sec_all", "ours", ["-O0", "--sec=all"]))
    c.append(("ours_O1_sec_all", "ours", ["-O1", "--sec=all"]))
    for o in ("O0", "O2"):
        c.append(("gcc_" + o, "gcc", ["-" + o]))
        c.append(("clang_" + o, "clang", ["-" + o]))
    c.append(("gcc_O0_stackprot", "gcc", ["-O0", "-fstack-protector-all"]))
    c.append(("gcc_O0_asan_ubsan", "gcc", ["-O0", "-fsanitize=address,undefined"]))
    return c


def construir(bench_src, nombre, tipo, flags, destino):
    """Devuelve (exe, tiempo_compilacion_ms[], bytes_texto, instrucciones, bytes_exe) o None."""
    os.makedirs(destino, exist_ok=True)
    exe = os.path.join(destino, nombre + ".exe")
    obj = os.path.join(destino, nombre + ".o")
    tiempos = []
    if tipo == "ours":
        asm = os.path.join(destino, nombre + ".s")
        cmd_c = "%s/compiler %s --asm %s %s" % (BUILD, bench_src, asm, " ".join(flags))
        cmd_o = "gcc -c %s -o %s" % (asm, obj)
        for _ in range(3):
            rc1, _, e1, t1 = sh(cmd_c)
            rc2, _, e2, t2 = sh(cmd_o)
            if rc1 != 0 or rc2 != 0:
                return None
            tiempos.append((t1 + t2) * 1000)
        enlazar = "gcc %s %s -o %s" % (obj, RUNTIME, exe)
        if "--sec=harden" in flags or "--sec=all" in flags:
            enlazar = "gcc %s %s %s -o %s" % (obj, RUNTIME, HARDEN, exe)
        elif any(f.startswith("--sec=") for f in flags):
            enlazar = "gcc %s %s -o %s" % (obj, RUNTIME, exe)
        rc, _, e, _ = sh(enlazar)
        if rc != 0:
            return None
    else:
        cxx = "g++" if tipo == "gcc" else "clang++"
        cmd_o = "%s -std=c++14 -w %s -c %s -o %s" % (cxx, " ".join(flags), bench_src, obj)
        for _ in range(3):
            rc, _, e, t = sh(cmd_o)
            if rc != 0:
                return None
            tiempos.append(t * 1000)
        rc, _, e, _ = sh("%s %s %s -o %s" % (cxx, " ".join(flags), obj, exe))
        if rc != 0:
            return None
    texto = 0
    for linea in sh("size -A %s" % obj)[1].splitlines():
        m = re.match(r"^(\.text\S*)\s+(\d+)", linea)
        if m:
            texto += int(m.group(2))
    insts = len([l for l in sh("objdump -d --no-show-raw-insn %s" % obj)[1].splitlines() if re.match(r"^\s+[0-9a-f]+:\s", l)])
    return exe, tiempos, texto, insts, os.path.getsize(exe)


def medir(exe, runs):
    sh(exe, timeout=120)   # calentamiento
    muestras = []
    salida = ""
    rc = 0
    for _ in range(runs):
        rc, out, err, t = sh(exe, timeout=180)
        muestras.append(t * 1000)
        salida = out
    return muestras, salida, rc


def parte_rendimiento(runs, out):
    benches = sorted(glob.glob(os.path.join(REPO, "benchmarks", "b0*.cpp")))
    configs = configs_rendimiento()
    resultados = {"entorno": entorno(), "runs": runs, "benchmarks": {}}
    ref_salida = {}
    for b in benches:
        nombre_b = os.path.basename(b)[:-4]
        # salida de referencia: gcc -O0
        rc, _, e, _ = sh("g++ -std=c++14 -w -O0 %s -o %s/%s.ref" % (b, WORK, nombre_b))
        ref_salida[nombre_b] = sh("%s/%s.ref" % (WORK, nombre_b), timeout=180)[1]
        resultados["benchmarks"][nombre_b] = {}
        for nombre, tipo, flags in configs:
            r = construir(b, nombre, tipo, flags, os.path.join(WORK, nombre_b))
            if r is None:
                print("  [omitido] %s %s" % (nombre_b, nombre), flush=True)
                continue
            exe, tcomp, texto, insts, tam = r
            muestras, salida, rc = medir(exe, runs)
            resultados["benchmarks"][nombre_b][nombre] = {
                "compile_ms": tcomp, "text_bytes": texto, "instructions": insts, "exe_bytes": tam,
                "run_ms": muestras, "same_output": salida == ref_salida[nombre_b], "exit": rc,
            }
            media = sum(muestras) / len(muestras)
            print("  %-12s %-24s %8.1f ms  insts=%-6d %s" % (nombre_b, nombre, media, insts,
                                                           "" if salida == ref_salida[nombre_b] else "SALIDA DISTINTA"), flush=True)
            os.remove(exe)
    with open(os.path.join(out, "rendimiento.json"), "w") as f:
        json.dump(resultados, f)


# ------------------------------------------------------------------------------------
# Seguridad
# ------------------------------------------------------------------------------------

MSG_DIAG = [
    "Error de seguridad", "stack smashing detected", "AddressSanitizer", "runtime error", "double free detected",
    "free(): invalid", "malloc(): corrupted", "free(): invalid pointer", "munmap_chunk", "corrupted size",
    "buffer overflow detected", "*** stack smashing", "UndefinedBehaviorSanitizer",
]


def clasificar(rc, err):
    """D = diagnostico especifico, C = terminacion anormal sin diagnostico, N = no detectado."""
    if any(m in err for m in MSG_DIAG):
        return "D"
    if rc in (124,) or rc >= 128 or rc < 0:
        return "C"
    return "N"


def ejecutar_prog(exe):
    rc, out, err, _ = sh("timeout 20 %s < /dev/null" % exe, timeout=40)
    return clasificar(rc, err), rc, err.strip()[:200]


def parte_seguridad(out):
    progs = sorted(glob.glob(os.path.join(REPO, "vulnerabilidades", "v*.cpp")))
    res = {"entorno": entorno(), "programas": {}}
    d = os.path.join(WORK, "vuln")
    os.makedirs(d, exist_ok=True)
    for src in progs:
        n = os.path.basename(src)[:-4]
        r = {}
        # compilador propio: analisis estatico
        rc, so, se, _ = sh("%s/compiler %s --sec=static --asm %s/%s.static.s" % (BUILD, src, d, n))
        avisos = [l.strip() for l in so.splitlines() if "Advertencia de seguridad" in l]
        r["ours_static"] = {"detected": bool(avisos), "messages": avisos}
        # compilador propio: cada mecanismo dinamico por separado y todos juntos
        for flag in ["bounds", "null", "div", "overflow", "canary", "heap", "recursion", "all"]:
            args = "--sec=%s" % flag + (" -O1" if flag == "all" else "")
            asm = "%s/%s.%s.s" % (d, n, flag)
            rc, so, se, _ = sh("%s/compiler %s --asm %s %s" % (BUILD, src, asm, args))
            if rc != 0:
                r["ours_" + flag] = {"class": "ERR", "exit": rc, "stderr": (so + se)[-100:]}
                continue
            exe = "%s/%s.%s.exe" % (d, n, flag)
            if sh("gcc %s %s -o %s" % (asm, RUNTIME, exe))[0] != 0:
                r["ours_" + flag] = {"class": "ERR", "exit": -1, "stderr": "ensamblado"}
                continue
            c, rc, err = ejecutar_prog(exe)
            r["ours_" + flag] = {"class": c, "exit": rc, "stderr": err}
        # sin proteccion (nuestro -O0)
        asm = "%s/%s.none.s" % (d, n)
        sh("%s/compiler %s --asm %s" % (BUILD, src, asm))
        exe = "%s/%s.none.exe" % (d, n)
        sh("gcc %s %s -o %s" % (asm, RUNTIME, exe))
        c, rc, err = ejecutar_prog(exe)
        r["ours_none"] = {"class": c, "exit": rc, "stderr": err}
        # gcc y clang
        for etiqueta, cxx, flags in [("gcc_plain", "g++", "-O0"), ("gcc_stackprot", "g++", "-O0 -fstack-protector-all"),
                                     ("gcc_asan_ubsan", "g++", "-O0 -fsanitize=address,undefined"),
                                     ("clang_asan_ubsan", "clang++", "-O0 -fsanitize=address,undefined")]:
            exe = "%s/%s.%s.exe" % (d, n, etiqueta)
            rc, _, e, _ = sh("%s -std=c++14 -w %s %s -o %s" % (cxx, flags, src, exe))
            if rc != 0:
                r[etiqueta] = {"class": "ERR", "exit": rc, "stderr": e[-100:]}
                continue
            c, rc, err = ejecutar_prog(exe)
            r[etiqueta] = {"class": c, "exit": rc, "stderr": err}
        # advertencias de compilacion de gcc y clang (analisis estatico equivalente)
        rc, so, se, _ = sh("g++ -std=c++14 -Wall -Wextra -O2 -fsyntax-only %s" % src)
        gw = [l.strip() for l in se.splitlines() if "warning:" in l]
        r["gcc_Wall"] = {"detected": bool(gw), "messages": gw[:5]}
        rc, so, se, _ = sh("clang++ -std=c++14 -Wall -Wextra -O2 -c -o /dev/null %s" % src)
        cw = [l.strip() for l in se.splitlines() if "warning:" in l and "unused" not in l.split("warning:")[1][:30]]
        r["clang_Wall"] = {"detected": bool(cw), "messages": cw[:5]}
        rc, so, se, _ = sh("clang++ -std=c++14 --analyze -Xanalyzer -analyzer-output=text %s" % src)
        ca = [l.strip() for l in (so + se).splitlines() if "warning:" in l]
        r["clang_analyze"] = {"detected": bool(ca), "messages": ca[:5]}
        res["programas"][n] = r
        print("  %-34s ours_static=%s all=%s | gcc: sp=%s asan=%s | clang: asan=%s" % (
            n, "S" if avisos else "-", r["ours_all"]["class"], r["gcc_stackprot"]["class"],
            r["gcc_asan_ubsan"]["class"], r["clang_asan_ubsan"]["class"]), flush=True)
    with open(os.path.join(out, "seguridad.json"), "w") as f:
        json.dump(res, f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--parte", default="todo", choices=["rendimiento", "seguridad", "todo"])
    ap.add_argument("--runs", type=int, default=10)
    ap.add_argument("--out", default=os.path.join(REPO, "benchmarks", "resultados"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    print("Construyendo el compilador dentro del contenedor...", flush=True)
    construir_compilador()
    if a.parte in ("seguridad", "todo"):
        print("== Seguridad ==", flush=True)
        parte_seguridad(a.out)
    if a.parte in ("rendimiento", "todo"):
        print("== Rendimiento (%d ejecuciones) ==" % a.runs, flush=True)
        parte_rendimiento(a.runs, a.out)
    print("Resultados en", a.out)


if __name__ == "__main__":
    main()
