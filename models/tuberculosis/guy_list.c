#include "guy.h"
#include "t_bitmap.h"


void scan_list_for_stats(guy_t *head) {

	guy_t *node;
	int count_sick, count_treat;
	int count_all;
	double avg_sick, avg_treat;

	while (!head) {

		if (bitmap_check(head->flags, f_sick)) count_sick++;
		if (bitmap_check(head->flags, f_treatment)) count_treat++;
		count_all++;
		head = head->next;
	}

	avg_sick = count_sick / count_all;
	avg_treat = count_treat / count_all;

}
		head = head->next;
	}
}



/**
 * This method inserts a guy in a list
 * 
 * @param next The reference to the next field of the previous node in the list
 * @param prev The reference to the previous node in the list
 * @param new_guy The node to insert
 */
void insert_guy_in_list(guy_t **next,  guy_t **prev, guy_t *new_guy) {


	new_guy->next = *next;
	new_guy->prev = *prev;
	*prev = new_guy;
	*next = new_guy;

}


/**
 * This method inserts a guy in a list based on the current status of the list
 * if the list is empty performs a first insertion
 * otherwise it always inserts at the bottom of the list
 * 
 * @param next The reference to the head of the list
 * @param prev The reference to the tail of the list
 * @param new_guy The node to insert
 */
void try_to_insert_guy(guy_t **head, guy_t **tail, guy_t *guy) {


	if (*head == NULL) { /// first insertion
		insert_guy_in_list(head, tail, guy);
		//printf("CORRECT FIRST INSERT %d\n ", *head == guy);
	} else { /// append 
		insert_guy_in_list(&((*tail)->next), tail, guy);
		//printf("CORRECT INSERT %d \n ", *tail == guy);
	}
}



/**
 * This method removes a guy from a list
 * 
 * @param pnext The reference to the next field of the previous node in the list
 * @param pprev The reference to the prev field of the successive node in the list
 * @param node The node to remove
 * @return The node removed
 */
guy_t *remove_guy_from_list(guy_t **pnext, guy_t **pprev, guy_t *node) {

	*pnext = node->next;
	*pprev = node->prev;

	return node;


}


/**
 * This method removes a guy from a list based on the relative position
 * of the node in the list
 * 
 * @param node The reference of the node to remove
 * @return The node removed
 */
guy_t *remove_guy(guy_t **node) {

	guy_t *removed;

	if (*node == NULL) {
		return NULL;
	}else if ((*node)->prev == NULL && (*node)->next == NULL) { /// one element
		removed = remove_guy_from_list(&((*node)->next), &((*node)->prev), *node);
	} else if ((*node)->prev == NULL) { /// first element
		removed = remove_guy_from_list(node, &((*node)->next->prev), *node);
	} else if ((*node)->next == NULL) { /// last element
		removed = remove_guy_from_list(&((*node)->prev->next), node, *node);
	} else { /// any element
		//printf("(*node)->prev->next %p --- (*node)->next->prev %p\n", (*node)->prev->next, (*node)->next->prev);
		removed = remove_guy_from_list(&((*node)->prev->next), &((*node)->next->prev), *node);
	}

	return removed;

}



