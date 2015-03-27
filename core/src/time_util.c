#include "time_util.h"


/* Metodo per il calcolo del timestamp (provvisorio) */


static __inline__ unsigned long int rdtsc(void)
{
  unsigned int hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ((unsigned long int)lo) | ( ((unsigned long int)hi) << 32 );
}

simtime_t get_timestamp(void)
{
  double ret = (double) rdtsc();
  return ret;
} 
