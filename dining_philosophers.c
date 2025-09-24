#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define DEFAULT_N 5
#define DEFAULT_ITER 10

typedef enum { DEADLOCK, ARBITER } philosopher_mode_t;

int N = DEFAULT_N;
int ITER = DEFAULT_ITER;
philosopher_mode_t MODE = DEADLOCK;
pthread_mutex_t *forks;
sem_t waiter;
pthread_mutex_t print_lock;
int deadlock_timeout = 5;
time_t last_activity;
pthread_mutex_t time_lock;

void print_msg(int id, const char* msg) {
    pthread_mutex_lock(&print_lock);
    printf("Philosopher %d: %s\n", id, msg);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);
}

void update_activity() {
    pthread_mutex_lock(&time_lock);
    last_activity = time(NULL);
    pthread_mutex_unlock(&time_lock);
}

void* philosopher(void *arg) {
    int id = *(int*)arg;
    free(arg);
    
    int left = id;
    int right = (id + 1) % N;
    
    for (int i = 0; i < ITER; i++) {
        print_msg(id, "hungry");
        
        if (MODE == DEADLOCK) {
            pthread_mutex_lock(&forks[left]);
            print_msg(id, "got left fork");
            
            usleep(50000 + rand() % 100000);
            
            pthread_mutex_lock(&forks[right]);
            print_msg(id, "got right fork");
            
        } else if (MODE == ARBITER) {
            sem_wait(&waiter);
            pthread_mutex_lock(&forks[left]);
            pthread_mutex_lock(&forks[right]);
            print_msg(id, "got both forks (with waiter)");
        }
        
        print_msg(id, "eating");
        update_activity();
        usleep(50000 + rand() % 100000);
        
        pthread_mutex_unlock(&forks[right]);
        pthread_mutex_unlock(&forks[left]);
        print_msg(id, "finished eating");
        
        if (MODE == ARBITER) {
            sem_post(&waiter);
        }
    }
    
    print_msg(id, "done");
    return NULL;
}

void* monitor(void *arg) {
    while (1) {
        sleep(deadlock_timeout);
        
        pthread_mutex_lock(&time_lock);
        time_t now = time(NULL);
        if (now - last_activity > deadlock_timeout) {
            printf("\nDEADLOCK - no activity for %d seconds\n", deadlock_timeout);
            exit(1);
        }
        pthread_mutex_unlock(&time_lock);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "arbiter") == 0) MODE = ARBITER;
        else MODE = DEADLOCK;
    }
    if (argc >= 3) N = atoi(argv[2]) > 1 ? atoi(argv[2]) : DEFAULT_N;
    if (argc >= 4) ITER = atoi(argv[3]) > 0 ? atoi(argv[3]) : DEFAULT_ITER;
    if (argc >= 5) deadlock_timeout = atoi(argv[4]) > 0 ? atoi(argv[4]) : 5;
    
    printf("Mode: %s, N=%d, Iterations=%d\n", 
           MODE == DEADLOCK ? "deadlock" : "arbiter", N, ITER);
    
    srand(time(NULL));
    
    forks = malloc(N * sizeof(pthread_mutex_t));
    pthread_t *threads = malloc(N * sizeof(pthread_t));
    
    for (int i = 0; i < N; i++) {
        pthread_mutex_init(&forks[i], NULL);
    }
    pthread_mutex_init(&print_lock, NULL);
    pthread_mutex_init(&time_lock, NULL);
    
    if (MODE == ARBITER) {
        sem_init(&waiter, 0, N - 1);
    }
    
    last_activity = time(NULL);
    
    pthread_t mon;
    pthread_create(&mon, NULL, monitor, NULL);
    
    for (int i = 0; i < N; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, philosopher, id);
    }
    
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Success.\n");
    
    for (int i = 0; i < N; i++) {
        pthread_mutex_destroy(&forks[i]);
    }
    if (MODE == ARBITER) {
        sem_destroy(&waiter);
    }
    pthread_mutex_destroy(&print_lock);
    pthread_mutex_destroy(&time_lock);
    free(forks);
    free(threads);
    
    return 0;
}