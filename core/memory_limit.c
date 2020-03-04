#include <memory_limit.h>
#include <sys/resource.h>
#include <stdio.h>
#include <hpdcs_utils.h>

void set_max_memory_allocable(unsigned long max_bytes_allocable){
    struct rlimit set_memory_limit;
    set_memory_limit.rlim_cur=max_bytes_allocable;
    set_memory_limit.rlim_max=max_bytes_allocable;
    
    if(setrlimit(RLIMIT_AS,&set_memory_limit)!=0){
        printf("errore set limit address space\n");
        perror("error:");
        gdb_abort;
    }
}

#if DEBUG==1
void test_memory_limit_mmap(unsigned long byte_to_alloc){
    char*memory_test;
    if ((memory_test = mmap(NULL, (size_t) byte_to_alloc, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, 0, 0)) == MAP_FAILED){
        printf("unable to alloc memory with mmap,test succeed\n");
        perror("error");
    }
    else
        free(memory_test);//free memory if limit not exceed
}


void test_memory_limit_malloc(unsigned long byte_to_alloc){
    char*memory_test=malloc(byte_to_alloc);
    if(memory_test==NULL){
        printf("unable to alloc memory with malloc,test succeed\n");
        perror("error");
    }
    else
        free(memory_test);//free memory if limit not exceed
}

void test_memory_limit(){
    test_memory_limit_malloc(MAX_MEMORY_ALLOCABLE);
    test_memory_limit_mmap(MAX_MEMORY_ALLOCABLE);
}
#endif