#!/bin/sh
# Script alternativo de compilacao direta para ambientes sem o utilitario make
set -e

CC="${CC:-cc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    if command -v gcc >/dev/null 2>&1; then
        CC="gcc"
    elif command -v clang >/dev/null 2>&1; then
        CC="clang"
    else
        echo "Erro: Nenhum compilador C encontrado (cc, gcc ou clang)." >&2
        exit 1
    fi
fi

CFLAGS="-std=c89 -Wall -Wextra -pedantic -pthread -O3 -D_POSIX_C_SOURCE=200809L"

echo "Compilando com $CC..."
$CC $CFLAGS src/conta-objetos-sequencial.c -o sequencial
echo "-> sequencial compilado com sucesso!"

$CC $CFLAGS src/conta-objetos-paralelo.c -o paralelo
echo "-> paralelo compilado com sucesso!"
