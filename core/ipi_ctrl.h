#pragma once
#ifndef __IPI_CTRL__
#define __IPI_CTRL__

extern int ipi_register_thread(int, unsigned long, void **, unsigned long long **,
				unsigned long long **, unsigned long, unsigned long, unsigned long);
extern int ipi_unregister_thread(void **, unsigned long);

#endif