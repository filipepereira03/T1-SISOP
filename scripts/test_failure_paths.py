#!/usr/bin/env python3
"""Run injected failure cases with a timeout so deadlocks fail the suite."""

import os
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit("uso: test_failure_paths.py CAMINHO_BINARIO")
    binary = sys.argv[1]
    if os.name == "nt" and not binary.lower().endswith(".exe"):
        binary += ".exe"
    for mode, name in ((1, "alocacao no worker"),
                       (2, "criacao parcial de threads"),
                       (3, "alocacao da contagem final"),
                       (4, "erro em pthread_join")):
        try:
            result = subprocess.run([binary, str(mode)], timeout=5,
                                    capture_output=True, text=True)
        except subprocess.TimeoutExpired:
            raise SystemExit("FALHA: bloqueio em " + name)
        if result.returncode != 0:
            raise SystemExit("FALHA: " + name + "\n" + result.stderr)
        print("OK: " + name)


if __name__ == "__main__":
    main()
