#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include <core.h>



unsigned short int number_of_threads = 1;

void *start_thread(void *args)
{
  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/start_thread: Inizio\n");
  #endif // MYDEBUG

  int tid = (int) __sync_fetch_and_add(&number_of_threads, 1);

  //START THREAD (definita in core.c)
  thread_loop(tid);

  pthread_exit(NULL);

  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/start_thread: Fine\n");
  #endif // MYDEBUG
}

void start_simulation(unsigned short int number_of_threads)
{
  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/start_simulation: Inizio\n");
  #endif // MYDEBUG

  pthread_t p_tid[number_of_threads - 1];
  int ret, i;

  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/start_simulation: inizio fork\n");
  #endif // MYDEBUG

  //Child thread
  for(i = 0; i < number_of_threads - 1; i++)
  {
    if( (ret = pthread_create(&p_tid[i], NULL, start_thread, NULL)) != 0)
    {
      fprintf(stderr, "%s\n", strerror(errno));
      abort();
    }
  }

  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/start_simulation: fine fork\n");
  #endif // MYDEBUG

  //Main thread
  thread_loop(0);

  for(i = 0; i < number_of_threads - 1; i++)
    pthread_join(p_tid[i], NULL);

  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/start_simulation: Fine\n");
  #endif // MYDEBUG
}

int main(int argn, char *argv[]) {
    #ifdef MYDEBUG
    printf("MYDEBUG-   main.c/main: Inizio main\n");
    #endif // MYDEBUG

  unsigned int n;

  if(argn < 3) {
    fprintf(stderr, "Usage: %s: n_threads n_lps\n", argv[0]);
    exit(EXIT_FAILURE);

  } else
  {
    n = atoi(argv[1]);
    init(n, atoi(argv[2]));
  }

  printf("Start simulation\n");

  start_simulation(n);

  printf("Simulation ended\n");

  #ifdef MYDEBUG
  printf("MYDEBUG-   main.c/main: Fine main\n");
  #endif // MYDEBUG

  return 0;
}

