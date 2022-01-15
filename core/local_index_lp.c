#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>

#include "local_index_lp.h"


#define SKIPLIST_MAX_LEVEL 32

int init_rng = 0; //to check if the rng has to be set


/* The function initializes the skiplist
@ param: list The skiplist
It return the skiplist on success, NULL on failure
*/
skiplist *skiplist_init(skiplist *list) {
    int i;

    //set seed for rng
    if (!init_rng) {
        srand((unsigned int) time(NULL));
        init_rng = 1;
    }

    node *head = (node *) malloc(sizeof(struct node));
    if (!head) return NULL;
    list->head = head;
    head->ts = (int) INT_MAX;
    head->next = (node **) malloc(
            sizeof(node*) * (SKIPLIST_MAX_LEVEL + 1));
    if (!head->next) return NULL;
    for (i = 0; i <= SKIPLIST_MAX_LEVEL; i++) {
        head->next[i] = list->head;
    }

    list->level = 1;

    return list;
}

/* The function randomically decides if the insertion has to take place
 in different levels other than the bottom one
 It returns the number of levels to visit in insertion mode
*/
int rand_level() {
    int level = 1;
    while (rand() < RAND_MAX / 2 && level < SKIPLIST_MAX_LEVEL)
        level++;
    return level;
}


/* The function iterates over the skiplist from top level to bottom level
 it keeps every previous node's next field to subsequently update it with new value
 @ param: list The skiplist 
 @ param: ts The timestampt of the event to insert used as the key
 @ param: event The event to insert used as the value
 It returns 0 on success, 1 on failure
*/
int skiplist_insert(skiplist *list, simtime_t ts, msg_t event) {

    node *update[SKIPLIST_MAX_LEVEL + 1];
    node *x = list->head;
    int i, level;

    // iterates from list->level to bottom level and keeps previous nodes' next fields 
    // in update[] for the following insertion
    for (i = list->level; i >= 1; i--) {
        while (x->next[i]->ts < ts)
            x = x->next[i];
        update[i] = x;
    }
    x = x->next[1];

    if (ts == x->ts) {
        x->event = event;
        return 0;
    } else {
        level = rand_level();
        if (level > list->level) { 
            for (i = list->level + 1; i <= level; i++) {
                update[i] = list->head;
            }
            list->level = level; //update max level with a non-empty list
        }

        
        x = (node *) malloc(sizeof(node));
        if (!x) return 1;
        x->ts = ts;
        x->event = event;
        x->next = (node **) malloc(sizeof(node*) * (level + 1));
        if (!x->next) return 1;

        // actual insertion
        for (i = 1; i <= level; i++) {
            x->next[i] = update[i]->next[i];
            update[i]->next[i] = x;
        }
    }
    return 0;
}


/* the function iterates over the skiplist from top level to bottom level
 until it finds the node with key ts in the bottom level
 @ param: list The skiplist 
 @ param: ts The timestampt of the event to find used as the key
 It returns the node on success, NULL on failure
*/
node *skiplist_search(skiplist *list, simtime_t ts) {

    node *x = list->head;
    int i;
    for (i = list->level; i >= 1; i--) {
        while (x->next[i]->ts < ts)
            x = x->next[i];
    }
    if (x->next[1]->ts == ts) {
        return x->next[1];
    } else {
        return NULL;
    }
    return NULL;
}


/* The function cleans a node of the list
@ param: x Node to be freed
*/
void skiplist_free_node(node *x) {
    if (x) {
        free(x->next);
        free(x);
    }
}

/* The function iterates over the skiplist from top level to bottom level
 it keeps every previous node's next field to subsequently delete it
 @ param: list The skiplist 
 @ param: ts The timestampt of the event to delete used as the key
 It returns 0 on success, 1 on failure
*/
int skiplist_delete(skiplist *list, int ts) {

    int i;
    node *update[SKIPLIST_MAX_LEVEL + 1];
    node *x = list->head;
    for (i = list->level; i >= 1; i--) {
        while (x->next[i]->ts < ts)
            x = x->next[i];
        update[i] = x;
    }
    x = x->next[1];

    if (x->ts == ts) {
        for (i = 1; i <= list->level; i++) {
            if (update[i]->next[i] != x) //if the node is not in the level break
                break;
            update[i]->next[i] = x->next[i]; //actual deletion
        }
        skiplist_free_node(x);

        while (list->level > 1 && list->head->next[list->level]
                == list->head) //if after the deletion there are level with empty lists
            list->level--;
        return 0;
    }
    return 1;
}


/* The function cleans the list
@ param: list Skiplist to be freed
*/
void skiplist_free_all(skiplist *list) {
    node *current_node = list->head->next[1];
    while(current_node != list->head) {
        node *next_node = current_node->next[1];
        free(current_node->next);
        free(current_node);
        current_node = next_node;
    }
    free(current_node->next);
    free(current_node);
    free(list);
}



