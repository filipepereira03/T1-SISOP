/* Testes de falhas das dependencias usados pela implementacao real. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int failure_mode;
static int creates;
static int joins;

static void *test_malloc(size_t size)
{
    if (failure_mode == 1 && size == 512 * sizeof(int) * 2) {
        return NULL;
    }
    return malloc(size);
}

static void *test_calloc(size_t count, size_t size)
{
    if (failure_mode == 3 && count == 10 && size == sizeof(unsigned char)) {
        return NULL;
    }
    return calloc(count, size);
}

static int test_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                               void *(*start)(void *), void *arg)
{
    creates++;
    if (failure_mode == 2 && creates == 2) {
        return EAGAIN;
    }
    return pthread_create(thread, attr, start, arg);
}

static int test_pthread_join(pthread_t thread, void **value)
{
    int rc = pthread_join(thread, value);
    joins++;
    if (failure_mode == 4 && joins == 1 && rc == 0) {
        return EINVAL;
    }
    return rc;
}

#define malloc test_malloc
#define calloc test_calloc
#define pthread_create test_pthread_create
#define pthread_join test_pthread_join
#define main app_main
#include "../src/conta-objetos-paralelo.c"
#undef main
#undef pthread_join
#undef pthread_create
#undef calloc
#undef malloc

int main(int argc, char **argv)
{
    static const int matrix[9] = {1, 0, 0, 0, 0, 0, 0, 0, 1};
    int result;
    if (argc != 2) {
        return 2;
    }
    failure_mode = atoi(argv[1]);
    result = count_objects_parallel(matrix, 3, 3, 2, NULL);
    if (result != -1) {
        fprintf(stderr, "falha %d: esperado -1, obtido %d\n", failure_mode, result);
        return 1;
    }
    return 0;
}
