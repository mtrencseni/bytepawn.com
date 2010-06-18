#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include "stm.h"

#define NTHREADS    10
#define NLOOPS      100

static stm_word_t stm_counter;
void stm_increase()
{
    stm_word_t local;
    sigjmp_buf *jmp = stm_get_env();
    sigsetjmp(*jmp, 0);
    stm_start(jmp, NULL);
    
    local = stm_load(&stm_counter);
    local++;
    stm_store(&stm_counter, local);
    
    stm_commit();
}

static long pthread_counter;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void pthread_increase()
{
    pthread_mutex_lock(&lock);
    pthread_counter++;
    pthread_mutex_unlock(&lock);
}

static long naive_counter = 0;
void naive_increase()
{
    naive_counter++;
}

void* tfunc(void* param) // accepts a function pointer
{
    int i;
    void (*increase)();
    
    increase = param;
    
    if (increase == stm_increase)
        stm_init_thread();
    
    for (i = 0; i < NLOOPS; i++)
        increase();

    if (increase == stm_increase)
        stm_exit_thread();
        
    return NULL;
}

void perf(void (*increase)())
{
    void* tret;
    int i, ret;
    pthread_t tid[NTHREADS];
    
    // launch threads
    for (i = 0; i < NTHREADS; i++) {
        // create thread with tfunc(), pass increase
        ret = pthread_create(&tid[i], NULL, &tfunc, increase);
        if (ret != 0) {
            fprintf(stderr, "pthread_create returned %d\n", ret); //errno
            exit(1);
        }
    }
    
    // wait for threads to exit
    for (i = 0; i < NTHREADS; i++) {
        ret = pthread_join(tid[i], &tret);
        if (ret != 0) {
            fprintf(stderr, "pthread_join returned %d\n", ret); //errno
            exit(1);
        }
    }
}

long int elapsed(struct timeval start, struct timeval end)
{
    return ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec) / 1000;
}

int main(void)
{
    struct timeval start, end;
    
    printf("NTHREADS: %d\t NLOOPS: %d\n", NTHREADS, NLOOPS);
    
    gettimeofday(&start, NULL);
    perf(&naive_increase);
    //printf("naive: \t\t%ld\n", naive_counter);
    gettimeofday(&end, NULL);
    printf("naive elapsed: %ld ms\n", elapsed(start, end));
    
    // lock-based performance test
    gettimeofday(&start, NULL);
    perf(&pthread_increase);
    //printf("pthread: \t%ld\n", pthread_counter);
    gettimeofday(&end, NULL);
    printf("pthread elapsed: %ld ms\n", elapsed(start, end));

    // stm performance test
    gettimeofday(&start, NULL);
    stm_init();
    perf(&stm_increase);
    stm_exit();
    //printf("stm: \t\t%ld\n", stm_counter);
    gettimeofday(&end, NULL);
    printf("stm elapsed: %ld ms\n", elapsed(start, end));
    
    return 0;
}