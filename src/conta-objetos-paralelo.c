/*
 * ============================================================================
 * Arquivo: conta-objetos-paralelo.c
 * Descricao: Contagem paralela de objetos em uma matriz binaria usando
 *            Pthreads, decomposicao por faixas de linhas, flood fill local
 *            com conectividade 8 e consolidacao com Union-Find (DSU).
 * Padrao: ANSI C (C89 / C90) estrito e POSIX Threads.
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#define TRUE 1
#define FALSE 0

/* Estrutura para representar coordenadas na matriz */
typedef struct {
    int r;
    int c;
} Point;

/* Pilha dinamica explicita para evitar recursao */
typedef struct {
    Point *data;
    size_t top;
    size_t capacity;
} Stack;

static int stack_init(Stack *s, size_t initial_cap)
{
    s->data = (Point *)malloc(initial_cap * sizeof(Point));
    if (s->data == NULL) {
        return -1;
    }
    s->top = 0;
    s->capacity = initial_cap;
    return 0;
}

static int stack_push(Stack *s, int r, int c)
{
    if (s->top >= s->capacity) {
        size_t new_cap = s->capacity * 2;
        Point *new_data;
        if (new_cap < 16) {
            new_cap = 16;
        }
        new_data = (Point *)realloc(s->data, new_cap * sizeof(Point));
        if (new_data == NULL) {
            return -1;
        }
        s->data = new_data;
        s->capacity = new_cap;
    }
    s->data[s->top].r = r;
    s->data[s->top].c = c;
    s->top++;
    return 0;
}

static Point stack_pop(Stack *s)
{
    s->top--;
    return s->data[s->top];
}

static int stack_is_empty(const Stack *s)
{
    return (s->top == 0);
}

static void stack_free(Stack *s)
{
    if (s->data != NULL) {
        free(s->data);
        s->data = NULL;
    }
    s->top = 0;
    s->capacity = 0;
}

/* ============================================================================
 * ESTRUTURA UNION-FIND (DISJOINT SET UNION - DSU)
 * ============================================================================
 */
typedef struct {
    int *parent;
    int *rank;
    int max_elements;
} DSU;

static int dsu_init(DSU *d, int max_elements)
{
    int i;
    d->parent = (int *)malloc(((size_t)max_elements + 1) * sizeof(int));
    if (d->parent == NULL) {
        return -1;
    }
    d->rank = (int *)malloc(((size_t)max_elements + 1) * sizeof(int));
    if (d->rank == NULL) {
        free(d->parent);
        d->parent = NULL;
        return -1;
    }
    d->max_elements = max_elements;

    for (i = 0; i <= max_elements; i++) {
        d->parent[i] = i;
        d->rank[i] = 0;
    }
    return 0;
}

static int dsu_find(DSU *d, int i)
{
    int root = i;
    int curr = i;

    /* Encontra a raiz */
    while (root != d->parent[root]) {
        root = d->parent[root];
    }
    /* Compressao de caminhos */
    while (curr != root) {
        int next = d->parent[curr];
        d->parent[curr] = root;
        curr = next;
    }
    return root;
}

static int dsu_union(DSU *d, int i, int j)
{
    int root_i = dsu_find(d, i);
    int root_j = dsu_find(d, j);

    if (root_i != root_j) {
        if (d->rank[root_i] < d->rank[root_j]) {
            d->parent[root_i] = root_j;
        } else if (d->rank[root_i] > d->rank[root_j]) {
            d->parent[root_j] = root_i;
        } else {
            d->parent[root_j] = root_i;
            d->rank[root_i]++;
        }
        return 1; /* Uniao realizada */
    }
    return 0; /* Ja pertenciam ao mesmo conjunto */
}

static void dsu_free(DSU *d)
{
    if (d->parent != NULL) {
        free(d->parent);
        d->parent = NULL;
    }
    if (d->rank != NULL) {
        free(d->rank);
        d->rank = NULL;
    }
    d->max_elements = 0;
}

/* ============================================================================
 * BARREIRA DE SINCRONIZACAO PORTAVEL (COMPATIVEL COM LINUX E MACOS)
 * ============================================================================
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int trip_count;
    int cycle;
} Barrier;

static int barrier_init(Barrier *b, int count)
{
    if (pthread_mutex_init(&b->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&b->cond, NULL) != 0) {
        pthread_mutex_destroy(&b->mutex);
        return -1;
    }
    b->count = 0;
    b->trip_count = count;
    b->cycle = 0;
    return 0;
}

static int barrier_wait(Barrier *b)
{
    int my_cycle;

    if (pthread_mutex_lock(&b->mutex) != 0) {
        return -1;
    }
    my_cycle = b->cycle;
    b->count++;

    if (b->count == b->trip_count) {
        b->cycle++;
        b->count = 0;
        if (pthread_cond_broadcast(&b->cond) != 0) {
            pthread_mutex_unlock(&b->mutex);
            return -1;
        }
    } else {
        while (my_cycle == b->cycle) {
            if (pthread_cond_wait(&b->cond, &b->mutex) != 0) {
                pthread_mutex_unlock(&b->mutex);
                return -1;
            }
        }
    }

    if (pthread_mutex_unlock(&b->mutex) != 0) {
        return -1;
    }
    return 0;
}

static void barrier_destroy(Barrier *b)
{
    pthread_cond_destroy(&b->cond);
    pthread_mutex_destroy(&b->mutex);
}

/* ============================================================================
 * ESTRUTURA DE DADOS DAS THREADS WORKERS
 * ============================================================================
 */
typedef struct {
    int thread_id;
    int num_threads;
    int rows;
    int cols;
    int start_row;
    int end_row;

    const int *matrix;
    int *labels;

    /* Componentes locais descobertos por esta thread */
    int *local_comps;
    size_t num_local_comps;
    size_t cap_local_comps;

    DSU *dsu;
    pthread_mutex_t *dsu_mutex;
    Barrier *barrier;
} ThreadData;

/* Adiciona um ID de componente local descoberto pela thread */
static int thread_add_local_comp(ThreadData *td, int comp_id)
{
    if (td->num_local_comps >= td->cap_local_comps) {
        size_t new_cap = td->cap_local_comps * 2;
        int *new_arr;
        if (new_cap < 32) {
            new_cap = 32;
        }
        new_arr = (int *)realloc(td->local_comps, new_cap * sizeof(int));
        if (new_arr == NULL) {
            return -1;
        }
        td->local_comps = new_arr;
        td->cap_local_comps = new_cap;
    }
    td->local_comps[td->num_local_comps++] = comp_id;
    return 0;
}

/*
 * Funcao executada por cada worker thread:
 * Fase 1: Flood fill local estritamente dentro da sua faixa de linhas [start_row, end_row)
 * Sincronizacao: Barreira
 * Fase 2: Consolidacao de bordas com a faixa seguinte [end_row - 1] <-> [end_row]
 */
static void *worker_func(void *arg)
{
    ThreadData *td = (ThreadData *)arg;
    Stack stk;
    int r, c, k;
    static const int dr[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    static const int dc[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

    if (stack_init(&stk, 512) != 0) {
        fprintf(stderr, "Thread %d: Erro ao inicializar pilha\n", td->thread_id);
        pthread_exit(NULL);
    }

    /*
     * FASE 1: Flood Fill Local
     * Percorre apenas as linhas atribuidas a esta thread: [td->start_row, td->end_row)
     */
    for (r = td->start_row; r < td->end_row; r++) {
        for (c = 0; c < td->cols; c++) {
            int idx = r * td->cols + c;

            if (td->matrix[idx] == 1 && td->labels[idx] == 0) {
                /*
                 * Atribui ID unico global para o componente local baseado na posicao inicial:
                 * comp_id = r * cols + c + 1 (estritamente positivo e unico).
                 */
                int comp_id = r * td->cols + c + 1;

                if (thread_add_local_comp(td, comp_id) != 0) {
                    fprintf(stderr, "Thread %d: Erro de memoria ao registrar componente\n",
                            td->thread_id);
                    stack_free(&stk);
                    pthread_exit(NULL);
                }

                td->labels[idx] = comp_id;
                if (stack_push(&stk, r, c) != 0) {
                    fprintf(stderr, "Thread %d: Erro ao empilhar\n", td->thread_id);
                    stack_free(&stk);
                    pthread_exit(NULL);
                }

                while (!stack_is_empty(&stk)) {
                    Point curr = stack_pop(&stk);

                    for (k = 0; k < 8; k++) {
                        int nr = curr.r + dr[k];
                        int nc = curr.c + dc[k];

                        /* Conectividade 8 contida estritamente dentro da faixa da thread */
                        if (nr >= td->start_row && nr < td->end_row &&
                            nc >= 0 && nc < td->cols) {
                            int nidx = nr * td->cols + nc;
                            if (td->matrix[nidx] == 1 && td->labels[nidx] == 0) {
                                td->labels[nidx] = comp_id;
                                if (stack_push(&stk, nr, nc) != 0) {
                                    fprintf(stderr, "Thread %d: Erro ao empilhar vizinho\n",
                                            td->thread_id);
                                    stack_free(&stk);
                                    pthread_exit(NULL);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    stack_free(&stk);

    /*
     * Sincronizacao entre threads: todas devem concluir a rotulagem local
     * antes de qualquer thread inspecionar as fronteiras vizinhas.
     */
    if (barrier_wait(td->barrier) != 0) {
        fprintf(stderr, "Thread %d: Falha na barreira de sincronizacao\n", td->thread_id);
        pthread_exit(NULL);
    }

    /*
     * FASE 2: Consolidacao de Fronteiras
     * Cada thread 't' (exceto a ultima) verifica a fronteira entre sua ultima linha (r_top)
     * e a primeira linha da proxima thread (r_bot = r_top + 1).
     */
    if (td->thread_id < td->num_threads - 1) {
        int r_top = td->end_row - 1;
        int r_bot = td->end_row;

        /* Verifica se ambas as faixas tem linhas validas */
        if (td->end_row > td->start_row && r_bot < td->rows) {
            for (c = 0; c < td->cols; c++) {
                int top_idx = r_top * td->cols + c;

                if (td->matrix[top_idx] == 1) {
                    int label_top = td->labels[top_idx];

                    /*
                     * Checa conectividade 8 com os 3 vizinhos da linha inferior:
                     * 1) Diagonal inferior esquerda: c - 1
                     * 2) Diretamente abaixo: c
                     * 3) Diagonal inferior direita: c + 1
                     */
                    int offset_c;
                    for (offset_c = -1; offset_c <= 1; offset_c++) {
                        int bot_c = c + offset_c;
                        if (bot_c >= 0 && bot_c < td->cols) {
                            int bot_idx = r_bot * td->cols + bot_c;
                            if (td->matrix[bot_idx] == 1) {
                                int label_bot = td->labels[bot_idx];

                                /* Protege operacao de uniao no DSU compartilhado via mutex */
                                if (pthread_mutex_lock(td->dsu_mutex) != 0) {
                                    perror("pthread_mutex_lock");
                                    pthread_exit(NULL);
                                }

                                dsu_union(td->dsu, label_top, label_bot);

                                if (pthread_mutex_unlock(td->dsu_mutex) != 0) {
                                    perror("pthread_mutex_unlock");
                                    pthread_exit(NULL);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    pthread_exit(NULL);
    return NULL;
}

/*
 * Algoritmo sequencial de referencia para comparacao e calculo de speedup
 */
static int count_objects_seq_ref(const int *matrix, int rows, int cols, double *out_time)
{
    static const int dr[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    static const int dc[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
    int *visited = NULL;
    int num_objects = 0;
    int r, c, k;
    Stack stk;
    struct timespec ts_start, ts_end;
    size_t total_cells;

    if (matrix == NULL || rows <= 0 || cols <= 0) {
        return 0;
    }

    total_cells = (size_t)rows * (size_t)cols;
    visited = (int *)calloc(total_cells, sizeof(int));
    if (visited == NULL) {
        return -1;
    }

    if (stack_init(&stk, 1024) != 0) {
        free(visited);
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &ts_start) != 0) {
        free(visited);
        stack_free(&stk);
        return -1;
    }

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            int idx = r * cols + c;
            if (matrix[idx] == 1 && visited[idx] == 0) {
                num_objects++;
                visited[idx] = 1;
                if (stack_push(&stk, r, c) != 0) {
                    free(visited);
                    stack_free(&stk);
                    return -1;
                }

                while (!stack_is_empty(&stk)) {
                    Point curr = stack_pop(&stk);
                    for (k = 0; k < 8; k++) {
                        int nr = curr.r + dr[k];
                        int nc = curr.c + dc[k];
                        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                            int nidx = nr * cols + nc;
                            if (matrix[nidx] == 1 && visited[nidx] == 0) {
                                visited[nidx] = 1;
                                if (stack_push(&stk, nr, nc) != 0) {
                                    free(visited);
                                    stack_free(&stk);
                                    return -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &ts_end) != 0) {
        free(visited);
        stack_free(&stk);
        return -1;
    }

    if (out_time != NULL) {
        *out_time = (double)(ts_end.tv_sec - ts_start.tv_sec) +
                    (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000000.0;
    }

    stack_free(&stk);
    free(visited);
    return num_objects;
}

/*
 * Conta objetos em paralelo utilizando decomposicao em faixas de linhas,
 * Pthreads e Union-Find para consolidacao global.
 */
int count_objects_parallel(const int *matrix, int rows, int cols, int num_threads, double *out_time)
{
    pthread_t *threads = NULL;
    ThreadData *td_array = NULL;
    int *labels = NULL;
    DSU dsu;
    pthread_mutex_t dsu_mutex;
    Barrier barrier;
    struct timespec ts_start, ts_end;
    int total_cells;
    int base_rows, remainder, curr_start;
    int t;
    int total_objects = 0;
    unsigned char *counted_roots = NULL;

    if (matrix == NULL || rows <= 0 || cols <= 0 || num_threads <= 0) {
        return 0;
    }

    /* Ajusta numero de threads se exceder o numero de linhas */
    if (num_threads > rows) {
        num_threads = rows;
    }

    total_cells = rows * cols;
    labels = (int *)calloc((size_t)total_cells, sizeof(int));
    if (labels == NULL) {
        fprintf(stderr, "Erro ao alocar matriz de rotulos\n");
        return -1;
    }

    if (dsu_init(&dsu, total_cells) != 0) {
        fprintf(stderr, "Erro ao inicializar DSU\n");
        free(labels);
        return -1;
    }

    if (pthread_mutex_init(&dsu_mutex, NULL) != 0) {
        fprintf(stderr, "Erro ao inicializar mutex do DSU\n");
        dsu_free(&dsu);
        free(labels);
        return -1;
    }

    if (barrier_init(&barrier, num_threads) != 0) {
        fprintf(stderr, "Erro ao inicializar barreira\n");
        pthread_mutex_destroy(&dsu_mutex);
        dsu_free(&dsu);
        free(labels);
        return -1;
    }

    threads = (pthread_t *)malloc((size_t)num_threads * sizeof(pthread_t));
    td_array = (ThreadData *)malloc((size_t)num_threads * sizeof(ThreadData));
    if (threads == NULL || td_array == NULL) {
        fprintf(stderr, "Erro de memoria para estruturas de threads\n");
        if (threads) free(threads);
        if (td_array) free(td_array);
        barrier_destroy(&barrier);
        pthread_mutex_destroy(&dsu_mutex);
        dsu_free(&dsu);
        free(labels);
        return -1;
    }

    /* Distribuicao uniforme das linhas entre as threads */
    base_rows = rows / num_threads;
    remainder = rows % num_threads;
    curr_start = 0;

    for (t = 0; t < num_threads; t++) {
        int count = base_rows + (t < remainder ? 1 : 0);
        td_array[t].thread_id = t;
        td_array[t].num_threads = num_threads;
        td_array[t].rows = rows;
        td_array[t].cols = cols;
        td_array[t].start_row = curr_start;
        td_array[t].end_row = curr_start + count;
        td_array[t].matrix = matrix;
        td_array[t].labels = labels;
        td_array[t].local_comps = NULL;
        td_array[t].num_local_comps = 0;
        td_array[t].cap_local_comps = 0;
        td_array[t].dsu = &dsu;
        td_array[t].dsu_mutex = &dsu_mutex;
        td_array[t].barrier = &barrier;

        curr_start += count;
    }

    /* Inicio da medicao de tempo */
    if (clock_gettime(CLOCK_MONOTONIC, &ts_start) != 0) {
        perror("clock_gettime ts_start");
    }

    /* Criacao das threads */
    for (t = 0; t < num_threads; t++) {
        int rc = pthread_create(&threads[t], NULL, worker_func, &td_array[t]);
        if (rc != 0) {
            fprintf(stderr, "Erro ao criar thread %d: %s\n", t, strerror(rc));
            /* Em caso de falha, espera as threads ja criadas */
            while (--t >= 0) {
                pthread_join(threads[t], NULL);
            }
            free(threads);
            free(td_array);
            barrier_destroy(&barrier);
            pthread_mutex_destroy(&dsu_mutex);
            dsu_free(&dsu);
            free(labels);
            return -1;
        }
    }

    /* Aguarda a finalizacao de todos os workers */
    for (t = 0; t < num_threads; t++) {
        int rc = pthread_join(threads[t], NULL);
        if (rc != 0) {
            fprintf(stderr, "Erro ao fazer join da thread %d: %s\n", t, strerror(rc));
        }
    }

    /*
     * FASE FINAL: Contagem de Componentes Conexos Globais Distintos
     * Cada componente local tem seu ID passado pela funcao dsu_find.
     * Contabiliza-se cada raiz unica exatamente uma vez.
     */
    counted_roots = (unsigned char *)calloc((size_t)(total_cells + 1), sizeof(unsigned char));
    if (counted_roots == NULL) {
        fprintf(stderr, "Erro ao alocar vetor de contagem de raizes\n");
    } else {
        for (t = 0; t < num_threads; t++) {
            size_t i;
            for (i = 0; i < td_array[t].num_local_comps; i++) {
                int comp_id = td_array[t].local_comps[i];
                int root = dsu_find(&dsu, comp_id);
                if (!counted_roots[root]) {
                    counted_roots[root] = 1;
                    total_objects++;
                }
            }
        }
        free(counted_roots);
    }

    /* Fim da medicao de tempo */
    if (clock_gettime(CLOCK_MONOTONIC, &ts_end) != 0) {
        perror("clock_gettime ts_end");
    }

    if (out_time != NULL) {
        *out_time = (double)(ts_end.tv_sec - ts_start.tv_sec) +
                    (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000000.0;
    }

    /* Liberacao de recursos */
    for (t = 0; t < num_threads; t++) {
        if (td_array[t].local_comps != NULL) {
            free(td_array[t].local_comps);
        }
    }
    free(td_array);
    free(threads);
    barrier_destroy(&barrier);
    pthread_mutex_destroy(&dsu_mutex);
    dsu_free(&dsu);
    free(labels);

    return total_objects;
}

/*
 * Imprime visualmente a matriz no terminal mostrando as cores dos objetos
 * e as linhas divisorias correspondentes a divisao por faixas de threads.
 */
static void print_visual_matrix_parallel(const int *matrix, int rows, int cols, int num_threads)
{
    static const char *colors[8] = {
        "\033[1;34m", /* Azul */
        "\033[1;32m", /* Verde */
        "\033[1;35m", /* Magenta */
        "\033[1;33m", /* Amarelo */
        "\033[1;31m", /* Vermelho */
        "\033[1;36m", /* Ciano */
        "\033[1;92m", /* Verde claro */
        "\033[1;95m"  /* Magenta claro */
    };
    static const char *reset = "\033[0m";
    static const char *gray = "\033[90m";
    static const char *border_col = "\033[1;33m";

    static const int dr[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    static const int dc[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

    int *labels;
    int num_objects = 0;
    int r, c, k, t;
    int base_rows, remainder, curr_start;
    int *thread_starts;
    Stack stk;

    if (num_threads > rows) {
        num_threads = rows;
    }

    labels = (int *)calloc((size_t)rows * (size_t)cols, sizeof(int));
    if (labels == NULL) {
        return;
    }
    thread_starts = (int *)malloc(((size_t)num_threads + 1) * sizeof(int));
    if (thread_starts == NULL) {
        free(labels);
        return;
    }

    base_rows = rows / num_threads;
    remainder = rows % num_threads;
    curr_start = 0;
    for (t = 0; t < num_threads; t++) {
        thread_starts[t] = curr_start;
        curr_start += base_rows + (t < remainder ? 1 : 0);
    }
    thread_starts[num_threads] = rows;

    if (stack_init(&stk, 1024) != 0) {
        free(thread_starts);
        free(labels);
        return;
    }

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            int idx = r * cols + c;
            if (matrix[idx] == 1 && labels[idx] == 0) {
                num_objects++;
                labels[idx] = num_objects;
                stack_push(&stk, r, c);
                while (!stack_is_empty(&stk)) {
                    Point curr = stack_pop(&stk);
                    for (k = 0; k < 8; k++) {
                        int nr = curr.r + dr[k];
                        int nc = curr.c + dc[k];
                        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                            int nidx = nr * cols + nc;
                            if (matrix[nidx] == 1 && labels[nidx] == 0) {
                                labels[nidx] = num_objects;
                                stack_push(&stk, nr, nc);
                            }
                        }
                    }
                }
            }
        }
    }

    stack_free(&stk);

    printf("\n=== Visualizacao Paralela (%dx%d, %d Threads, %d Objetos) ===\n",
           rows, cols, num_threads, num_objects);

    for (t = 0; t < num_threads; t++) {
        int r_begin = thread_starts[t];
        int r_end = thread_starts[t + 1];

        if (r_end > r_begin) {
            printf("%s--- Faixa da Thread %d: Linhas [%d .. %d] ---%s\n",
                   border_col, t, r_begin, r_end - 1, reset);

            for (r = r_begin; r < r_end; r++) {
                printf("  [Linha %2d] ", r);
                for (c = 0; c < cols; c++) {
                    int idx = r * cols + c;
                    if (matrix[idx] == 0) {
                        printf("%s . %s", gray, reset);
                    } else {
                        int obj = labels[idx];
                        const char *col = colors[(obj - 1) % 8];
                        printf("%s%2d %s", col, obj, reset);
                    }
                }
                printf("\n");
            }
        }
    }
    printf("===============================================================\n\n");

    free(thread_starts);
    free(labels);
}

/* ============================================================================
 * MATRIZES OBRIGATORIAS DE TESTE
 * ============================================================================
 */
static const int TEST_M1[5 * 5] = {
    1, 1, 0, 0, 0,
    1, 1, 0, 0, 0,
    0, 0, 0, 1, 0,
    0, 0, 0, 1, 0,
    1, 0, 0, 0, 0
};

static const int TEST_M2[6 * 8] = {
    0, 0, 0, 0, 0, 0, 1, 1,
    0, 1, 1, 1, 1, 0, 1, 0,
    0, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 1,
    1, 1, 0, 0, 0, 0, 1, 1
};

static const int TEST_M3[8 * 8] = {
    1, 1, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 1, 1, 0, 1, 0,
    0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 1,
    0, 0, 1, 0, 0, 0, 1, 1
};

static const int TEST_M4[9 * 12] = {
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0,
    0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1
};

static const int TEST_M5[12 * 12] = {
    1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1,
    0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1
};

/* Bateria de testes das 5 matrizes obrigatorias comparando com esperado */
int run_all_tests(int num_threads)
{
    struct TestCase {
        const char *name;
        const int *matrix;
        int rows;
        int cols;
        int expected;
    };

    struct TestCase tests[5];
    int i;
    int all_passed = TRUE;

    tests[0].name = "Exemplo 1 (5x5)";
    tests[0].matrix = TEST_M1;
    tests[0].rows = 5;
    tests[0].cols = 5;
    tests[0].expected = 3;

    tests[1].name = "Exemplo 2 (6x8)";
    tests[1].matrix = TEST_M2;
    tests[1].rows = 6;
    tests[1].cols = 8;
    tests[1].expected = 4;

    tests[2].name = "Exemplo 3 (8x8)";
    tests[2].matrix = TEST_M3;
    tests[2].rows = 8;
    tests[2].cols = 8;
    tests[2].expected = 5;

    tests[3].name = "Exemplo 4 (9x12)";
    tests[3].matrix = TEST_M4;
    tests[3].rows = 9;
    tests[3].cols = 12;
    tests[3].expected = 6;

    tests[4].name = "Exemplo 5 (12x12)";
    tests[4].matrix = TEST_M5;
    tests[4].rows = 12;
    tests[4].cols = 12;
    tests[4].expected = 7;

    printf("====================================================\n");
    printf("  BATERIA DE TESTES OBRIGATORIOS (PARALELO - %d THREADS)\n", num_threads);
    printf("====================================================\n");

    for (i = 0; i < 5; i++) {
        double elapsed = 0.0;
        int result = count_objects_parallel(tests[i].matrix,
                                            tests[i].rows,
                                            tests[i].cols,
                                            num_threads,
                                            &elapsed);

        if (result == tests[i].expected) {
            printf("[%s] Esperado: %2d | Obtido: %2d -> OK  (tempo: %.6f s)\n",
                   tests[i].name, tests[i].expected, result, elapsed);
        } else {
            printf("[%s] Esperado: %2d | Obtido: %2d -> ERRO (tempo: %.6f s)\n",
                   tests[i].name, tests[i].expected, result, elapsed);
            all_passed = FALSE;
        }
    }

    printf("----------------------------------------------------\n");
    if (all_passed) {
        printf("Resultado: TODOS OS TESTES PASSARAM COM SUCESSO!\n");
    } else {
        printf("Resultado: FALHA EM UM OU MAIS TESTES.\n");
    }
    printf("====================================================\n\n");

    return all_passed ? 0 : 1;
}

/* Leitor de matriz a partir de stream */
static int load_matrix_from_stream(FILE *fp, int *out_rows, int *out_cols, int **out_matrix)
{
    int capacity = 1024;
    int count = 0;
    int *raw = NULL;
    int val;
    int i;

    if (fp == NULL || out_rows == NULL || out_cols == NULL || out_matrix == NULL) {
        return -1;
    }

    raw = (int *)malloc((size_t)capacity * sizeof(int));
    if (raw == NULL) {
        return -1;
    }

    while (fscanf(fp, "%d", &val) == 1) {
        if (count >= capacity) {
            int new_cap = capacity * 2;
            int *new_raw = (int *)realloc(raw, (size_t)new_cap * sizeof(int));
            if (new_raw == NULL) {
                free(raw);
                return -1;
            }
            raw = new_raw;
            capacity = new_cap;
        }
        raw[count++] = val;
    }

    if (count == 0) {
        free(raw);
        *out_rows = 0;
        *out_cols = 0;
        *out_matrix = NULL;
        return 0;
    }

    /* Caso com cabecalho 'R C': se R > 0 e C > 0 e R * C == count - 2 */
    if (count >= 2 && raw[0] > 0 && raw[1] > 0 && (size_t)raw[0] * (size_t)raw[1] == (size_t)(count - 2)) {
        int expected = raw[0] * raw[1];
        int *data = (int *)malloc((size_t)expected * sizeof(int));
        if (data == NULL) {
            free(raw);
            return -1;
        }
        for (i = 0; i < expected; i++) {
            data[i] = (raw[i + 2] != 0) ? 1 : 0;
        }
        *out_rows = raw[0];
        *out_cols = raw[1];
        *out_matrix = data;
        free(raw);
        return 0;
    }

    /* Caso sem cabecalho (ex: matriz 1xN): trata os dados lidos como matriz 1 x count */
    for (i = 0; i < count; i++) {
        raw[i] = (raw[i] != 0) ? 1 : 0;
    }
    *out_rows = 1;
    *out_cols = count;
    *out_matrix = raw;
    return 0;
}

static int load_matrix_from_file(const char *filename, int *out_rows, int *out_cols, int **out_matrix)
{
    FILE *fp;
    int res;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Nao foi possivel abrir o arquivo '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }

    res = load_matrix_from_stream(fp, out_rows, out_cols, out_matrix);
    fclose(fp);
    return res;
}

static int *create_random_matrix(int rows, int cols, double density, unsigned int seed)
{
    int *mat;
    size_t total;
    size_t i;
    int threshold;

    total = (size_t)rows * (size_t)cols;
    mat = (int *)malloc(total * sizeof(int));
    if (mat == NULL) {
        return NULL;
    }

    srand(seed);
    threshold = (int)(density * (double)RAND_MAX);

    for (i = 0; i < total; i++) {
        mat[i] = (rand() <= threshold) ? 1 : 0;
    }

    return mat;
}

/*
 * Funcao Principal
 */
int main(int argc, char *argv[])
{
    int num_threads = 4;
    int *matrix = NULL;
    int rows = 0;
    int cols = 0;
    int total_objects = 0;
    double elapsed_parallel = 0.0;
    int free_needed = FALSE;

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <num_threads> [arquivo_matriz | --test | --benchmark]\n", argv[0]);
        fprintf(stderr, "Exemplos:\n");
        fprintf(stderr, "  %s 4                       (roda bateria de testes com 4 threads)\n", argv[0]);
        fprintf(stderr, "  %s 4 --test                (roda apenas os testes)\n", argv[0]);
        fprintf(stderr, "  %s 4 data/exemplo1.txt     (processa matriz de arquivo)\n", argv[0]);
        fprintf(stderr, "  %s 4 --benchmark 1000 1000 (mede speedup em matriz 1000x1000)\n\n", argv[0]);
        printf("Executando bateria de testes padrao com 4 threads:\n");
        run_all_tests(4);
        return 0;
    }

    num_threads = atoi(argv[1]);
    if (num_threads <= 0) {
        fprintf(stderr, "Erro: Numero de threads deve ser maior que 0. Valor fornecido: %s\n", argv[1]);
        return 1;
    }

    /* Caso sem argumento extra: executa testes e exemplo padrao */
    if (argc == 2) {
        run_all_tests(num_threads);

        printf("Executando exemplo padrao hardcoded (Exemplo 1 - 5x5) com %d threads:\n",
               num_threads);
        matrix = (int *)TEST_M1;
        rows = 5;
        cols = 5;
        free_needed = FALSE;

        total_objects = count_objects_parallel(matrix, rows, cols, num_threads, &elapsed_parallel);
        printf("Objetos encontrados: %d\n", total_objects);
        printf("Tempo paralelo: %.6f segundos\n", elapsed_parallel);
        return 0;
    }

    /* Caso '--test' */
    if (strcmp(argv[2], "--test") == 0) {
        return run_all_tests(num_threads);
    }

    /* Caso '--visual' */
    if (strcmp(argv[2], "--visual") == 0) {
        const char *filename = (argc >= 4) ? argv[3] : "data/exemplo5.txt";
        if (load_matrix_from_file(filename, &rows, &cols, &matrix) != 0) {
            return 1;
        }
        total_objects = count_objects_parallel(matrix, rows, cols, num_threads, &elapsed_parallel);
        print_visual_matrix_parallel(matrix, rows, cols, num_threads);
        printf("Objetos encontrados: %d\n", total_objects);
        printf("Tempo paralelo (%d threads): %.6f segundos\n", num_threads, elapsed_parallel);
        free(matrix);
        return 0;
    }

    /* Caso '--benchmark' */
    if (strcmp(argv[2], "--benchmark") == 0) {
        unsigned int seed = 42;
        double elapsed_seq = 0.0;
        int objects_seq = 0;
        double speedup = 0.0;

        rows = 1000;
        cols = 1000;

        if (argc >= 4) {
            rows = atoi(argv[3]);
        }
        if (argc >= 5) {
            cols = atoi(argv[4]);
        }
        if (argc >= 6) {
            seed = (unsigned int)atoi(argv[5]);
        }

        printf("Gerando matriz aleatoria de benchmark: %dx%d (semente: %u, densidade: 30%%)...\n",
               rows, cols, seed);
        matrix = create_random_matrix(rows, cols, 0.30, seed);
        if (matrix == NULL) {
            fprintf(stderr, "Erro de memoria ao gerar matriz para benchmark.\n");
            return 1;
        }

        printf("Executando versao sequencial de referencia...\n");
        objects_seq = count_objects_seq_ref(matrix, rows, cols, &elapsed_seq);

        printf("Executando versao paralela com %d threads...\n", num_threads);
        total_objects = count_objects_parallel(matrix, rows, cols, num_threads, &elapsed_parallel);

        printf("\n====================================================\n");
        printf("  RESULTADOS DO BENCHMARK (%dx%d, %d THREADS)\n", rows, cols, num_threads);
        printf("====================================================\n");
        printf("Objetos encontrados (Sequencial): %d\n", objects_seq);
        printf("Objetos encontrados (Paralelo)  : %d\n", total_objects);
        if (objects_seq == total_objects) {
            printf("Verificacao de Corretude        : OK (Resultados identicos)\n");
        } else {
            printf("Verificacao de Corretude        : ERRO (Resultados divergentes!)\n");
        }
        printf("Tempo Sequencial                : %.6f segundos\n", elapsed_seq);
        printf("Tempo Paralelo                  : %.6f segundos\n", elapsed_parallel);

        if (elapsed_parallel > 0.0) {
            speedup = elapsed_seq / elapsed_parallel;
            printf("Aceleracao (Speedup S = T_seq/T_par): %.2fx\n", speedup);
        }
        printf("====================================================\n\n");

        free(matrix);
        return 0;
    }

    /* Leitura de stdin '-' */
    if (strcmp(argv[2], "-") == 0) {
        if (load_matrix_from_stream(stdin, &rows, &cols, &matrix) != 0) {
            fprintf(stderr, "Erro ao ler matriz da entrada padrao (stdin).\n");
            return 1;
        }
        free_needed = TRUE;
    } else {
        /* Leitura de arquivo */
        if (load_matrix_from_file(argv[2], &rows, &cols, &matrix) != 0) {
            return 1;
        }
        free_needed = TRUE;
    }

    total_objects = count_objects_parallel(matrix, rows, cols, num_threads, &elapsed_parallel);
    printf("Objetos encontrados: %d\n", total_objects);
    printf("Tempo paralelo (%d threads): %.6f segundos\n", num_threads, elapsed_parallel);

    if (free_needed && matrix != NULL) {
        free(matrix);
    }

    return 0;
}
