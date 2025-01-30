
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "application.h"
#include "setup.h"
#include "memory.h"

lp_state_type* states[OBJECTS];

#define state states[me]

#define UNBALANCE


//this is a macro for setting up multiple mmapped zones at distance displacement, e.g. 2<<11
#ifdef NUMA_UBIQUITOUS 
#define displacement (1<<21)
#define SET_MEMORY(addr, value, me) \
    do { \
        *(addr) = (value); \
        for (int _i = 1; _i < (MEM_NODES); ++_i){ \
                *((typeof(addr))((char*)(addr) + _i * (displacement))) = *(addr); \
        } \
    } while(0)
#else
#define SET_MEMORY(addr,value, me) *(addr) = (value)
#endif


int add_car(unsigned int me, elem * head, elem * tail, elem * car, double time){

	if(tail->prev == NULL){
		printf("me is %u - null pointer for car insertion at time %e!!\n",me,time);
		exit(EXIT_FAILURE);
	}

	head = head;//bypassing compiler warnings

	SET_MEMORY(&(car->next),tail,me);
	SET_MEMORY(&(car->prev),tail->prev,me);
	SET_MEMORY(&(car->prev->next),car,me);
	SET_MEMORY(&(tail->prev),car,me);
	return 0;
}

elem * del_car(int me, elem * head, elem * tail){
	elem * car = NULL;
	elem * aux = NULL;

	//bypass compiler warnings on unused parameters
	me = me;

	if(head->next == tail){
		return NULL;
	}
	car = head->next;
	aux = head->next->next;
	SET_MEMORY(&(head->next),aux,me);
	SET_MEMORY(&(aux->prev),head,me);
	return car;

}

void traversal(unsigned int me, elem * head, elem * tail){


	int i = 0;
	int randomized = 0;//flag for the randomic car class switch
	int skip = 0;

	if(tail->prev == head){
		return;
	}

	tail = tail->prev;

	while(tail->prev != head){
		if (randomized){
			//change of the car class - not yet implemented
		}
		memcpy((char*)&(state->temp_buffer),(char*)tail->prev,sizeof(elem));
		if (!skip && tail->residence < tail->prev->residence){
			//here we use per NUMA nodes temporary buffers
			memcpy((char*)&(state->temp_buffer),(char*)tail->prev,sizeof(elem));
			SET_MEMORY(&(tail->prev->residence),tail->residence,me);
			SET_MEMORY(&(tail->prev->type),tail->type,me);
			memcpy(&(tail->prev->buff),&(tail->buff),CAR_INFO_SIZE);
			SET_MEMORY(&(tail->residence),(state->temp_buffer.residence),me);
			SET_MEMORY(&(tail->type),(state->temp_buffer.type),me);
			memcpy(&(tail->buff),&(state->temp_buffer.buff),CAR_INFO_SIZE);
		}
		else{
			skip = 1;
		}
		tail = tail->prev;
		i++;
	}
}

//callback function for processing an event at an object
void ProcessEvent(unsigned int me, double now, int event_type, void *event_content, unsigned int size, void *ptr) {

	double timestamp;
	double scaling;
	uint32_t *s1, *s2;
	unsigned int dest;
	int i;
	int temp;
	enum car_type type;
	double car_type_probability;
	elem * car;

	//just bypassing compile time indications
	//on unused parameters
	event_content = event_content;
	ptr = ptr;
	size = size;

	//printf("object %d - executing event %d at time %f\n", me, event_type, now);

	if (state){
		s1 = &(state->seed1);
		s2 = &(state->seed2);
	}

	switch(event_type) {

		case INIT:

			// Initialize the LP's state
			state  = (lp_state_type *) malloc(sizeof(lp_state_type));
			if (state == NULL){
				printf("out of memory at startup\n");
				exit(EXIT_FAILURE);
			}

			state->event_count = 0;

			s1 = &(state->seed1);
			s2 = &(state->seed2);
			*s1 = (0x01 * me) + 1;
			*s2 = *s1 ^ *s1;

			state->right_head.next = &(state->right_tail);// setup initial double linked list
			state->right_head.prev = NULL;// setup initial double linked list

        		state->right_tail.prev = &(state->right_head);
        		state->right_tail.next = NULL;

			state->left_head.next = &(state->left_tail);// setup initial double linked list
			state->left_head.prev = NULL;// setup initial double linked list

        		state->left_tail.prev = &(state->left_head);
			state->left_tail.next = NULL;// setup initial double linked list

			state->right_load = 0;
			state->left_load = 0;
			
			i = 0;

#ifdef UNBALANCE
if(me < (OBJECTS >> 1)) i = 0; else i = (INITIAL_CARS >> 1); 
#endif
			for (;i<INITIAL_CARS;i++){

				//setup of the initial events 
				timestamp = 0.001 * Expent(TA,s1,s2);

				car_type_probability =  Random(s1,s2);
				if(car_type_probability <= PHIGH) { type = HIGH; goto type_done_right;}
				if(car_type_probability > PHIGH && car_type_probability <= PREGULAR) { type = REGULAR; goto type_done_right;}
				type = LOW;
type_done_right:
				dest = me;
				ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_RIGHT, (char*)&type, sizeof(enum car_type));
//				printf("object %d - scheduled event %d - timestamp is %e - destination is %d\n ", me, CAR_TRAVERSAL_RIGHT, timestamp, dest);
			}

			i = 0; 
#ifdef UNBALANCE
if(me < (OBJECTS >> 1)) i = 0; else i = (INITIAL_CARS >> 1); 
#endif
			for (;i<INITIAL_CARS;i++){

				//setup of the initial events 
				timestamp = 0.001 * Expent(TA,s1,s2);
				car_type_probability =  Random(s1,s2);
				if(car_type_probability <= PHIGH) { type = HIGH; goto type_done_left;}
				if(car_type_probability > PHIGH && car_type_probability <= PREGULAR) { type = REGULAR; goto type_done_left;}
				type = LOW;
type_done_left:
				dest = me;
				ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_LEFT, (char*)&type, sizeof(enum car_type));
//				printf("object %d - scheduled event %d - timestamp is %e - destination is %d\n ", me, CAR_TRAVERSAL_LEFT, timestamp, dest);
			}

#ifdef NUMA_UBIQUITOUS 
			memcpy((char*)state+(1<<21),(char*)state,sizeof(lp_state_type));
#endif

			break;

		case CAR_TRAVERSAL_RIGHT:

			type = *(enum car_type*)event_content;
			car = (elem*)malloc(sizeof(elem));
			if (!car){
				printf("object %d - out of memory\n",me);
				fflush(stdout);
				exit(EXIT_FAILURE);
			}

			SET_MEMORY(&(car->type),type,me);

			temp = state->right_load + 1;
			SET_MEMORY(&(state->right_load),temp,me);

			if (state->right_load <= _130KM_MAX_CAR_COUNT){
				scaling = 1.0;
			}
			else{
				scaling  = (double)_130KM_MAX_CAR_COUNT / (double)state->right_load;
			}	

			timestamp = now + (1/scaling) * Expent(TA,s1,s2);
			if (timestamp < now + LOOKAHEAD) timestamp = now + LOOKAHEAD;

			SET_MEMORY(&(car->residence),timestamp,me);
			add_car(me,&(state->right_head),&(state->right_tail),car, now);
			traversal(me, &(state->right_head),&(state->right_tail));
			ScheduleNewEvent(me, timestamp, CAR_LEAVING_RIGHT, NULL, 0);

			dest = me + 1; 
#ifdef UNBALANCE
			if ((me < (OBJECTS >> 1)) && (dest >= (OBJECTS >> 1))) {
				//you can add whathever probability of routing the car out of the more loaded area
				dest = 0;
			}
			else{
				//you can add whathever probability of routing the car out of the less loaded area
				if (dest >= OBJECTS) dest = OBJECTS >> 1;
			}
			ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_RIGHT, (char*)&type, sizeof(enum car_type));
#else
			if (dest < OBJECTS) {
				ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_RIGHT, (char*)&type, sizeof(enum car_type));
			}
			else{
				dest = 0;
				ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_RIGHT, (char*)&type, sizeof(enum car_type));
			}
#endif

			break;	

		case CAR_LEAVING_RIGHT:
			temp = state->right_load - 1;
			SET_MEMORY(&(state->right_load),temp,me);

#ifndef MODEL_DEBUG
			car = del_car(me,&(state->right_head),&(state->right_tail));
			if (car == NULL){
				printf("object %d - removing car from empty list!!\n",me);
				fflush(stdout);
				exit(EXIT_FAILURE);
			}
			free(car);

			traversal(me, &(state->right_head),&(state->right_tail));
#endif

			break;

		case CAR_TRAVERSAL_LEFT:

			type = *(enum car_type*)event_content;
			car = (elem*)malloc(sizeof(elem));
			if (!car){
				printf("object %d - out of memory\n",me);
				fflush(stdout);
				exit(EXIT_FAILURE);
			}

			SET_MEMORY(&(car->type),type,me);

			temp = state->left_load + 1;
			SET_MEMORY(&(state->left_load),temp,me);

			if (state->right_load <= _130KM_MAX_CAR_COUNT){
		 		scaling = 1.0;
			}
			else{
				scaling  = (double)_130KM_MAX_CAR_COUNT / (double)state->left_load;
			}

			timestamp = now + (1/scaling) * Expent(TA,s1,s2);
			if (timestamp < now + LOOKAHEAD) timestamp = now + LOOKAHEAD;
			SET_MEMORY(&(car->residence),timestamp,me);
			add_car(me,&(state->left_head),&(state->left_tail),car, now);
			traversal(me,&(state->left_head),&(state->left_tail));
			ScheduleNewEvent(me, timestamp, CAR_LEAVING_LEFT, NULL, 0);

			temp = (int)me - 1; 
			if (temp > -1) {
				dest = (unsigned int) temp;
				ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_LEFT, (char*)&type, sizeof(enum car_type));
			}
			else{
				dest = OBJECTS - 1;
				ScheduleNewEvent(dest, timestamp, CAR_TRAVERSAL_LEFT, (char*)&type, sizeof(enum car_type));
			}

			break;

		case CAR_LEAVING_LEFT:
			temp = state->left_load - 1;
			SET_MEMORY(&(state->left_load),temp,me);

			car = del_car(me,&(state->left_head),&(state->left_tail));
			if (car == NULL){
				printf("object %d - removing car from empty list!!\n",me);
				fflush(stdout);
				exit(EXIT_FAILURE);
			}
			free(car);

			traversal(me,&(state->left_head),&(state->left_tail));

			break;

		default:
			printf("highway: unknown event type! (me = %d - event type = %d)\n", me, event_type);
			exit(EXIT_FAILURE);

	}

	SET_MEMORY(s1,*s1,me);
	SET_MEMORY(s2,*s2,me);
	temp = state->event_count + 1;
	SET_MEMORY(&(state->event_count),temp,me);
}
