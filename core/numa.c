#include <numa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hpdcs_utils.h>
unsigned int NUMA_TOPOLOGY[N_CPU];

void set_cores_id_on_numa_topology(unsigned int id_numa_node,unsigned int num_cores_per_numa_node){
	char final_command[256];
	int index=0;
	const char quotes='\"';
	memset(final_command,'\0',256);

	const char*basic_command="numactl --hardware | grep \"node ";
	int len_basic_command=strlen(basic_command);

	memcpy(final_command,basic_command,len_basic_command);
	index+=len_basic_command;

	char core_id_to_string[3]={0,0,0};
	sprintf(core_id_to_string, "%u", id_numa_node);

	memcpy(final_command+index,core_id_to_string,strlen(core_id_to_string));
	index+=strlen(core_id_to_string);

	memcpy(final_command+index,&quotes,1);
	index+=1;

	FILE*fp = popen(final_command, "r");
	if(fp==NULL){
        printf("popen failed in set_cores_id_on_numa_topology\n");
        gdb_abort;
    }
    char break_point=':';
    char character='\0';
    while(character!=break_point){
    	if(fread(&character,1,1,fp)!=1){
    		printf("error in fread set_cores_id_on_numa_topology\n");
    		gdb_abort;
    	}
    }
    //breakpoint readed

	for(unsigned int i=0;i<num_cores_per_numa_node;i++){
		unsigned int core_id;
		if(fscanf(fp,"%u",&core_id)!=1){
			printf("error in fscanf set_cores_id_on_numa_topology\n");
			gdb_abort;
		}
		NUMA_TOPOLOGY[num_cores_per_numa_node*id_numa_node+i]=core_id;
	}
	pclose(fp);
	return;
}

void set_numa_topology(){
	//assume that every numa_node contains same num of cores
	unsigned int num_cores_per_numa_node = N_CPU/N_NUMA_NODES;
	for(unsigned int id_numa_node=0;id_numa_node<N_NUMA_NODES;id_numa_node++){
		set_cores_id_on_numa_topology(id_numa_node,num_cores_per_numa_node);
	}
	for(int i=0;i<N_CPU;i++){
		printf("NUMA_TOPOLOGY i=%d\n",NUMA_TOPOLOGY[i]);
	}
}