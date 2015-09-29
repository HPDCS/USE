#include <ROOT-Sim.h>
#include <stdio.h>
#include <limits.h>

#include "application.h"



void *states[4096];
unsigned int num_cells;

#include "agent.h"
#include "cell.h"

#define getPar(args, i) (((char **)args)[(i)])



static int seek_param(void **args, char *name) {
	int i = 0;
	bool parse_end = false;

	while(true) {

		if(getPar(args, i) == NULL) {
			parse_end = true;
			break;
		}

		if(strcmp(getPar(args, i), name) == 0) {
			break;
		}

		i++;
	}

	if(parse_end) {
		i = -1;
	}

	return i;
}


int GetParameterInt(void *args, char *name) {

	int i = seek_param(args, name);

	if(i == -1) {
		rootsim_error(false, "Parameter %s not set, returning -1...\n", name);
		return -1;
	}

	return parseInt(getPar(args, i+1));
}

bool IsParameterPresent(void *args, char *name) {
	return (seek_param(args, name) != -1);
}

void ProcessEvent(int me, simtime_t now, int event_type, void *event_content, int event_size, void *pointer) {

	if(event_type == INIT) {
		
		if(!IsParameterPresent(event_content, "num_cells")) {
			printf("You must specify the number of cells (num_cells)\n");
		}
		num_cells = GetParameterInt(event_content, "num_cells");

		if(num_cells >= n_prc_tot) {
			printf("With %d cells I need at least %d LPs\n", num_cells, num_cells + 1);
		}
	}


	if(is_agent(me)) {
		AgentProcessEvent(me, now, event_type, event_content, event_size, pointer);
	} else {
		CellProcessEvent(me, now, event_type, event_content, event_size, pointer);
	}
}


int OnGVT(unsigned int me, void *snapshot) {

	if(is_agent(me)) {
		return AgentOnGVT(me, snapshot);
	} else  {
		return CellOnGVT(me, snapshot);
	}

	return true;
}
