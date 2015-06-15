#include <stdio.h>

#include <ROOT-Sim.h>


#define AGENT_ARRIVAL	1
#define AGENT_DEPARTURE	2

#define SIZE 45

#define INITIAL_AGENTS 2

#define INITIAL_JOBS 32
#define INCREMENT 1.0
#define TRACE if(0)
#define THRESHOLD 0.7

#define PROCESSING_TIME 1.0
#define AUDIT_PERIOD    10 // number of server jobs


typedef struct _lp_state_type 
{
  simtime_t current_simulation_time;
  unsigned int agents_in;
  unsigned int agents_passed_through;
  simtime_t lvt;
  
} lp_state_type;

lp_state_type state[SIZE][SIZE];


unsigned  model_seed=464325;


//#define BIAS 0.012
//#define BIAS 0.001
#define BIAS 0.0

#define MAX_REGION	30000
#define MAX_TIME	10.0

typedef struct _model_data
{
  int x;
  int y;
  
} model_data;

#define coord_to_LP(nx, ny) (ny * SIZE + nx)

void audit_all(void){

	int i,j;
	system("clear");
	for (i=0;i<SIZE;i++){
		for(j=0;j<SIZE;j++) 
		{
			if(state[i][j].agents_in > 0) printf("| ");
			else printf("  "); 
		}
		printf("\n");
	}
	printf("\n----------total visits in region---------- \n");
	for (i=0;i<SIZE;i++){
		for(j=0;j<SIZE;j++) 
		{
			printf("%d ",state[i][j].agents_passed_through); 
		}
		printf("\n");
	}
}

static int counter_region = 0;

bool OnGVT(unsigned int me, lp_state_type *snapshot) 
{


  if(snapshot->lvt > MAX_TIME)
	return true;

  if(snapshot->agents_passed_through > MAX_REGION)
    return true;
  return false;
}

void ProcessEvent(unsigned int me, simtime_t now, unsigned int event_type, void *event, unsigned int size, lp_state_type *my_state)
{

  int i, j;
  int x = 0, y = 0;
  int recv;
  model_data new_msg;
  model_data *msg = (model_data*)event;
  int target_center;
  simtime_t new_time;
  double value;
  void *the_state;

  if(my_state != NULL)
	my_state->lvt = now;

    switch(event_type)
    {

      case INIT:
	
	      the_state  = malloc(sizeof(lp_state_type));
		SetState(the_state);
	
	      TRACE
	      printf("INIT on all regions\n");
	      for (i=0;i<SIZE;i++){
		      for(j=0;j<SIZE;j++) {
			      state[x][y].current_simulation_time =  0.0;
			      state[x][y].agents_in = 0;
			      state[x][y].agents_passed_through = 0;
		      }
	      }
               TRACE
	      printf("INIT on all regions done\n");

	      for(i=0;i<INITIAL_AGENTS;i++){
		      new_msg.x = (int)(Expent(/*&model_seed,*/SIZE/2));
		      if(new_msg.x >= SIZE) new_msg.x = SIZE-1;
		      new_msg.y = (int)(Expent(/*&model_seed,*/SIZE/2));
		      if(new_msg.y >= SIZE) new_msg.y = SIZE-1;
		      new_time = now + 0.01*INCREMENT; 
		      TRACE
		      printf("schedule agent in (%d,%d) - time %e\n",new_msg.x,new_msg.y,new_time);
		      recv = coord_to_LP(new_msg.x, new_msg.y);
		      ScheduleNewEvent(recv, new_time, AGENT_ARRIVAL, &new_msg, sizeof(model_data));
	      }
      break;


      case AGENT_ARRIVAL:
	
	  x = msg->x;
	  y = msg->y;
	  state[x][y].current_simulation_time = now;

	      TRACE
	      printf("AGENT ARRIVAL IN region (%d,%d) - time %e - agents in %d - agents passed through %d\n",
			      x,
			      y,
			      state[x][y].current_simulation_time,
			      state[x][y].agents_in,
			      state[x][y].agents_passed_through
		      );
	      
	   /************************/   
	      //counter_region++;
	      my_state->agents_passed_through++;

	      state[x][y].agents_in++;

	      //audit_all();

	      new_msg.x = x;
	      new_msg.y = y;
	      new_time = now + Expent(/*&model_seed,*/INCREMENT); 
	      TRACE
	      printf("schedule agent out (%d,%d) - time %e\n",new_msg.x,new_msg.y,new_time);
	      
	      ScheduleNewEvent(me, new_time, AGENT_DEPARTURE, &new_msg, sizeof(model_data));

      break;


      case AGENT_DEPARTURE:
	
	  x = msg->x;
	  y = msg->y;
	  state[x][y].current_simulation_time = now;

	      state[x][y].agents_in--;
	      state[x][y].agents_passed_through++;

	      if(Expent(/*&model_seed,*/THRESHOLD)>0.5) 
		      new_msg.x = x+1;
	      else
		      new_msg.x = x-1;
		      
	      if(Expent(/*&model_seed,*/THRESHOLD)>0.4) 
		      new_msg.y = y+1;
	      else
		      new_msg.y = y-1;

	      if(new_msg.x <0 || new_msg.x >= SIZE) new_msg.x = x;
	      if(new_msg.y <0 || new_msg.y >= SIZE) new_msg.y = y;
	      new_time = now + 0.001*Expent(/*&model_seed,*/INCREMENT); 

	      TRACE
	      printf("schedule agent in (%d,%d) - time %e\n",new_msg.x,new_msg.y,new_time);
	      recv = coord_to_LP(new_msg.x, new_msg.y);
	      ScheduleNewEvent(recv, new_time, AGENT_ARRIVAL, &new_msg, sizeof(model_data));

      break;



	}
	//audit_all();



}

