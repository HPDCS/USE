#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "application.h"

static double ta;
static int complete_calls;
static double ref_ta;
static double ta_durata;
static double ta_cambio;
int channels_per_cell;
bool power_management;
static bool variable_ta;
static bool fading_recheck;
static int complete_time;

float handoff_counter;


void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void * ptr) {
	int w;

	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.second_cell = -1;
	new_event_content.channel = -1;
	new_event_content.call_term_time = -1;
	
	simtime_t handoff_time;
	simtime_t timestamp=0;
	
	lp_state_type * pointer;
	pointer = (lp_state_type*)ptr;

	switch(event_type) {
		
		case INIT:
		
			pointer = (lp_state_type *)malloc(sizeof(lp_state_type));
			if (pointer == NULL){
				printf("ERROR in malloc!\n");
				exit(EXIT_FAILURE);
			}

		
			SetState(pointer);
			
//			pointer->channels = NULL;
			pointer->contatore_canali = CHANNELS_PER_CELL;
			pointer->cont_chiamate_entranti = -1;
			pointer->cont_chiamate_complete = 0;
			pointer->cont_handoff_uscita = 0;
			pointer->cont_bloccate_in_partenza = 0;
			pointer->cont_bloccate_in_handoff  = 0;
			pointer->handoffs_entranti = 0;
//			power_management = true;
//			variable_ta = true;
//			fading_recheck = true;

			// INIT is not considered as an event
			pointer->contatore_eventi = 0;
//			pointer->time = now;


			// Load the predefined values
//			variable_ta = true;
			complete_calls = COMPLETE_CALLS;
			ta = TA;
			ta_durata = TA_DURATA;
			ta_cambio = TA_CAMBIO;
			channels_per_cell = CHANNELS_PER_CELL;
			power_management = true;

			// Read runtime parameters
			char **arguments = (char **)event_content;
			for(w = 0; w < size; w += 2) {

				if(strcmp(arguments[w],"complete-calls") == 0) {
					complete_calls = parseInt(arguments[w + 1]);
				} else if(strcmp(arguments[w],"ta") == 0) {
					ta = parseDouble(arguments[w + 1]);
				} else if(strcmp(arguments[w],"ta-durata") == 0) {
					ta_durata = parseDouble(arguments[w + 1]);
				} else if(strcmp(arguments[w],"ta-cambio") == 0) {
					ta_cambio = parseDouble(arguments[w + 1]);
				} else if(strcmp(arguments[w],"channels-per-cell") == 0) {
					channels_per_cell = parseInt(arguments[w + 1]);
				} else if(strcmp(arguments[w],"complete-time") == 0) {
					complete_time = parseInt(arguments[w + 1]);
				} else if(strcmp(arguments[w],"no-power-management") == 0) {
					w -= 1;
					power_management = false;
				} else if(strcmp(arguments[w],"power-management") == 0) {
					w -= 1;
					power_management = true;
				} else if(strcmp(arguments[w],"variable-ta") == 0) {
					w -= 1;
					variable_ta = true;
				} else if(strcmp(arguments[w],"fading-recheck") == 0) {
					fading_recheck = true;
				} else if(strcmp(arguments[w],"complete-time") == 0) {
					complete_time = parseInt(arguments[w + 1]);
				}
			}
			ref_ta = ta;

			// Show current configuration, only once
			if(me == 0) {
				printf("CURRENT CONFIGURATION:\nCOMPLETE CALLS: %d\nTA: %f\nTA_DURATA: %f\nTA_CAMBIO: %f\nCHANNELS_PER_CELL: %d\nCOMPLETE_TIME: %d\n",
					complete_calls, ta, ta_durata, ta_cambio, channels_per_cell, complete_time);
				printf("POWER MANAGMENT: %d\nFADING RECHECK: %d\nVARIABLE TA: %d\n",
					power_management, fading_recheck, variable_ta);
				fflush(stdout);
			}

			pointer->contatore_canali = channels_per_cell;

			for (w = 0; w < pointer->contatore_canali / (sizeof(int) * 8) + 1; w++)
				pointer->channel_state[w] = 0;
			
			pointer->buff_topology = (_PCS_routing*)malloc(sizeof(_PCS_routing));
			if(pointer->buff_topology == NULL){
				printf("Chiamata a malloc errata sulla topologia della rete!\n");
				exit(EXIT_FAILURE);
			}
	
			set_my_topology(me, pointer->buff_topology);

			timestamp = (simtime_t) (20 * Random());	
			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);
			
			if (fading_recheck) {
				timestamp = now + (simtime_t) (FADING_RECHECK_FREQUENCY); 
				ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
			}

			break;

	
		case START_CALL:
			pointer->time = now;
			

			//make_copy(pointer->cont_chiamate_entranti, pointer->cont_chiamate_entranti+1);
			pointer->cont_chiamate_entranti++;
			//make_copy(pointer->contatore_eventi, pointer->contatore_eventi+1);
			pointer->contatore_eventi++;
			
			if (pointer->contatore_canali == 0) {
				//make_copy(pointer->cont_bloccate_in_partenza, pointer->cont_bloccate_in_partenza+1);
				pointer->cont_bloccate_in_partenza++;
			} else {
						
				pointer->contatore_canali--;
				//make_copy(pointer->contatore_canali, pointer->contatore_canali-1);
				
				#ifdef ACCURATE_SIMULATION
				new_event_content.channel = allocation(me, pointer);
				#endif
				
				// Determine call duration
				switch (DISTRIBUZIONE_DURATA) {
	
					case UNIFORME:
						new_event_content.call_term_time = now+
			       			(simtime_t) (ta_durata * Random());

						break;

					case ESPONENZIALE:
						new_event_content.call_term_time = now +
 						(simtime_t)( Expent(ta_durata ));

						break;

					default:
								
 						new_event_content.call_term_time = now+
						(simtime_t) (5 * Random() );	
						}
			
				// Determine whether the call will be handed-off or not
				switch (DISTRIBUZIONE_CAMBIOCELLA) {

					case UNIFORME:
						
						handoff_time  = now+ 
			       			(simtime_t) ((ta_cambio) * Random() );
						break;

					case ESPONENZIALE:
						handoff_time = now+ 
			       			(simtime_t)( Expent( ta_cambio ));
						break;

					default:
						handoff_time = now+ 
			       			(simtime_t) (5 * Random() );
					
				}
			
				if( new_event_content.call_term_time <=  handoff_time) {
				    ScheduleNewEvent(me,new_event_content.call_term_time,END_CALL,&new_event_content,sizeof(new_event_content));

				} else {
					new_event_content.cell = __FindReceiver(me,pointer);
							
					new_event_content.second_cell = -1;
			
					ScheduleNewEvent(me,handoff_time,HANDOFF_LEAVE,&new_event_content,sizeof(new_event_content));

//					#ifdef PRE_SCHEDULING
					new_event_content.call_term_time = new_event_content.call_term_time;
					ScheduleNewEvent(new_event_content.cell,handoff_time,HANDOFF_RECV,&new_event_content,sizeof(new_event_content));
//					#endif
				}
			} // if (pointer->contatore_canali == 0) 


			if (variable_ta)
				ta = recompute_ta(ref_ta, now);

			// Determine the time at which the call will end						
			switch (DISTRIBUZIONE) {   

				case UNIFORME:
					timestamp= now+ 
			   		(simtime_t) (ta * Random() );
					break;
	
				case ESPONENZIALE:
					timestamp= now+ 
			   		(simtime_t)( Expent( ta ));
					break;

				default:
					timestamp= now+	
			   		(simtime_t) (5 * Random());		
						
			}

			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);

			break;

		case END_CALL:
		
			pointer->time = now;
//			make_copy(pointer->contatore_eventi, pointer->contatore_eventi+1);
			pointer->contatore_eventi++;
			//make_copy(pointer->contatore_canali, pointer->contatore_canali+1);
			pointer->contatore_canali++;
			//make_copy(pointer->cont_chiamate_complete, pointer->cont_chiamate_complete+1);
			pointer->cont_chiamate_complete++;
			#ifdef ACCURATE_SIMULATION
			deallocation(me, pointer, event_content->channel);
			#endif
			
			break;

		case HANDOFF_LEAVE:

			pointer->time = now;
			//make_copy(pointer->contatore_eventi, pointer->contatore_eventi+1);
			pointer->contatore_eventi++;
			//make_copy(pointer->contatore_canali, pointer->contatore_canali+1);
			pointer->contatore_canali++;
			//make_copy(pointer->cont_handoff_uscita, pointer->cont_handoff_uscita+1);
			pointer->cont_handoff_uscita++;
			#ifdef ACCURATE_SIMULATION
			deallocation(me, pointer, event_content->channel);
			#endif
			
//			#ifndef PRE_SCHEDULING
//			new_event_content.call_term_time =  event_content->call_term_time + 0.00005;
//			ScheduleNewEvent(event_content->cell, now + 0.00003 , HANDOFF_RECV, &new_event_content, sizeof(new_event_content));
//			#endif
			break;

        	case HANDOFF_RECV:
			pointer->time = now;
			//handoff_counter++;
			//make_copy(pointer->handoffs_entranti, pointer->handoffs_entranti+1);
			pointer->handoffs_entranti++;
			//make_copy(pointer->cont_chiamate_entranti, pointer->cont_chiamate_entranti+1);
			//make_copy(pointer->contatore_eventi, pointer->contatore_eventi+1);
			pointer->contatore_eventi++;
			pointer->cont_chiamate_entranti++;
			
			if (pointer->contatore_canali == 0) 
				//make_copy(pointer->cont_bloccate_in_handoff, pointer->cont_bloccate_in_handoff+1);
				pointer->cont_bloccate_in_handoff++;
			else {
				//make_copy(pointer->contatore_canali, pointer->contatore_canali-1);
				pointer->contatore_canali--;
				
				#ifdef ACCURATE_SIMULATION
				new_event_content.channel = allocation(me, pointer);
				#endif
				
				new_event_content.call_term_time = event_content->call_term_time; 
				
				switch (DISTRIBUZIONE_CAMBIOCELLA) {
					case UNIFORME:
						handoff_time  = now+ 
			    			(simtime_t) ((ta_cambio) * Random());
			
						break;
					case ESPONENZIALE:
						handoff_time = now+ 
			    			(simtime_t)( Expent( ta_cambio ));
			
						break;
					default:
						handoff_time = now+ 
			    			(simtime_t) (5 * Random());
				}
				
				
				if (Random() < 0.5) handoff_time *= 10;
				
				if( new_event_content.call_term_time <=  handoff_time ) {
					ScheduleNewEvent(me , new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));


				} else {
					new_event_content.cell = __FindReceiver(me,ptr);
					
					#ifdef  NO_UNCERTAINTY
						new_event_content.second_cell = -1;
					#endif
					ScheduleNewEvent(me , new_event_content.call_term_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));

				}
			}
			

			break;


		case FADING_RECHECK:

//			pointer->time = now;
			if (pointer->check_fading == true) {
				//make_copy(pointer->check_fading, false);
				pointer->check_fading = false;
			} else {
				//make_copy(pointer->check_fading, true);
				pointer->check_fading = true;
			}

			timestamp = now + (simtime_t) (FADING_RECHECK_FREQUENCY);
			ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
				
			break;

      		default: 
			fprintf(stderr, " pointer simulation: error - inconsistent event (me = %d - event type = %d)\n", me, event_type);
			break;
	} // switch(event->type) 
}


int OnGVT(int gid, lp_state_type *snapshot) {

	if (snapshot->cont_chiamate_complete >= complete_calls) 
		return 1;
//	if (complete_time > 0 && snapshot->time >= complete_time) 
//		return 1;
	return 0;
}


