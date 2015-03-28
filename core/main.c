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
  int tid = (int) __sync_fetch_and_add(&number_of_threads, 1);
  
  //START THREAD (definita in core.c)
  thread_loop(tid);
  
  pthread_exit(NULL);
}

void start_simulation(unsigned short int number_of_threads)
{
  pthread_t tid[number_of_threads - 1];
  int ret, i;
  
  //Child thread
  for(i = 0; i < number_of_threads - 1; i++)
  {
    if( (ret = pthread_create(&tid[i], NULL, start_thread, NULL)) != 0)
    {
      fprintf(stderr, "%s\n", strerror(errno));
      abort();
    }
  }
  
  //Main thread
  thread_loop(0);
  
  for(i = 0; i < number_of_threads - 1; i++)
    pthread_join(tid[i], NULL);
}


int main(int argn, char *argv[]) {

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
    
  return 0;
}

