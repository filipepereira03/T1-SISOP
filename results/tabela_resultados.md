# Resultados e Desempenho - Trabalho Prático 1

## 1. Tabela 7.1 - Registro dos Resultados Obrigatórios

| Exemplo | Dimensões | Objetos Esperados | Sequencial | Paralelo (4 Threads) | Status |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **Exemplo 1** | 5 x 5 | **3** | 3 | 3 | **OK** |
| **Exemplo 2** | 6 x 8 | **4** | 4 | 4 | **OK** |
| **Exemplo 3** | 8 x 8 | **5** | 5 | 5 | **OK** |
| **Exemplo 4** | 9 x 12 | **6** | 6 | 6 | **OK** |
| **Exemplo 5** | 12 x 12 | **7** | 7 | 7 | **OK** |

> Todos os 5 casos de teste obtiveram 100% de correspondência exata entre o resultado esperado, a versão sequencial e a versão paralela.

## 2. Experimento de Desempenho em Matriz Grande (1000x1000)

- **Metodologia de Medição (Requisito 36):** Foram executadas **10 repetições** independentes para cada configuração. O valor representativo adotado foi a **média aritmética**, acompanhada do **desvio padrão amostral** para verificar a estabilidade das medições.
- **Semente e Densidade:** Semente `42`, densidade de 30% de células ativas (gerando aproximadamente 47.698 objetos conexos com diversas pontes nas fronteiras).

| Configuração | Tempo Médio (s) | Desvio Padrão (s) | Objetos Detectados | Aceleração (Speedup $S$) | Eficiência ($E$) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Sequencial** | 0.030362 s | ±0.008901 s | 47,698 | **1.00x** | 100.0% |
| **Paralelo (1T)** | 0.055006 s | ±0.018412 s | 47,698 | **0.55x** | 55.2% |
| **Paralelo (2T)** | 0.023131 s | ±0.003557 s | 47,698 | **1.31x** | 65.6% |
| **Paralelo (4T)** | 0.021102 s | ±0.008227 s | 47,698 | **1.44x** | 36.0% |
| **Paralelo (8T)** | 0.026908 s | ±0.007341 s | 47,698 | **1.13x** | 14.1% |

## 3. Análise da Versão Paralela em Matrizes Pequenas (Requisito 38)

- **Matriz Exemplo 1 (5x5, 25 células):**
  - Tempo Sequencial Médio: `1.00 µs` (0.000001 s)
  - Tempo Paralelo Médio (4 Threads): `1705.70 µs` (0.001706 s)
  - **Razão:** A versão paralela foi aproximadamente `1705.7x mais lenta` nessa matriz pequena.

### Explicação Teórica e Prática (Requisito 38):
1. **Sobrecanrga de Criação de Threads (*Overhead* do POSIX):** Criar threads via `pthread_create()` e sincronizá-las via `pthread_join()` exige chamadas de sistema no kernel do SO, alocação de estruturas de controle e pilhas de execução, consumido cerca de 50 a 500 microssegundos.
2. **Granularidade do Trabalho:** Em uma matriz 5x5, o algoritmo sequencial processa 25 células em apenas ~1 microssegundo. O custo de criar e coordenar as threads supera amplamente o tempo de processamento das células.
3. **Ponto de Inflexão (*Break-even Point*):** O paralelismo se torna vantajoso a partir do momento em que a carga de trabalho por thread compensa o custo de coordenação (em matrizes médias e grandes, a partir de centenas de linhas).
