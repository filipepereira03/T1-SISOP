/*
 * ============================================================================
 * Arquivo: conta-objetos-sequencial.c
 * Descricao: Contagem sequencial de objetos em uma matriz binaria usando
 *            flood fill iterativo com conectividade 8 (arestas e vertices).
 * Padrao: ANSI C (C89 / C90) estrito.
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0

/* Estrutura para representar coordenadas na matriz */
typedef struct {
    int r;
    int c;
} Point;

/* Pilha dinamica explicita para evitar recursao e estouro de pilha */
typedef struct {
    Point *data;
    size_t top;
    size_t capacity;
} Stack;

/* Inicializacao da pilha */
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

/* Empilhar elemento, expandindo capacidade se necessario */
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

/* Desempilhar elemento */
static Point stack_pop(Stack *s)
{
    s->top--;
    return s->data[s->top];
}

/* Verificar se pilha esta vazia */
static int stack_is_empty(const Stack *s)
{
    return (s->top == 0);
}

/* Liberar recursos da pilha */
static void stack_free(Stack *s)
{
    if (s->data != NULL) {
        free(s->data);
        s->data = NULL;
    }
    s->top = 0;
    s->capacity = 0;
}

/*
 * Conta os objetos conexos (conectividade 8) em uma matriz binaria
 * usando flood fill iterativo com pilha explicita.
 * Retorna o numero de objetos encontrados ou -1 em caso de erro de memoria.
 */
int count_objects_sequential(const int *matrix, int rows, int cols, double *out_time)
{
    /* Deslocamentos para os 8 vizinhos (cima, baixo, esquerda, direita e 4 diagonais) */
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
        fprintf(stderr, "Erro ao alocar matriz de visitados (%lu bytes)\n",
                (unsigned long)(total_cells * sizeof(int)));
        return -1;
    }

    if (stack_init(&stk, 1024) != 0) {
        fprintf(stderr, "Erro ao inicializar pilha para flood fill\n");
        free(visited);
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &ts_start) != 0) {
        perror("clock_gettime ts_start");
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
                    fprintf(stderr, "Erro ao empilhar elemento\n");
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
                                    fprintf(stderr, "Erro ao empilhar vizinho\n");
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
        perror("clock_gettime ts_end");
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
 * Imprime visualmente a matriz no terminal com cores ANSI para cada objeto conexo
 */
static void print_visual_matrix(const int *matrix, int rows, int cols)
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

    static const int dr[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    static const int dc[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

    int *labels;
    int num_objects = 0;
    int r, c, k;
    Stack stk;

    labels = (int *)calloc((size_t)rows * (size_t)cols, sizeof(int));
    if (labels == NULL) {
        return;
    }
    if (stack_init(&stk, 1024) != 0) {
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

    printf("\n--- Visualizacao da Matriz (%dx%d, %d objetos) ---\n", rows, cols, num_objects);
    for (r = 0; r < rows; r++) {
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
    printf("--------------------------------------------------\n\n");
    free(labels);
}

/*
 * Leitor robusto de matriz binaria a partir de um fluxo (arquivo ou stdin).
 * Suporta formatos:
 * 1) Cabecalho com dimensoes: '<linhas> <colunas>' seguido pelos valores 0/1.
 * 2) Grade crua de linhas com digitos separados por espacos.
 */
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

/* Leitor de arquivo */
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

/* Gera matriz aleatoria para benchmark */
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

/* ============================================================================
 * MATRIZES OBRIGATORIAS DE TESTE
 * ============================================================================
 */

/* Exemplo 1 - 5x5 - esperado: 3 */
static const int TEST_M1[5 * 5] = {
    1, 1, 0, 0, 0,
    1, 1, 0, 0, 0,
    0, 0, 0, 1, 0,
    0, 0, 0, 1, 0,
    1, 0, 0, 0, 0
};

/* Exemplo 2 - 6x8 - esperado: 4 */
static const int TEST_M2[6 * 8] = {
    0, 0, 0, 0, 0, 0, 1, 1,
    0, 1, 1, 1, 1, 0, 1, 0,
    0, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 1,
    1, 1, 0, 0, 0, 0, 1, 1
};

/* Exemplo 3 - 8x8 - esperado: 5 */
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

/* Exemplo 4 - 9x12 - esperado: 6 */
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

/* Exemplo 5 - 12x12 - esperado: 7 */
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

/* Executa bateria de testes das 5 matrizes obrigatorias */
int run_all_tests(void)
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
    printf("  BATERIA DE TESTES OBRIGATORIOS (SEQUENCIAL)\n");
    printf("====================================================\n");

    for (i = 0; i < 5; i++) {
        double elapsed = 0.0;
        int result = count_objects_sequential(tests[i].matrix,
                                              tests[i].rows,
                                              tests[i].cols,
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

/*
 * Funcao Principal
 */
int main(int argc, char *argv[])
{
    int *matrix = NULL;
    int rows = 0;
    int cols = 0;
    int total_objects = 0;
    double elapsed_time = 0.0;
    int free_needed = FALSE;

    /* Sem argumentos: executa bateria de testes e exemplo padrao */
    if (argc == 1) {
        run_all_tests();

        printf("Executando exemplo padrao hardcoded (Exemplo 1 - 5x5):\n");
        matrix = (int *)TEST_M1;
        rows = 5;
        cols = 5;
        free_needed = FALSE;

        total_objects = count_objects_sequential(matrix, rows, cols, &elapsed_time);
        printf("Objetos encontrados: %d\n", total_objects);
        printf("Tempo sequencial: %.6f segundos\n", elapsed_time);
        return 0;
    }

    /* Argumento '--test' */
    if (strcmp(argv[1], "--test") == 0) {
        return run_all_tests();
    }

    /* Argumento '--visual' */
    if (strcmp(argv[1], "--visual") == 0) {
        const char *filename = (argc >= 3) ? argv[2] : "data/exemplo1.txt";
        if (load_matrix_from_file(filename, &rows, &cols, &matrix) != 0) {
            return 1;
        }
        total_objects = count_objects_sequential(matrix, rows, cols, &elapsed_time);
        print_visual_matrix(matrix, rows, cols);
        printf("Objetos encontrados: %d\n", total_objects);
        printf("Tempo sequencial: %.6f segundos\n", elapsed_time);
        free(matrix);
        return 0;
    }

    /* Argumento '--benchmark' */
    if (strcmp(argv[1], "--benchmark") == 0) {
        unsigned int seed = 42;
        rows = 1000;
        cols = 1000;

        if (argc >= 3) {
            rows = atoi(argv[2]);
        }
        if (argc >= 4) {
            cols = atoi(argv[3]);
        }
        if (argc >= 5) {
            seed = (unsigned int)atoi(argv[4]);
        }

        printf("Gerando matriz aleatoria %dx%d (semente: %u)...\n", rows, cols, seed);
        matrix = create_random_matrix(rows, cols, 0.30, seed);
        if (matrix == NULL) {
            fprintf(stderr, "Erro de memoria ao gerar matriz de benchmark.\n");
            return 1;
        }
        free_needed = TRUE;

        printf("Executando contagem sequencial...\n");
        total_objects = count_objects_sequential(matrix, rows, cols, &elapsed_time);

        printf("Objetos encontrados: %d\n", total_objects);
        printf("Tempo sequencial: %.6f segundos\n", elapsed_time);

        if (free_needed && matrix != NULL) {
            free(matrix);
        }
        return 0;
    }

    /* Argumento '-' para leitura de stdin */
    if (strcmp(argv[1], "-") == 0) {
        if (load_matrix_from_stream(stdin, &rows, &cols, &matrix) != 0) {
            fprintf(stderr, "Erro ao ler matriz da entrada padrao (stdin).\n");
            return 1;
        }
        free_needed = TRUE;
    } else {
        /* Nome de arquivo fornecido */
        if (load_matrix_from_file(argv[1], &rows, &cols, &matrix) != 0) {
            return 1;
        }
        free_needed = TRUE;
    }

    total_objects = count_objects_sequential(matrix, rows, cols, &elapsed_time);
    printf("Objetos encontrados: %d\n", total_objects);
    printf("Tempo sequencial: %.6f segundos\n", elapsed_time);

    if (free_needed && matrix != NULL) {
        free(matrix);
    }

    return 0;
}
