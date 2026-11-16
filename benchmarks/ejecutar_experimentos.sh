#!/bin/sh
# Ejecuta los experimentos dentro de un contenedor linux/amd64 (gcc 13 + clang 14).
# Uso: benchmarks/ejecutar_experimentos.sh [rendimiento|seguridad|todo] [ejecuciones]
cd "$(dirname "$0")/.." || exit 1
PARTE=${1:-todo}
RUNS=${2:-10}
docker build --platform linux/amd64 -q -f benchmarks/Dockerfile.bench -t mc-bench:latest benchmarks >/dev/null
docker run --rm --platform linux/amd64 -v "$PWD":/w -w /w mc-bench:latest \
  python3 benchmarks/experimento.py --parte "$PARTE" --runs "$RUNS"
