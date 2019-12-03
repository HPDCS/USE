#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>


#define IPI_REGISTER_THREAD     (1U << 2)
#define IPI_UNREGISTER_THREAD   (1U << 3)
#define IPI_SET_TEXT_START      (1U << 4)
#define IPI_SET_TEXT_END        (1U << 5)


int ipi_register_thread(int, unsigned long, unsigned long, unsigned long);
int ipi_unregister_thread(void);


static __thread int fd;
static __thread int cpu;
static __thread cpu_set_t oldset;


static inline int pin_thread_to_core(int core)
{
	int sched_cpu;
	cpu_set_t cpuset;

    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &oldset);

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

static inline int remove_thread_pinning(void)
{
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &oldset))
        return 1;

    return 0;
}

int ipi_register_thread(int tid, unsigned long callback,
        unsigned long text_start, unsigned long text_end)
{
    if (tid < 0)
    {
    	printf("The argument \"tid\" is a negative value. "
    			"Thread will work with no IPI support.\n");
		return 1;
    }

    if (text_end < text_start)
    {
        printf("Invalid arguments \"text_start\" and \"text_end\". "
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

        if (remove_thread_pinning())
            printf("Unable to remove thread pinning from core %d.\n", cpu);

        return 1;
    }

    if (ioctl(fd, IPI_REGISTER_THREAD, callback) < 0)
    {
        printf("Unable to register thread for IPI on core %d. "
        		"Thread will work with no IPI support.\n", cpu);

        close(fd);

        if (remove_thread_pinning())
            printf("Unable to remove thread pinning from core %d.\n", cpu);

        return 1;
    }

    if (ioctl(fd, IPI_SET_TEXT_START, text_start) < 0)
    {
        printf("Unable to set text_start with address 0x%lx. "
                "Thread will work with no IPI support.\n", text_start);

        ioctl(fd, IPI_UNREGISTER_THREAD);
        close(fd);

        if (remove_thread_pinning())
            printf("Unable to remove thread pinning from core %d.\n", cpu);

        return 1;
    }

    if (ioctl(fd, IPI_SET_TEXT_END, text_end) < 0)
    {
        printf("Unable to set text_end with address 0x%lx. "
                "Thread will work with no IPI support.\n", text_end);

        ioctl(fd, IPI_UNREGISTER_THREAD);
        close(fd);

        if (remove_thread_pinning())
            printf("Unable to remove thread pinning from core %d.\n", cpu);

        return 1;
    }

    return 0;
}

int ipi_unregister_thread(void)
{
    int res = 0;

    if (ioctl(fd, IPI_UNREGISTER_THREAD) < 0)
    {
        printf("Unable to unregister thread for IPI support on core %d.\n", cpu);
        res = 1;
    }

    close(fd);

    if (remove_thread_pinning())
    {
        printf("Unable to remove thread pinning from core %d.\n", cpu);
        res = 1;
    }

    return res;
}