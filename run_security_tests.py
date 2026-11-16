#!/usr/bin/env python3
"""Pruebas de seguridad: programas con vulnerabilidades intencionales de vulnerabilidades/.

Marcas dentro de cada .cpp:
  // STATIC: texto           el analisis estatico (--sec=static) debe informar ese texto
  // DYNAMIC: flag | mensaje con --sec=flag el programa debe terminar con codigo 70 y el
                             mensaje en stderr; sin la bandera no debe terminar con 70
Ademas comprueba que:
  * con --sec=all -O1 las violaciones dinamicas se siguen detectando (las optimizaciones no
    deben ocultar errores de seguridad);
  * el analisis estatico no produce falsos positivos de desreferencia nula, variable sin
    inicializar, indice fuera de rango, doble liberacion o puntero colgante sobre los
    programas validos de tests_gencode/.
En Linux x86-64 usa gcc local; en otras plataformas usa Docker (gcc para linux/amd64).
"""
import glob
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_gencode_tests import ejecutar_script, escribir_script, RUNTIME  # noqa: E402

COMPILER = "./compiler"
OUT = os.path.join("outputs", "security")
VULN = "vulnerabilidades"
PROHIBIDOS = ["Desreferencia de puntero nulo", "sin haber sido inicializada", "fuera de rango", "Doble liberación",
              "Puntero colgante", "Uso después de liberar", "puede terminar sin retornar"]


def marcas(path):
    estaticas, dinamicas = [], []
    for linea in open(path, encoding="utf-8"):
        m = re.search(r"//\s*STATIC:\s*(.*)$", linea)
        if m:
            estaticas.append(m.group(1).strip())
        m = re.search(r"//\s*DYNAMIC:\s*(\S+)\s*\|\s*(.*)$", linea)
        if m:
            dinamicas.append((m.group(1).strip(), m.group(2).strip()))
    return estaticas, dinamicas


def main():
    if subprocess.run(["make", "-s"], capture_output=True).returncode != 0:
        print("Error al compilar el compilador con make")
        return 1
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(OUT, exist_ok=True)
    progs = sorted(glob.glob(os.path.join(VULN, "*.cpp")))
    resultados = {}
    lineas = []
    corridas = []   # (nombre, etiqueta, esperar_violacion, mensaje)

    for f in progs:
        n = os.path.basename(f)[:-4]
        estaticas, dinamicas = marcas(f)
        problemas = []
        if estaticas:
            p = subprocess.run([COMPILER, f, "--sec=static", "--asm", os.path.join(OUT, n + ".static.s")], capture_output=True, text=True)
            for e in estaticas:
                if e not in p.stdout:
                    problemas.append("falta aviso estatico: " + e)
        for flag, msg in dinamicas:
            variantes = [("base", [], False), (flag, ["--sec=" + flag], True), ("all-O1", ["--sec=all", "-O1"], True)]
            for etiqueta, extra, esperar in variantes:
                asm = os.path.join(OUT, f"{n}.{etiqueta}.s")
                p = subprocess.run([COMPILER, f, "--asm", asm] + extra, capture_output=True, text=True)
                if p.returncode != 0:
                    problemas.append(f"el compilador fallo con {etiqueta}: " + (p.stdout + p.stderr)[-150:])
                    continue
                exe = os.path.join(OUT, f"{n}.{etiqueta}.bin")
                out = os.path.join(OUT, f"{n}.{etiqueta}")
                lineas.append(f"gcc {asm} {RUNTIME} -o {exe} 2> {out}.gcc || {{ echo 'FALLO ensamblado' > {out}.res; continue; }}")
                lineas.append(f"timeout 20 {exe} < /dev/null > {out}.out 2> {out}.err; echo $? > {out}.res")
                corridas.append((n, etiqueta, esperar, msg))
        resultados[n] = problemas

    if lineas:
        script = os.path.join(OUT, "ejecutar.sh")
        escribir_script(script, lineas)
        r = ejecutar_script(script)
        if r.returncode != 0:
            print(r.stdout + r.stderr)

    for n, etiqueta, esperar, msg in corridas:
        base = os.path.join(OUT, f"{n}.{etiqueta}")
        if not os.path.exists(base + ".res"):
            resultados[n].append(f"{etiqueta}: no se ejecuto")
            continue
        codigo = open(base + ".res").read().strip()
        err = open(base + ".err", errors="replace").read() if os.path.exists(base + ".err") else ""
        if esperar:
            # con --sec=all puede dispararse otro mecanismo antes: se acepta cualquiera de los esperados
            validos = [msg] if etiqueta != "all-O1" else [m for (nn, _e, _x, m) in corridas if nn == n]
            if codigo != "70" or not any(m in err for m in validos):
                resultados[n].append(f"{etiqueta}: esperaba codigo 70 y '{msg}'; obtuvo codigo {codigo} stderr='{err.strip()[:80]}'")
        elif codigo == "70":
            resultados[n].append(f"{etiqueta}: sin proteccion no deberia terminar con 70")

    # endurecimiento del binario: PIE, pila no ejecutable y RELRO completo
    hard = []
    asm = os.path.join(OUT, "harden.s")
    p = subprocess.run([COMPILER, "tests_gencode/g01_arith.cpp" if os.path.exists("tests_gencode/g01_arith.cpp") else sorted(glob.glob("tests_gencode/g*.cpp"))[0],
                        "--asm", asm, "--sec=harden"], capture_output=True, text=True)
    hscript = os.path.join(OUT, "harden.sh")
    escribir_script(hscript, [
        "gcc %s %s -fPIE -pie -Wl,-z,relro,-z,now,-z,noexecstack -o %s/harden.bin" % (asm, RUNTIME, OUT),
        "readelf -h %s/harden.bin | grep -q 'Type:.*DYN' && echo PIE_OK > %s/harden.res || echo PIE_FALLA > %s/harden.res" % (OUT, OUT, OUT),
        "readelf -lW %s/harden.bin | grep GNU_STACK | grep -q RWE && echo NX_FALLA >> %s/harden.res || echo NX_OK >> %s/harden.res" % (OUT, OUT, OUT),
        "readelf -dW %s/harden.bin | grep -q BIND_NOW && echo RELRO_OK >> %s/harden.res || echo RELRO_FALLA >> %s/harden.res" % (OUT, OUT, OUT),
        "%s/harden.bin > /dev/null < /dev/null; echo EXIT_$? >> %s/harden.res" % (OUT, OUT),
    ])
    ejecutar_script(hscript)
    hres = open(os.path.join(OUT, "harden.res")).read().split() if os.path.exists(os.path.join(OUT, "harden.res")) else ["SIN_RESULTADO"]
    hard_ok = hres[:3] == ["PIE_OK", "NX_OK", "RELRO_OK"] and p.returncode == 0

    # falsos positivos del analisis estatico sobre programas validos
    falsos = []
    for f in sorted(glob.glob("tests_gencode/[gh]*.cpp")) + sorted(glob.glob("tests_opt/o0[1-9]*.cpp")):
        p = subprocess.run([COMPILER, f, "--sec=static", "--asm", os.path.join(OUT, "fp.s")], capture_output=True, text=True)
        for linea in p.stdout.splitlines():
            if "Advertencia de seguridad" in linea and any(x in linea for x in PROHIBIDOS):
                falsos.append(f + ": " + linea.strip())

    print("=" * 78)
    pasaron = 0
    for f in progs:
        n = os.path.basename(f)[:-4]
        pr = resultados.get(n, [])
        print(f"{n:<36} {'PASO' if not pr else 'FALLO: ' + ' | '.join(pr)}")
        pasaron += not pr
    print("=" * 78)
    print(f"Endurecimiento del binario (PIE, pila no ejecutable, RELRO): {'ok' if hard_ok else 'FALLO ' + ' '.join(hres)}")
    print(f"Sin falsos positivos del analisis estatico: {'si' if not falsos else 'NO'}")
    for x in falsos:
        print("  " + x)
    print(f"Pruebas de seguridad: {pasaron}/{len(progs)} superadas")
    return 0 if pasaron == len(progs) and not falsos and hard_ok else 1


if __name__ == "__main__":
    sys.exit(main())
