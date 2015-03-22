#pragma once
#ifndef __CORE_H
#define __CORE_H



extern unsigned int n_cores;

extern __thread unsigned int tid;


void init(void);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);


#endif
