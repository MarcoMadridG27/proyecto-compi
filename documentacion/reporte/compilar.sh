#!/bin/sh
# Genera reporte.pdf con LaTeX dentro de Docker (no hace falta instalar TeX Live).
cd "$(dirname "$0")" || exit 1
docker build -q -f Dockerfile.latex -t mc-latex:latest . >/dev/null
docker run --rm -v "$PWD":/w -w /w mc-latex:latest sh -c 'pdflatex -interaction=nonstopmode reporte.tex >/dev/null && pdflatex -interaction=nonstopmode reporte.tex > reporte.log2; tail -3 reporte.log2'
