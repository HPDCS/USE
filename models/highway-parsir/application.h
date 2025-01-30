
#include <stdint.h>
#include <stdlib.h>
//#include <params.h>

#define FLT_EPSILON 1.19209290E-07F // decimal constant

//#define POSITION_FACTOR (100)

typedef double simtime_t;
typedef int bool;

#define false 0
#define true 1

#define _130KM_THRESOLD_DISTANCE (40) // expressed as meters
#define _130KM_ADMITTED_CARS_PER_LINE  (int)(1000/40)
#define LINES (3)
#define _130KM_MAX_CAR_COUNT (_130KM_ADMITTED_CARS_PER_LINE * LINES)
#define SCALING_FACTOR (1.0) // set this as you want
#define INITIAL_CARS   (int)(_130KM_MAX_CAR_COUNT * SCALING_FACTOR)

#define TA (LOOKAHEAD*12)//here we are just halfing the below mentioned looakhead
//#define TA (LOOKAHEAD*6)//LOOKAHEAD IS EXPRESSING 5 SECONDS and TA is expressing
                        //30 seconds which is the required time for 1KM at about
                        //120 KM per HOUR of SPEED

enum car_type {
    HIGH,  // Default value is 0
    REGULAR,  // Default value is 1
    LOW   // Default value is 2
};

#define PHIGH 	0.33
#define PREGULAR 0.66
#define PLOW	1.0 

#define CAR_INFO_SIZE 256

typedef struct _elem{
	int car_primary_id;
	int car_secondary_id;
	enum car_type type;
	char buff[CAR_INFO_SIZE];
	double residence;
        struct _elem * next;
        struct _elem * prev;
} elem;





/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORM		0
#define EXPONENTIAL	1

//event types
#define CAR_ARRIVAL 1
#define CAR_TRAVERSAL_RIGHT 2
#define CAR_TRAVERSAL_LEFT 3
#define CAR_LEAVING_RIGHT 4
#define CAR_LEAVING_LEFT 5

typedef struct _lp_state_type{
	int event_count;
	uint32_t seed1;//seed passed in input to randomization functions
       	uint32_t seed2;//seed passed in input to randomization functions
	elem right_head;
	elem right_tail;
	int right_load;//this determines the traffic level - right move
	elem left_head;
	elem left_tail;
	int left_load;//this determines the traffic level - left move
	elem temp_buffer;
	elem temp;
//	char temp_buffer[CAR_INFO_SIZE];
//	char temp[CAR_INFO_SIZE];
} lp_state_type;



