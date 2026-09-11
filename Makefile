# ==============================================================================
# Makefile - Contagem Paralela de Objetos em Matriz Binaria
# Padrao: ANSI C (C89/C90), POSIX Threads
# ==============================================================================

CC ?= cc
CFLAGS ?= -std=c89 -Wall -Wextra -pedantic -pthread -O3
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L

SRC_DIR = src
DATA_DIR = data

TARGET_SEQ = sequencial
TARGET_PAR = paralelo

.PHONY: all sequencial paralelo test benchmark clean help

all: sequencial paralelo

sequencial: $(SRC_DIR)/conta-objetos-sequencial.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $(TARGET_SEQ)

paralelo: $(SRC_DIR)/conta-objetos-paralelo.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $(TARGET_PAR)

test: all
	@echo "=========================================================="
	@echo "           TESTES DA VERSAO SEQUENCIAL                   "
	@echo "=========================================================="
	./$(TARGET_SEQ) --test
	@echo ""
	@echo "=========================================================="
	@echo "           TESTES DA VERSAO PARALELA (1 THREAD)          "
	@echo "=========================================================="
	./$(TARGET_PAR) 1 --test
	@echo ""
	@echo "=========================================================="
	@echo "           TESTES DA VERSAO PARALELA (2 THREADS)         "
	@echo "=========================================================="
	./$(TARGET_PAR) 2 --test
	@echo ""
	@echo "=========================================================="
	@echo "           TESTES DA VERSAO PARALELA (4 THREADS)         "
	@echo "=========================================================="
	./$(TARGET_PAR) 4 --test
	@echo ""
	@echo "=========================================================="
	@echo "           TESTES COM ARQUIVOS EM data/                  "
	@echo "=========================================================="
	@for file in $(DATA_DIR)/*.txt; do \
		echo "--- Testando $$file ---"; \
		printf "Sequencial : "; ./$(TARGET_SEQ) "$$file"; \
		printf "Paralelo 4T: "; ./$(TARGET_PAR) 4 "$$file"; \
		echo ""; \
	done

benchmark: all
	@echo "=========================================================="
	@echo "      BENCHMARK DE DESEMPENHO (1000x1000, 30% DENSIDADE)  "
	@echo "=========================================================="
	@echo ">> 2 Threads:"
	./$(TARGET_PAR) 2 --benchmark 1000 1000 42
	@echo ">> 4 Threads:"
	./$(TARGET_PAR) 4 --benchmark 1000 1000 42
	@echo ">> 8 Threads:"
	./$(TARGET_PAR) 8 --benchmark 1000 1000 42

clean:
	rm -f $(TARGET_SEQ) $(TARGET_PAR) *.o

help:
	@echo "Opcoes do Makefile:"
	@echo "  make              - Compila ambas as versoes (sequencial e paralelo)"
	@echo "  make sequencial   - Compila apenas a versao sequencial"
	@echo "  make paralelo     - Compila apenas a versao paralela"
	@echo "  make test         - Executa todos os testes unitarios e arquivos de exemplo"
	@echo "  make benchmark    - Executa benchmark com matriz 1000x1000 variando threads"
	@echo "  make clean        - Remove os executaveis gerados"
