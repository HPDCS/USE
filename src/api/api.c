#include <lp/lp.h>
#include <queue.h>
#include <core.h>

extern LP_state **LPS;

unsigned int GetNumaNode(unsigned int lp){return LPS[lp]->numa_node;}

// can be replaced with a macro?
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
		queue_insert(receiver, timestamp, event_type, event_content, event_size);
	}
}