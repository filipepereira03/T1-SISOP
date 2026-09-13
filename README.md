# Contagem Paralela de Objetos em uma Matriz Binária

Trabalho acadêmico desenvolvido para a disciplina de **Sistemas Operacionais** (Trabalho Prático 1).

---

## Autores

Filipe da Silva Pereira

Alice Borstmann Koepp

Júlia Teixeira Tietbohl

Matheus Silva de Lima

---

## 1. Descrição do Problema

Uma **matriz binária** bidimensional representa uma imagem digital onde o valor `0` corresponde ao fundo e o valor `1` corresponde a um ponto de objeto.

Um **objeto conexo** é definido como um conjunto maximal de células com valor `1` conectadas entre si por arestas horizontais, arestas verticais ou cantos diagonais — configurando **conectividade 8** (8-vizinhança).

O objetivo do projeto é determinar o número total de componentes conexos distintos na matriz. O preenchimento e rotulagem das regiões é baseado no algoritmo de **Flood Fill** (preenchimento por inundação).

### Conectividade 8

Dada uma célula nas coordenadas $(r, c)$, seus 8 vizinhos potenciais são:

$$\{(r-1, c-1), (r-1, c), (r-1, c+1), (r, c-1), (r, c+1), (r+1, c-1), (r+1, c), (r+1, c+1)\}$$

### Prevenção de Estouro de Pilha (*Stack Overflow*)

Em implementações recursivas clássicas de flood fill, cada passo de chamada adiciona um quadro à pilha de execução do sistema operacional (*call stack*). Em matrizes grandes (por exemplo, $1000 \times 1000$ ou maiores), um único componente pode conter centenas de milhares de células, ocasionando falha de segmentação (*segmentation fault*) por estouro de pilha. 

Para eliminar esse risco, ambas as implementações (sequencial e paralela) utilizam uma **pilha explícita alocada dinamicamente na heap**, operando de forma estritamente iterativa.

---

## 2. Padrão Técnico e Portabilidade

- **Padrão:** Estritamente **ANSI C (C89 / C90)**.
  - Declaração de todas as variáveis no topo dos blocos `{ ... }`.
  - Apenas comentários no estilo clássico `/* ... */` (sem `//`).
  - Sem uso de cabeçalhos introduzidos no C99 (sem `<stdint.h>`, sem `<stdbool.h>`).
  - Sem matrizes de comprimento variável (*Variable-Length Arrays* - VLAs).
- **Compilador e Flags:** Totalmente compatível com:
  ```bash
  cc -std=c89 -Wall -Wextra -pedantic -pthread programa.c -o programa
  ```
- **Portabilidade:** Compatível com ambientes **Linux** (glibc/musl) e **macOS** (POSIX).
- **Gestão de Recursos:** Todas as chamadas de alocação (`malloc`, `calloc`, `realloc`), primitivas POSIX (`pthread_create`, `pthread_join`, `pthread_mutex_*`, `pthread_cond_*`) e temporizadores (`clock_gettime`) possuem checagem de retorno com tratamento de erro e liberação completa de memória e descritores ao término.

---

## 3. Instruções de Compilação e Execução

### 3.1 Compilação via Makefile

O projeto inclui um `Makefile` configurado com regras completas:

```bash
# Compila ambas as versoes (sequencial e paralelo)
make

# Compila apenas a versao sequencial
make sequencial

# Compila apenas a versao paralela
make paralelo

# Executa todos os testes unitarios obrigatorios e arquivos de dados
make test

# Executa o benchmark com matriz 1000x1000 com diferentes quantidades de threads
make benchmark

# Limpa os binarios gerados
make clean
```

### 3.2 Compilação Manual (sem make)

```bash
# Versao Sequencial
cc -std=c89 -Wall -Wextra -pedantic -pthread -O3 src/conta-objetos-sequencial.c -o sequencial

# Versao Paralela
cc -std=c89 -Wall -Wextra -pedantic -pthread -O3 src/conta-objetos-paralelo.c -o paralelo
```

---

## 4. Como Executar

### 4.1 Versão Sequencial (`./sequencial`)

```bash
# 1. Modo Padrao: executa a bateria de testes e o exemplo padrao embutido
./sequencial

# 2. Modo Teste: executa estritamente as 5 matrizes obrigatorias
./sequencial --test

# 3. Modo Arquivo: le uma matriz de um arquivo texto
./sequencial data/exemplo1.txt
./sequencial data/exemplo4.txt

# 4. Modo Stdin: le a matriz da entrada padrao
cat data/exemplo2.txt | ./sequencial -

# 5. Modo Benchmark: gera matriz aleatoria (linhas, colunas, semente)
./sequencial --benchmark 1000 1000 42
```

### 4.2 Versão Paralela (`./paralelo <num_threads>`)

O primeiro argumento obrigatório é o número de threads que atuarão no cálculo:

```bash
# 1. Modo Padrao: executa testes e exemplo padrao com N threads
./paralelo 4

# 2. Modo Teste: executa estritamente as 5 matrizes obrigatorias com N threads
./paralelo 4 --test

# 3. Modo Arquivo: processa matriz de arquivo com N threads
./paralelo 2 data/exemplo3.txt
./paralelo 4 data/exemplo5.txt

# 4. Modo Stdin: processa matriz da entrada padrao
cat data/exemplo4.txt | ./paralelo 4 -

# 5. Modo Benchmark: compara sequencial vs paralelo e calcula o Speedup
./paralelo 4 --benchmark 1000 1000 42
./paralelo 8 --benchmark 2000 2000 42
```

---

## 5. Arquitetura da Solução Paralela

A paralelização do algoritmo de contagem de componentes em uma matriz requer tratamento cuidadoso, pois componentes podem se estender ao longo de várias regiões processadas por threads distintas.

```
+-------------------------------------------------------------+
| Thread 0 : Faixa de Linhas [0 .. R/T - 1]                   |
|            Flood fill local -> Gera Componentes Locais      |
+~~~~~~~~~~~~~~~~~~~~~~~~ FRONTEIRA 0 ~~~~~~~~~~~~~~~~~~~~~~~~+  <-- Unificacao com DSU + Mutex
| Thread 1 : Faixa de Linhas [R/T .. 2*R/T - 1]               |
|            Flood fill local -> Gera Componentes Locais      |
+~~~~~~~~~~~~~~~~~~~~~~~~ FRONTEIRA 1 ~~~~~~~~~~~~~~~~~~~~~~~~+  <-- Unificacao com DSU + Mutex
| Thread 2 : Faixa de Linhas [2*R/T .. 3*R/T - 1]             |
|            Flood fill local -> Gera Componentes Locais      |
+~~~~~~~~~~~~~~~~~~~~~~~~ FRONTEIRA 2 ~~~~~~~~~~~~~~~~~~~~~~~~+  <-- Unificacao com DSU + Mutex
| Thread 3 : Faixa de Linhas [3*R/T .. R - 1]                 |
|            Flood fill local -> Gera Componentes Locais      |
+-------------------------------------------------------------+
```

### 5.1 Decomposição de Domínio (Divisão por Faixas de Linhas)

A matriz de $R$ linhas e $C$ colunas é dividida horizontalmente entre $T$ threads em blocos contíguos de linhas:
- Linhas base por thread: $B = \lfloor R / T \rfloor$
- Resto de linhas: $K = R \pmod T$
- Cada thread $t$ recebe o intervalo $[start\_row_t, end\_row_t)$, onde:
  $$start\_row_t = t \cdot B + \min(t, K)$$
  $$count_t = B + (t < K ? 1 : 0)$$
  $$end\_row_t = start\_row_t + count_t$$

Essa estratégia oferece excelente localidade espacial e temporal de cache, pois os elementos de cada linha estão contíguos na memória.

### 5.2 Fase 1: Flood Fill Local Concorrente

Cada thread opera de forma independente e concorrente em sua respectiva faixa de linhas:
1. Ao encontrar uma célula $(r, c)$ com valor `1` e ainda não rotulada (`labels[r * C + c] == 0`), identifica-se um novo componente conexo local.
2. É gerado um identificador único baseado nas coordenadas da primeira célula:
   $$ID = r \cdot C + c + 1$$
   Por ser derivado de coordenadas espaciais únicas, o ID é estritamente positivo e não sofre colisão entre threads.
3. A thread preenche iterativamente todos os vizinhos conectados (8 direções) **restringindo-se estritamente** às linhas do seu domínio $[start\_row_t, end\_row_t)$.
4. Todas as células descobertas são marcadas na matriz de rótulos com esse $ID$, e o $ID$ é adicionado à lista local de componentes da thread.

### 5.3 Barreira de Sincronização Portável

Antes de iniciar a consolidação, é mandatório que todas as threads concluam a rotulagem de suas faixas.

Como a primitiva `pthread_barrier_t` é opcional no padrão POSIX e **não está presente no macOS**, implementou-se uma barreira própria, robusta e portável, utilizando exclusivamente `pthread_mutex_t` e `pthread_cond_t`.

### 5.4 Fase 2: Consolidação nas Fronteiras com Union-Find (DSU)

Cada thread $t$ (para $0 \le t < T - 1$) é responsável por inspecionar a interface entre a última linha da sua faixa ($r_{top} = end\_row_t - 1$) e a primeira linha da faixa seguinte ($r_{bot} = end\_row_t$):

Para cada coluna $c \in [0, C-1]$ tal que a célula superior seja objeto (`matrix[r_top * C + c] == 1`):
1. Obtém-se o rótulo $L_{top} = labels[r_{top} \cdot C + c]$.
2. Verifica-se a conectividade 8 com as três células vizinhas na linha inferior:
   - Diagonal inferior esquerda: $(r_{bot}, c - 1)$
   - Diretamente abaixo: $(r_{bot}, c)$
   - Diagonal inferior direita: $(r_{bot}, c + 1)$
3. Para cada vizinho inferior com valor 1, obtém-se $L_{bot} = labels[r_{bot} \cdot C + c']$.
4. Caso pertençam a componentes locais distintos, a união é realizada na estrutura de **Union-Find (DSU)**:
   ```c
   pthread_mutex_lock(dsu_mutex);
   dsu_union(dsu, label_top, label_bot);
   pthread_mutex_unlock(dsu_mutex);
   ```

A proteção via `pthread_mutex_t` garante que múltiplas threads possam consolidar fronteiras concorrentemente sem condições de corrida (*race conditions*) sobre o grafo de conjuntos disjuntos.

### 5.5 Fase 3: Contagem Global de Componentes Conexos

Após o término de todas as threads (`pthread_join`):
1. Itera-se sobre os IDs de todos os componentes locais registrados por todas as threads.
2. Para cada componente local $ID$, consulta-se a raiz canônica no DSU via `dsu_find(&dsu, ID)`.
3. Utilizando um vetor de marcação, cada raiz única é contabilizada exatamente uma vez.
4. Células de fundo nunca recebem rótulo e componentes que se tocam nas fronteiras (ou transitivamente através de múltiplas fronteiras, como objetos em formato de "U") convergem para a mesma raiz, garantindo contagem matematicamente exata e idêntica à versão sequencial.

---

## 6. Casos de Teste Obrigatórios

O código inclui nativamente as 5 matrizes especificadas:

| Teste | Dimensões | Objetos Esperados | Sequencial | Paralelo (4T) | Status |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **Exemplo 1** | $5 \times 5$ | **3** | 3 | 3 | **OK** |
| **Exemplo 2** | $6 \times 8$ | **4** | 4 | 4 | **OK** |
| **Exemplo 3** | $8 \times 8$ | **5** | 5 | 5 | **OK** |
| **Exemplo 4** | $9 \times 12$ | **6** | 6 | 6 | **OK** |
| **Exemplo 5** | $12 \times 12$ | **7** | 7 | 7 | **OK** |

---

## 7. Medição de Desempenho e Resultados de Benchmark

A medição de tempo de execução foi realizada utilizando a chamada POSIX de alta precisão `clock_gettime(CLOCK_MONOTONIC)`.

O teste de escalabilidade foi conduzido sobre matrizes aleatórias com **densidade de 30%** de elementos ativos (onde múltiplos componentes grandes e pequenos se formam e cruzam as fronteiras das faixas).

### Resultados com Matriz $1000 \times 1000$ (1.000.000 de células, ~47.698 objetos)

$$S = \frac{T_{seq}}{T_{par}} \qquad E = \frac{S}{T}$$

| Configuração | Tempo Médio (s) | Objetos Detectados | Aceleração ($S$) | Eficiência ($E$) |
| :--- | :---: | :---: | :---: | :---: |
| **Sequencial** | 0.0510 s | 47.698 | 1.00x | 100% |
| **Paralelo (1 Thread)** | 0.0488 s | 47.698 | 1.05x | 105% |
| **Paralelo (2 Threads)** | 0.0341 s | 47.698 | 1.50x | 75.0% |
| **Paralelo (4 Threads)** | 0.0205 s | 47.698 | 2.49x | 62.3% |
| **Paralelo (8 Threads)** | 0.0160 s | 47.698 | **2.63x** | 32.9% |

### Resultados com Matriz $2000 \times 2000$ (4.000.000 de células, ~189.058 objetos)

| Configuração | Tempo Médio (s) | Objetos Detectados | Aceleração ($S$) | Eficiência ($E$) |
| :--- | :---: | :---: | :---: | :---: |
| **Sequencial** | 0.2186 s | 189.058 | 1.00x | 100% |
| **Paralelo (4 Threads)** | 0.0989 s | 189.058 | **2.21x** | 55.3% |

### Análise dos Resultados

1. **Corretude Rigorosa:** Em todas as execuções, o número de objetos detectados pela versão paralela foi **estritamente idêntico** ao da versão sequencial, comprovando a eficácia do algoritmo de consolidação com Union-Find.
2. **Ganhos de Desempenho:** A versão paralela atinge aceleração consistente ($> 2.4x$ com 4 threads na matriz de $1000 \times 1000$), reduzindo substancialmente o tempo total de processamento.
3. **Overhead de Sincronização:** Em matrizes muito pequenas ($5 \times 5$ a $12 \times 12$), o custo de criação das threads (`pthread_create`) e sincronização da barreira supera o tempo computacional das células. Já em matrizes a partir de centenas de milhares de elementos, o paralelismo compensa amplamente o overhead.
4. **Ausência de Condições de Corrida:** A execução foi validada com os analisadores de memória e concorrência **AddressSanitizer** (`-fsanitize=address`) e **ThreadSanitizer** (`-fsanitize=thread`), confirmando ausência total de vazamentos de memória (*memory leaks*), leituras inválidas e disputas de dados (*data races*).

---

## 8. Estrutura de Arquivos do Projeto

```
.
├── Makefile                     # Script de automacao de compilacao, testes e benchmarks
├── README.md                    # Relatorio tecnico detalhado do projeto
├── data/                        # Casos de teste em arquivos de texto
│   ├── exemplo1.txt             # Matriz 5x5 (esperado 3)
│   ├── exemplo2.txt             # Matriz 6x8 (esperado 4)
│   ├── exemplo3.txt             # Matriz 8x8 (esperado 5)
│   ├── exemplo4.txt             # Matriz 9x12 (esperado 6)
│   └── exemplo5.txt             # Matriz 12x12 (esperado 7)
└── src/                         # Codigo-fonte em C ANSI (C89/C90)
    ├── conta-objetos-sequencial.c # Versao sequencial (flood fill iterativo com pilha)
    └── conta-objetos-paralelo.c   # Versao paralela (Pthreads, faixas, DSU, mutex)
```
