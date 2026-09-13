# Contagem Paralela de Objetos em uma Matriz Binária

**Pontifícia Universidade Católica do Rio Grande do Sul (PUCRS)**  
**Escola Politécnica — Sistemas Operacionais (2026/II)**  
**Professor:** Filipo Mór ([www.filipomor.com](https://www.filipomor.com))  
**Trabalho Prático 1:** Processos e Threads POSIX

---

## Autores

- Filipe da Silva Pereira
- Júlia Teixeira Tietbohl 
- Alice Borstmann Koepp
- Matheus Silva de Lima
---

## 1. Contextualização e Objetivos de Aprendizagem

Uma matriz binária bidimensional representa uma imagem digital na qual o valor `0` indica o fundo e o valor `1` indica o primeiro plano. Um **objeto** é definido como uma região maximal de células com valor `1` conectadas por arestas horizontais, verticais ou cantos diagonais — regra denominada **conectividade 8 (8-vizinhança)**.

O objetivo do trabalho é implementar e comparar duas soluções completas para a contagem exata de objetos conexos:
1. **Versão Sequencial:** Baseada no algoritmo iterativo de preenchimento por inundação (*Flood Fill*) com pilha explícita na *heap*, servindo como referência de correção (*ground truth*) e desempenho.
2. **Versão Paralela:** Implementada com **Pthreads**, utilizando decomposição de domínio em faixas de linhas (*1D Row-Striping*), sincronização por barreira portável, unificação de fronteiras com **Union-Find (DSU)** e exclusão mútua via **Mutexes POSIX**.

### Objetivos Alcançados (Seção 2 do Enunciado):
- Distinção clara entre execução sequencial e paralela.
- Criação, sincronização e finalização correta de threads POSIX (`pthread_create`, `pthread_join`, `pthread_mutex_*`, `pthread_cond_*`).
- Decomposição balanceada de trabalho com preservação de conectividade 8.
- Reconhecimento de regiões críticas e eliminação completa de condições de corrida (*race conditions*) e *deadlocks*.
- Consolidação global determinística com Disjoint Set Union (DSU).
- Avaliação rigorosa de sobrecarga (*overhead*), escalabilidade e aceleração ($S = T_{seq} / T_{par}$).

---

## 2. Requisitos Técnicos e Portabilidade

- **Padrão:** Estritamente **ANSI C (padrão C89 / C90)**.
  - Todas as variáveis declaradas impreterivelmente no topo dos blocos `{ ... }`.
  - Comentários exclusivamente no formato clássico `/* ... */`.
  - Ausência de recursos C99/C11 (sem `<stdint.h>`, `<stdbool.h>`, VLAs ou declarações em loops `for`).
- **Compilação de Referência:**
  ```bash
  cc -std=c89 -Wall -Wextra -pedantic -pthread programa.c -o programa
  ```
  Compilação limpa, sem erros e sem avisos (*warnings*).
- **Portabilidade:** Executa perfeitamente em sistemas operacionais **Linux** e **macOS** (compatibilidade total com as especificações POSIX.1-2008).
- **Tratamento de Erros e Liberação de Recursos:**
  - Verificação de retorno de todas as chamadas POSIX (`pthread_*`, `clock_gettime`) e chamadas de sistema/biblioteca padrão (`malloc`, `calloc`, `fopen`, `fclose`).
  - Liberação integral de recursos de memória (*free*), descritores de arquivo e destruição de mutexes e variáveis de condição ao término.

---

## 3. Estrutura do Repositório

O projeto segue rigorosamente a estrutura de diretórios e arquivos sugerida na página 8 do enunciado:

```
T1-SISOP/
├── README.md                      # Relatório técnico completo e documentação
├── Makefile                       # Automação de compilação, testes e relatórios
├── build.sh                       # Script auxiliar de compilação sem dependência do make
├── visualizador.html              # Editor gráfico interativo em HTML/Tailwind para desenho livre
├── src/
│   ├── conta-objetos-sequencial.c # Implementação sequencial (Flood Fill com pilha explícita)
│   └── conta-objetos-paralelo.c   # Implementação paralela (Pthreads, faixas de linhas, DSU)
├── tests/                         # Arquivos das matrizes de teste obrigatórias
│   ├── exemplo1.txt               # 5 x 5   (esperado: 3 objetos)
│   ├── exemplo2.txt               # 6 x 8   (esperado: 4 objetos)
│   ├── exemplo3.txt               # 8 x 8   (esperado: 5 objetos)
│   ├── exemplo4.txt               # 9 x 12  (esperado: 6 objetos)
│   └── exemplo5.txt               # 12 x 12 (esperado: 7 objetos)
├── results/                       # Registro de resultados e medições de desempenho
│   ├── tabela_resultados.md       # Tabela 7.1 e análise comparativa
│   └── benchmark_repetido.txt     # Log bruto das medições repetidas (10 rodadas)
├── scripts/
│   └── benchmark_runner.py        # Automatizador de testes estatísticos e benchmarks
└── slides/
    ├── apresentacao.html          # Código-fonte da apresentação de 10 minutos
    └── apresentacao.pdf           # Slides da apresentação em PDF (Requisito 50)
```

---

## 4. Instruções de Compilação e Execução

### 4.1 Compilação via Makefile

```bash
# Compila ambas as versoes (sequencial e paralelo) com otimizacao -O3
make

# Compila apenas a versao sequencial
make sequencial

# Compila apenas a versao paralela
make paralelo

# Executa todos os testes unitarios e as matrizes em tests/
make test

# Executa benchmark interativo com matriz 1000x1000 variando threads
make benchmark

# Executa bateria de 10 rodadas repetidas e atualiza results/
make results

# Limpa os binarios gerados
make clean
```

### 4.2 Execução

#### Versão Sequencial (`./sequencial`)
```bash
# Bateria de testes embutida e exemplo padrao
./sequencial

# Apenas a bateria de testes oficiais
./sequencial --test

# Executar arquivo especifico
./sequencial tests/exemplo2.txt

# Modo Visual no Terminal (matriz colorida com identificacao dos objetos):
./sequencial --visual tests/exemplo2.txt

# Modo Benchmark com matriz aleatoria (linhas, colunas, semente)
./sequencial --benchmark 1000 1000 42
```

#### Versão Paralela (`./paralelo <num_threads>`)
```bash
# Bateria de testes oficial com N threads
./paralelo 4 --test

# Executar arquivo especifico com 4 threads
./paralelo 4 tests/exemplo5.txt

# Modo Visual no Terminal (matriz colorida com linhas de corte entre faixas de threads):
./paralelo 3 --visual tests/exemplo5.txt

# Modo Benchmark (compara sequencial vs paralelo e calcula Speedup):
./paralelo 4 --benchmark 1000 1000 42
```

---

## 5. Arquitetura da Solução e Estratégia de Consolidação

```
┌─────────────────────────────────────────────────────────────┐
│ Thread 0 : Faixa de Linhas [0 .. end_row_0 - 1]             │
│            Flood Fill Local -> Identifica Componentes       │
├┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ FRONTEIRA 0 ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┤ <─ Unificação Concorrente com DSU + Mutex
│ Thread 1 : Faixa de Linhas [start_row_1 .. end_row_1 - 1]   │
│            Flood Fill Local -> Identifica Componentes       │
├┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ FRONTEIRA 1 ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┤ <─ Unificação Concorrente com DSU + Mutex
│ Thread 2 : Faixa de Linhas [start_row_2 .. R - 1]           │
│            Flood Fill Local -> Identifica Componentes       │
└─────────────────────────────────────────────────────────────┘
```

### 5.1 Decomposição do Trabalho (Divisão em Faixas de Linhas)
A matriz é dividida horizontalmente entre $T$ threads em fatias contíguas:
- `base_rows = rows / num_threads`
- `remainder = rows % num_threads`
- A thread $t$ recebe o intervalo $[start\_row_t, end\_row_t)$, onde as primeiras `remainder` threads recebem 1 linha extra para balancear a carga.

**Justificativa Técnica da Decomposição (Item 24 e 33):**
1. **Localidade Espacial de Cache (*Row-Major Order*):** Em C, as linhas de uma matriz são alocadas contiguamente na memória física. Cada thread acessa endereços adjacentes de memória RAM, maximizando o reuso de linhas da cache L1/L2/L3 e eliminando o falso compartilhamento (*false sharing*).
2. **Minimização de Fronteiras:** A decomposição 1D em faixas de linhas gera exatamente $T - 1$ linhas de interface horizontal, simplificando a consolidação e reduzindo a concorrência sobre a estrutura de união.

### 5.2 Identificação de Componentes Locais (Item 29)
Na Fase 1, cada thread varre exclusivamente a sua faixa. Quando encontra uma célula `1` ainda não visitada, cria um componente local.
Para evitar qualquer necessidade de comunicação, lock ou contador atômico entre threads, o identificador do componente é derivado da coordenada de sua primeira célula:
$$ID = r \cdot C + c + 1$$
Como cada coordenada $(r, c)$ é única em toda a matriz, o $ID$ gerado é **estritamente positivo e globalmente único**, livre de colisões entre threads.

### 5.3 Barreira de Sincronização Portável
Para garantir que nenhuma thread acesse a fronteira antes que as faixas vizinhas tenham terminado a rotulagem local, é necessária uma barreira de sincronização. Como a função `pthread_barrier_t` é opcional e ausente no macOS, foi implementada uma **barreira de sincronização própria e portável** utilizando `pthread_mutex_t` e `pthread_cond_t`.

### 5.4 Consolidação nas Fronteiras com Union-Find (DSU) (Itens 25, 26, 30, 31 e 32)
Na Fase 2, cada thread $t$ ($0 \le t < T - 1$) inspeciona a linha de fronteira entre a última linha da sua faixa ($r_{top} = end\_row_t - 1$) e a primeira linha da faixa seguinte ($r_{bot} = end\_row_t$):
- Para cada coluna $c$ tal que a célula superior seja objeto (`matrix[r_top * C + c] == 1`), com rótulo $L_{top}$:
  - Verifica os 3 vizinhos inferiores na conectividade 8:
    - Diagonal inferior-esquerda: $(r_{bot}, c - 1)$
    - Diretamente abaixo: $(r_{bot}, c)$
    - Diagonal inferior-direita: $(r_{bot}, c + 1)$
  - Se algum vizinho inferior tiver valor `1` e rótulo $L_{bot}$, a união é realizada no **Disjoint Set Union (DSU)** com proteção de mutex:
    ```c
    pthread_mutex_lock(&dsu_mutex);
    dsu_union(&dsu, label_top, label_bot);
    pthread_mutex_unlock(&dsu_mutex);
    ```

**Tratamento de Conexões Diagonais e Múltiplos Blocos (Item 31):**
Graças à checagem das diagonais $(c - 1)$ e $(c + 1)$ nas fronteiras e à propriedade de **transitividade do DSU**, componentes com formatos complexos (como "U", ferraduras, serpentinas ou nós que tocam 4 blocos) são unificados automaticamente na mesma raiz canônica.

### 5.5 Trechos Paralelos vs Trechos Sequenciais (Item 33)
- **Trechos Paralelos:**
  - Varredura e rotulagem local por Flood Fill (100% independente, sem locks).
  - Inspeção concorrente das $T - 1$ fronteiras pelas threads.
- **Trechos com Região Crítica (Sincronizados):**
  - Invocação de `dsu_union()` protegida pelo mutex `dsu_mutex`.
- **Trechos Sequenciais:**
  - Leitura e alocação inicial da matriz na memória principal.
  - Criação e disparo das threads (`pthread_create`) e junção final (`pthread_join`).
  - Contagem final de raízes canônicas distintas no DSU (operação linear rápida após o término das threads).

---

## 6. Resultados das Matrizes Obrigatórias (Tabela 7.1)

A tabela a seguir consolida a execução oficial realizada com os arquivos de `tests/`:

| Exemplo | Dimensões | Objetos Esperados | Sequencial | Paralelo (4 Threads) | Status | Características Verificadas |
| :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **Exemplo 1** | $5 \times 5$ | **3** | 3 | 3 | **OK** | Identificação básica de componentes compactos |
| **Exemplo 2** | $6 \times 8$ | **4** | 4 | 4 | **OK** | Objeto central atravessando fronteiras horizontais |
| **Exemplo 3** | $8 \times 8$ | **5** | 5 | 5 | **OK** | Encontro de blocos e conexões diagonais |
| **Exemplo 4** | $9 \times 12$ | **6** | 6 | 6 | **OK** | Objetos irregulares estendendo-se por múltiplos blocos |
| **Exemplo 5** | $12 \times 12$ | **7** | 7 | 7 | **OK** | Travessia diagonal longa ao longo de 3 faixas consecutivas |

> Em todos os casos, a versão paralela produziu **exatamente o mesmo resultado** da versão sequencial.

---

## 7. Análise de Desempenho e Experimentos Adicionais

### 7.1 Medições Repetidas em Matriz Grande ($1000 \times 1000$)
Para cumprir rigorosamente o **Requisito 36**, foram executadas **10 repetições independentes** para cada configuração sobre uma matriz de $1000 \times 1000$ (1 milhão de células, densidade de 30%, contendo 47.698 objetos conexos).

O valor representativo adotado foi a **média aritmética**, acompanhada do **desvio padrão amostral**:

$$S = \frac{T_{sequencial}}{T_{paralelo}} \qquad E = \frac{S}{T} \times 100\%$$

| Configuração | Tempo Médio (s) | Desvio Padrão (s) | Objetos Detectados | Aceleração ($S$) | Eficiência ($E$) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Sequencial** | 0.030362 s | ±0.008901 s | 47.698 | **1.00x** | 100.0% |
| **Paralelo (1 Thread)** | 0.055006 s | ±0.018412 s | 47.698 | **0.55x** | 55.2% |
| **Paralelo (2 Threads)** | 0.023131 s | ±0.003557 s | 47.698 | **1.31x** | 65.6% |
| **Paralelo (4 Threads)** | 0.021102 s | ±0.008227 s | 47.698 | **1.44x** | 36.0% |
| **Paralelo (8 Threads)** | 0.026908 s | ±0.007341 s | 47.698 | **1.13x** | 14.1% |

### 7.2 Análise do Requisito 38: Por que a versão paralela é mais lenta em certos cenários?

O enunciado solicita explicitamente: *"38. Explicar resultados nos quais a versão paralela seja mais lenta."*

No experimento com a matriz **Exemplo 1 ($5 \times 5$, 25 células)**:
- **Tempo Sequencial Médio:** `1.00 µs` ($0.000001\text{ s}$)
- **Tempo Paralelo Médio (4 Threads):** `1705.70 µs` ($0.001706\text{ s}$)
- **Resultado:** A versão paralela foi aproximadamente **1700 vezes mais lenta** que a sequencial.

**Causas do Comportamento:**
1. **Sobrecarga de Criação e Destruição de Threads (*POSIX Overhead*):** Invocar `pthread_create()`, alocar a pilha de execução de cada thread no kernel e realizar a junção com `pthread_join()` demanda dezenas a centenas de microssegundos.
2. **Granularidade do Trabalho:** Em matrizes pequenas ($5 \times 5$ a $12 \times 12$), a quantidade de operações aritméticas é ínfima. O tempo gasto criando e coordenando as threads é ordens de grandeza superior ao tempo de processar as células.
3. **Contenção e Barreira:** A sincronização via condição de barreira e mutex introduz trocas de contexto (*context switches*) que custam caro quando a tarefa útil por worker é desprezível.
4. **Ponto de Inflexão (*Break-even Point*):** A versão paralela torna-se vantajosa em matrizes médias e grandes (a partir de $500 \times 500$ células), onde o volume de trabalho em paralelo supera os custos fixos de criação das threads.

---

## 8. Apresentação em Aula (10 Minutos) e Slides em PDF

Os slides completos para a apresentação obrigatória foram gerados e armazenados em [`slides/apresentacao.pdf`](file:///c:/Users/F/T1-SISOP/slides/apresentacao.pdf) (Requisito 50), organizados conforme a distribuição sugerida na Seção 11:

| Minuto | Tema do Slide | Foco da Apresentação |
| :---: | :--- | :--- |
| **1 min** | **Problema e Estratégia Escolhida** | Definição da matriz binária, regra de conectividade 8 e o desafio da divisão concorrente. |
| **2 min** | **Implementação Sequencial** | Flood fill iterativo com pilha explícita na *heap*, eliminação de estouro de pilha e padrão ouro. |
| **2 min** | **Decomposição e Threads** | Divisão por faixas de linhas, localidade espacial de cache e IDs locais calculados sem locks. |
| **2 min** | **Consolidação e Demonstração** | Barreira portável, unificação de fronteiras com Union-Find (DSU) protegido por mutex e transitividade. |
| **2 min** | **Testes e Desempenho** | Tabela 7.1, medições repetidas (10 rodadas), aceleração e a explicação do Requisito 38. |
| **1 min** | **Conclusões** | Demonstração no terminal com `--visual` e encerramento. |

---

## 9. Referências Bibliográficas e Ferramentas

Em cumprimento ao item 13 do enunciado, listam-se as referências e ferramentas utilizadas:
1. **Padrão ANSI C (C89/C90):** ISO/IEC 9899:1990 — *Programming Languages — C*.
2. **Padrão POSIX Threads:** IEEE Std 1003.1-2008 — *Standard for Information Technology — Portable Operating System Interface (POSIX)*.
3. **Estrutura de Conjuntos Disjuntos (DSU / Union-Find):** Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. *Algoritmos: Teoria e Prática*. 3ª edição.
4. **Editor de Tabelas C:** Mór, Filipo. *Editor de Tabelas C para inicialização de matrizes* ([https://filipomor.com/editor-tabelas-c](https://filipomor.com/editor-tabelas-c)).
5. **Ferramentas de Verificação:** GCC 16, AddressSanitizer (`-fsanitize=address`) e ThreadSanitizer (`-fsanitize=thread`).
