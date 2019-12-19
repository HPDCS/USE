#pragma once
#ifndef __IPI_CTRL__
#define __IPI_CTRL__

<<<<<<< HEAD
extern int ipi_register_thread(int, unsigned long, void **, unsigned long, unsigned long*, unsigned long*,unsigned int);
=======
extern int ipi_register_thread(int, unsigned long, void **, unsigned long long **,
									unsigned long, unsigned long, unsigned long);
>>>>>>> Registration of non_preemptable_counter variable
extern int ipi_unregister_thread(void **, unsigned long);

#endif