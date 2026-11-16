#!/usr/bin/env python3
"""Genera las tablas (.tex) y las figuras (.png) del reporte a partir de los resultados
medidos por benchmarks/experimento.py (benchmarks/resultados/*.json).

Uso:  python3 documentacion/reporte/generar_datos.py
Salida: documentacion/reporte/tablas/*.tex y documentacion/reporte/figuras/*.png
"""
import json
import math
import os
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

AQUI = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(AQUI, "..", "..", "benchmarks", "resultados")
TAB = os.path.join(AQUI, "tablas")
FIG = os.path.join(AQUI, "figuras")
os.makedirs(TAB, exist_ok=True)
os.makedirs(FIG, exist_ok=True)

T95 = {2: 12.71, 3: 4.30, 4: 3.18, 5: 2.78, 6: 2.57, 7: 2.45, 8: 2.36, 9: 2.31, 10: 2.26, 11: 2.23, 12: 2.20,
       15: 2.14, 20: 2.09, 30: 2.05}
PASES = ["inline", "cprop", "fold", "strength", "dce", "sethi", "peephole"]
SEC = ["bounds", "null", "div", "overflow", "canary", "heap", "recursion", "harden"]
NOMBRES = {
    "b01_fib": "fib", "b02_sieve": "criba", "b03_matmul": "matmul", "b04_primos": "primos",
    "b05_oscilador": "oscilador", "b06_vectores": "vectores", "b07_burbuja": "burbuja", "b08_memoria": "memoria",
}


def esc(s):
    return s.replace("_", r"\_").replace("%", r"\%").replace("&", r"\&").replace("#", r"\#")


def stats(xs):
    n = len(xs)
    m = sum(xs) / n
    sd = statistics.stdev(xs) if n > 1 else 0.0
    t = T95.get(n, 2.0)
    return m, sd, t * sd / math.sqrt(n), statistics.median(xs), min(xs)


def geomean(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else float("nan")


def write(name, text):
    with open(os.path.join(TAB, name), "w", encoding="utf-8") as f:
        f.write(text)


def rendimiento():
    path = os.path.join(RES, "rendimiento.json")
    if not os.path.exists(path):
        print("sin rendimiento.json")
        return
    d = json.load(open(path))
    B = d["benchmarks"]
    runs = d["runs"]
    # se descartan las mediciones cuyo ejecutable se detuvo manualmente (salida distinta de la de referencia)
    for b in B:
        for c in list(B[b]):
            if c.startswith("ours_sec_heap") and not B[b][c]["same_output"]:
                del B[b][c]
    bn = sorted(B)
    mean = lambda b, c: stats(B[b][c]["run_ms"])[0] if c in B[b] else float("nan")

    # entorno
    e = d["entorno"]
    write("entorno.tex", "\\begin{tabular}{ll}\\toprule\nElemento & Valor \\\\\\midrule\n" + "".join(
        "%s & %s \\\\\n" % (esc(k), esc(str(v))) for k, v in [
            ("Arquitectura", e["arquitectura"]), ("Sistema", e["sistema"]), ("CPU", e["cpu"]),
            ("Compilador de referencia 1", e["gcc"]), ("Compilador de referencia 2", e["clang"]),
            ("Ensamblador", e["as"]), ("Ejecuciones por medicion", str(runs) + " (mas una de calentamiento)")]) +
        "\\bottomrule\\end{tabular}\n")

    # T1: tiempos por benchmark
    cols = [("gcc_O0", "gcc -O0"), ("gcc_O2", "gcc -O2"), ("clang_O0", "clang -O0"), ("clang_O2", "clang -O2"),
            ("ours_O0", "propio -O0"), ("ours_O1", "propio -O1")]
    t = "\\begin{tabular}{l" + "r" * len(cols) + "rr}\\toprule\nPrograma & " + " & ".join(c[1] for c in cols) + \
        " & Aceleracion -O1/-O0 & -O1 / gcc -O2 \\\\\\midrule\n"
    acc, rel = [], []
    for b in bn:
        cells = []
        for c, _ in cols:
            if c in B[b]:
                m, sd, ci, med, mn = stats(B[b][c]["run_ms"])
                cells.append("%.0f $\\pm$ %.0f" % (m, sd))
            else:
                cells.append("--")
        a = mean(b, "ours_O0") / mean(b, "ours_O1")
        r = mean(b, "ours_O1") / mean(b, "gcc_O2")
        acc.append(a)
        rel.append(r)
        t += "%s & %s & %.2f$\\times$ & %.1f$\\times$ \\\\\n" % (NOMBRES.get(b, b), " & ".join(cells), a, r)
    t += "\\midrule\nMedia geometrica & " + " & ".join("" for _ in cols) + " & %.2f$\\times$ & %.1f$\\times$ \\\\\n" % (
        geomean(acc), geomean(rel))
    t += "\\bottomrule\\end{tabular}\n"
    write("rendimiento.tex", t)
    write("resumen_numeros.tex",
          "\\newcommand{\\aceleracionMedia}{%.2f}\n\\newcommand{\\vsGccMedia}{%.1f}\n\\newcommand{\\runsPorMedicion}{%d}\n" % (
              geomean(acc), geomean(rel), runs))

    # Figura 1: tiempos por configuracion (escala logaritmica)
    fig, ax = plt.subplots(figsize=(9, 4))
    ancho = 0.13
    conf = ["gcc_O0", "gcc_O2", "clang_O0", "clang_O2", "ours_O0", "ours_O1"]
    etiquetas = ["gcc -O0", "gcc -O2", "clang -O0", "clang -O2", "propio -O0", "propio -O1"]
    for i, (c, lab) in enumerate(zip(conf, etiquetas)):
        vals = [mean(b, c) for b in bn]
        errs = [stats(B[b][c]["run_ms"])[1] if c in B[b] else 0 for b in bn]
        ax.bar([x + i * ancho for x in range(len(bn))], vals, ancho, yerr=errs, label=lab, capsize=1.5)
    ax.set_xticks([x + 2.5 * ancho for x in range(len(bn))])
    ax.set_xticklabels([NOMBRES.get(b, b) for b in bn])
    ax.set_yscale("log")
    ax.set_ylabel("tiempo de ejecucion (ms, escala log)")
    ax.legend(ncol=3, fontsize=7)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(FIG, "tiempos.png"), dpi=160)
    plt.close(fig)

    # T2: impacto de cada optimizacion (cada pase solo frente a -O0)
    t = "\\begin{tabular}{lrrr}\\toprule\nPase & Aceleracion (media geom.) & Mejor caso & Instrucciones (\\%) \\\\\\midrule\n"
    imp_solo = []
    for p in PASES:
        sp = [mean(b, "ours_O0") / mean(b, "ours_only_" + p) for b in bn if "ours_only_" + p in B[b]]
        mejor = max(sp) if sp else float("nan")
        inst = [100.0 * (B[b]["ours_only_" + p]["instructions"] - B[b]["ours_O0"]["instructions"]) / B[b]["ours_O0"]["instructions"]
                for b in bn if "ours_only_" + p in B[b]]
        imp_solo.append(geomean(sp))
        t += "%s & %.3f$\\times$ & %.2f$\\times$ & %+.1f \\\\\n" % (esc(p), geomean(sp), mejor, statistics.mean(inst))
    t += "\\midrule\nTodos (-O1) & %.2f$\\times$ & %.2f$\\times$ & %+.1f \\\\\n" % (
        geomean(acc), max(acc), statistics.mean([100.0 * (B[b]["ours_O1"]["instructions"] - B[b]["ours_O0"]["instructions"]) / B[b]["ours_O0"]["instructions"] for b in bn]))
    t += "\\bottomrule\\end{tabular}\n"
    write("impacto_pases.tex", t)
    fig, ax = plt.subplots(figsize=(6.5, 3.2))
    ax.bar(range(len(PASES)), [(1 / v - 1) * 100 for v in imp_solo], color="#4c72b0")
    ax.set_xticks(range(len(PASES)))
    ax.set_xticklabels(PASES, rotation=20)
    ax.set_ylabel("variacion del tiempo vs -O0 (%)")
    ax.axhline(0, color="k", lw=0.6)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(FIG, "impacto_pases.png"), dpi=160)
    plt.close(fig)

    # T3: calidad del codigo
    t = "\\begin{tabular}{lrrrrr}\\toprule\nConfiguracion & Instrucciones & Bytes de .text & Tamano del ejecutable (KB) & Compilacion (ms) & Tiempo ejec. (ms) \\\\\\midrule\n"
    for c, lab in [("gcc_O0", "gcc -O0"), ("gcc_O2", "gcc -O2"), ("clang_O0", "clang -O0"), ("clang_O2", "clang -O2"),
                   ("ours_O0", "propio -O0"), ("ours_O1", "propio -O1")]:
        ins = statistics.mean([B[b][c]["instructions"] for b in bn if c in B[b]])
        tx = statistics.mean([B[b][c]["text_bytes"] for b in bn if c in B[b]])
        ex = statistics.mean([B[b][c]["exe_bytes"] for b in bn if c in B[b]]) / 1024
        cm = statistics.mean([statistics.mean(B[b][c]["compile_ms"]) for b in bn if c in B[b]])
        tm = geomean([mean(b, c) for b in bn if c in B[b]])
        t += "%s & %.0f & %.0f & %.1f & %.1f & %.0f \\\\\n" % (lab, ins, tx, ex, cm, tm)
    t += "\\bottomrule\\end{tabular}\n"
    write("calidad_codigo.tex", t)

    # T4: costo de la seguridad
    t = "\\begin{tabular}{lrrrr}\\toprule\nMecanismo & Sobrecarga media (\\%) & Peor caso (\\%) & Instrucciones (\\%) & .text (\\%) \\\\\\midrule\n"
    costos = []
    for s in SEC + ["all"]:
        c = "ours_sec_" + s
        ov = [100.0 * (mean(b, c) / mean(b, "ours_O0") - 1) for b in bn if c in B[b]]
        inst = [100.0 * (B[b][c]["instructions"] / B[b]["ours_O0"]["instructions"] - 1) for b in bn if c in B[b]]
        tx = [100.0 * (B[b][c]["text_bytes"] / B[b]["ours_O0"]["text_bytes"] - 1) for b in bn if c in B[b]]
        g = 100.0 * (geomean([mean(b, c) / mean(b, "ours_O0") for b in bn if c in B[b]]) - 1)
        costos.append((s, g))
        t += "%s & %+.1f & %+.1f & %+.1f & %+.1f \\\\\n" % (esc("todos (--sec=all)" if s == "all" else s), g, max(ov), statistics.mean(inst), statistics.mean(tx))
    ov = [100.0 * (mean(b, "ours_O1_sec_all") / mean(b, "ours_O1") - 1) for b in bn if "ours_O1_sec_all" in B[b]]
    t += "\\midrule\ntodos con -O1 (vs -O1) & %+.1f & %+.1f & -- & -- \\\\\n" % (
        100.0 * (geomean([mean(b, "ours_O1_sec_all") / mean(b, "ours_O1") for b in bn if "ours_O1_sec_all" in B[b]]) - 1), max(ov))
    t += "\\bottomrule\\end{tabular}\n"
    write("costo_seguridad.tex", t)

    t = "\\begin{tabular}{lrr}\\toprule\nReferencia (vs gcc -O0) & Sobrecarga media (\\%) & Peor caso (\\%) \\\\\\midrule\n"
    refs = []
    for c, lab in [("gcc_O0_stackprot", "gcc -fstack-protector-all"),
                   ("gcc_O0_asan_ubsan", "gcc -fsanitize=address,undefined")]:
        base = "gcc_O0" if c.startswith("gcc") else "clang_O0"
        ov = [100.0 * (mean(b, c) / mean(b, base) - 1) for b in bn if c in B[b]]
        if not ov:
            continue
        g = 100.0 * (geomean([mean(b, c) / mean(b, base) for b in bn if c in B[b]]) - 1)
        refs.append((lab, g))
        t += "%s & %+.1f & %+.1f \\\\\n" % (esc(lab), g, max(ov))
    t += "\\bottomrule\\end{tabular}\n"
    write("costo_seguridad_ref.tex", t)
    fig, ax = plt.subplots(figsize=(8, 3.6))
    nombres = [("todos" if s == "all" else s) for s, _ in costos] + [r[0].replace("gcc -fsanitize=", "gcc ").replace("-fstack-protector-all", "stack-prot") for r in refs]
    vals = [v for _, v in costos] + [v for _, v in refs]
    colores = ["#4c72b0"] * len(costos) + ["#c44e52"] * len(refs)
    ax.bar(range(len(vals)), vals, color=colores)
    ax.set_xticks(range(len(vals)))
    ax.set_xticklabels(nombres, rotation=35, ha="right", fontsize=7)
    ax.set_ylabel("sobrecarga de tiempo (%)")
    ax.grid(axis="y", alpha=0.3)
    ax.set_title("Costo de la seguridad (azul: este compilador, rojo: gcc/clang)", fontsize=9)
    fig.tight_layout()
    fig.savefig(os.path.join(FIG, "costo_seguridad.png"), dpi=160)
    plt.close(fig)

    # tabla completa por benchmark (apendice)
    cs = ["ours_O0", "ours_O1"] + ["ours_only_" + p for p in PASES]
    t = "\\begin{tabular}{l" + "r" * len(cs) + "}\\toprule\nPrograma & " + " & ".join(esc(c.replace("ours_", "")) for c in cs) + " \\\\\\midrule\n"
    for b in bn:
        t += "%s & %s \\\\\n" % (NOMBRES.get(b, b), " & ".join("%.0f" % mean(b, c) for c in cs))
    t += "\\bottomrule\\end{tabular}\n"
    write("apendice_pases.tex", t)
    cs = ["ours_O0"] + ["ours_sec_" + s for s in SEC] + ["ours_sec_all"]
    t = "\\begin{tabular}{l" + "r" * len(cs) + "}\\toprule\nPrograma & " + " & ".join(esc(c.replace("ours_", "").replace("sec_", "")) for c in cs) + " \\\\\\midrule\n"
    for b in bn:
        t += "%s & %s \\\\\n" % (NOMBRES.get(b, b), " & ".join("%.0f" % mean(b, c) for c in cs))
    t += "\\bottomrule\\end{tabular}\n"
    write("apendice_seguridad.tex", t)

    # fidelidad
    ok = sum(1 for b in bn for c in B[b] if B[b][c]["same_output"])
    tot = sum(1 for b in bn for c in B[b])
    write("fidelidad.tex", "\\newcommand{\\fidelidadOk}{%d}\n\\newcommand{\\fidelidadTotal}{%d}\n" % (ok, tot))
    print("rendimiento: tablas y figuras generadas (salidas iguales: %d/%d)" % (ok, tot))


def seguridad():
    path = os.path.join(RES, "seguridad.json")
    if not os.path.exists(path):
        print("sin seguridad.json")
        return
    d = json.load(open(path))["programas"]
    nombres = {
        "v01": "desbordamiento de pila", "v02": "indice constante fuera de rango", "v03": "puntero nulo evidente",
        "v04": "puntero nulo dinamico", "v05": "division entre cero", "v06": "desbordamiento de entero",
        "v07": "desbordamiento constante", "v08": "uso despues de liberar", "v09": "doble liberacion",
        "v10": "variable sin inicializar", "v11": "retorno faltante", "v12": "recursion infinita",
        "v13": "puntero colgante", "v14": "desbordamiento de heap", "v15": "indice de string", "v16": "conversion insegura",
    }
    sim = {"D": "D", "C": "C", "N": "--", "ERR": "?"}
    flags = ["bounds", "null", "div", "overflow", "canary", "heap", "recursion"]
    t = ("\\begin{tabular}{llcccccccc}\\toprule\n & & \\multicolumn{3}{c}{Este compilador} & \\multicolumn{2}{c}{gcc} & "
         "\\multicolumn{2}{c}{clang} & gcc \\\\\n\\cmidrule(lr){3-5}\\cmidrule(lr){6-7}\\cmidrule(lr){8-9}\n"
         "Programa & Vulnerabilidad & estatico & dinamico (mecanismo) & todos & stack-prot & asan+ubsan & asan+ubsan & -Wall \\\\\\midrule\n")
    conteo = {"est": 0, "din": 0, "ours": 0, "sp": 0, "ga": 0, "ca": 0, "wall": 0, "cwall": 0}
    for n, r in d.items():
        k = n[:3]
        est = r["ours_static"]["detected"]
        flags_d = [f for f in flags if r["ours_" + f]["class"] == "D" and not (r["ours_none"]["class"] == "D")]
        todos = r["ours_all"]["class"]
        ours_ok = est or todos == "D"
        conteo["est"] += est
        conteo["din"] += bool(flags_d)
        conteo["ours"] += ours_ok
        conteo["sp"] += r["gcc_stackprot"]["class"] == "D"
        conteo["ga"] += r["gcc_asan_ubsan"]["class"] == "D"
        conteo["ca"] += r["clang_asan_ubsan"]["class"] == "D"
        conteo["wall"] += r["gcc_Wall"]["detected"]
        t += "%s & %s & %s & %s & %s & %s & %s & %s & %s \\\\\n" % (
            esc(n[:3]), nombres.get(k, n), "S" if est else "--", esc(", ".join(flags_d)) if flags_d else "--",
            sim.get(todos, "?"), sim.get(r["gcc_stackprot"]["class"], "?"), sim.get(r["gcc_asan_ubsan"]["class"], "?"),
            sim.get(r["clang_asan_ubsan"]["class"], "?"), "W" if r["gcc_Wall"]["detected"] else "--")
    n = len(d)
    t += "\\midrule\nDetectadas & & %d/%d & %d/%d & %d/%d & %d/%d & %d/%d & %d/%d & %d/%d \\\\\n" % (
        conteo["est"], n, conteo["din"], n, sum(1 for r in d.values() if r["ours_all"]["class"] == "D"), n, conteo["sp"], n,
        conteo["ga"], n, conteo["ca"], n, conteo["wall"], n)
    t += "\\bottomrule\\end{tabular}\n"
    write("efectividad.tex", t)
    write("resumen_seguridad.tex", "\\newcommand{\\vulnTotal}{%d}\n\\newcommand{\\vulnPropio}{%d}\n\\newcommand{\\vulnEstatico}{%d}\n"
          "\\newcommand{\\vulnDinamico}{%d}\n\\newcommand{\\vulnStackProt}{%d}\n\\newcommand{\\vulnAsan}{%d}\n" % (
              n, conteo["ours"], conteo["est"], sum(1 for r in d.values() if r["ours_all"]["class"] == "D"),
              conteo["sp"], conteo["ga"]))

    # mapa de calor
    cols = ["ours_static", "ours_all", "gcc_stackprot", "gcc_asan_ubsan", "clang_asan_ubsan", "gcc_Wall"]
    etiq = ["propio\nestatico", "propio\n--sec=all", "gcc\nstack-prot", "gcc\nasan+ubsan", "clang\nasan+ubsan", "gcc\n-Wall"]
    fig, ax = plt.subplots(figsize=(6.5, 5.2))
    mat = []
    for nme, r in d.items():
        fila = []
        for c in cols:
            if c in ("ours_static", "gcc_Wall"):
                fila.append(2 if r[c]["detected"] else 0)
            else:
                fila.append({"D": 2, "C": 1}.get(r[c]["class"], 0))
        mat.append(fila)
    ax.imshow(mat, cmap="RdYlGn", vmin=0, vmax=2, aspect="auto")
    ax.set_xticks(range(len(cols)))
    ax.set_xticklabels(etiq, fontsize=7)
    ax.set_yticks(range(len(d)))
    ax.set_yticklabels([nombres.get(k[:3], k) for k in d], fontsize=7)
    ax.set_title("Deteccion (verde: diagnostico, amarillo: caida sin diagnostico, rojo: no detectado)", fontsize=7)
    fig.tight_layout()
    fig.savefig(os.path.join(FIG, "efectividad.png"), dpi=160)
    plt.close(fig)
    print("seguridad: tabla y figura generadas")


if __name__ == "__main__":
    rendimiento()
    seguridad()
