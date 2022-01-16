#ifndef __LOCAL_INDEX_LP_H
#define __LOCAL_INDEX_LP_H


#include "ROOT-Sim.h"
#include <simtypes.h>
#include "events.h"


typedef struct node {
    simtime_t ts;
    msg_t event;
    struct node **next; //for each node keep ana array of next fields for the insertions
} node;

typedef struct skiplist {
    int level; //last level with a non-empty list
    struct node *head;
} skiplist;


/* ---- init and clean functions ---- */
skiplist *skiplist_init(skiplist *list);

void skiplist_free_all(skiplist *list);


/* utilities functions */
int rand_level();


/* ---- skiplist management functions ---- */

node *skiplist_search(skiplist *list, simtime_t ts);

int skiplist_insert(skiplist *list, simtime_t ts, msg_t event);

int skiplist_delete(skiplist *list, simtime_t ts);

void skiplist_free_node(node *x);




#endif