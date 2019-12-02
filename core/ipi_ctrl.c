#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>


static __thread int fd;
static __thread int cpu;


static inline int pin_thread_to_core(int core)
{
	int sched_cpu;
	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
		return 1;

	while (1)
	{
		if ((sched_cpu = sched_getcpu()) == -1)
			return 1;
		else if (sched_cpu == core)
			break;
	}

	return 0;
}

int ipi_register_thread(int tid)
{
    if (tid < 0)
    {
    	printf("The argument \"tid\" is a negative value. "
    			"Thread will work with no IPI support.\n");
		return 1;
    }

    cpu = tid;

    if (pin_thread_to_core(cpu))
    {
        printf("Unable to pin thread to core %d. "
        		"Thread will work with no IPI support.\n", cpu);
        return 1;
    }

    if ((fd = open("/dev/ipi", (O_RDONLY | O_NONBLOCK))) < 0)
    {
        printf("Unable to open IPI device. "
        		"Thread will work with no IPI support.\n");
        return 1;
    }

    if (ioctl(fd, IPI_REGISTER_THREAD, ipi_trampoline) < 0)
    {
        printf("Unable to register thread for IPI on core %d. "
        		"Thread will work with no IPI support.\n", cpu);
        close(fd);
        return 1;
    }

    if (ioctl(fd, IPI_SET_TEXT_START, (unsigned long) start_func) < 0)
    {
        printf("Unable to set text_start address.\n");
        ioctl(fd, IPI_UNREGISTER_THREAD);
        close(fd);
        return 1;
    }

    if (ioctl(fd, IPI_SET_TEXT_END, (unsigned long) end_func) < 0)
    {
        printf("Unable to set text_end address.\n");
        ioctl(fd, IPI_UNREGISTER_THREAD);
        close(fd);
        return 1;
    }

    working_function(cpu);

    if (ioctl(fd, IPI_UNREGISTER_THREAD) < 0)
    {
        printf("Unable to unregister thread for IPI support on core %d.\n", cpu);
        close(fd);
        return 1;
    }

    close(fd);

    return 0;
}