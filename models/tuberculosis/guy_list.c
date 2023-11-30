#include "guy.h"
#include "t_bitmap.h"
#include <math.h>
#include <assert.h>


bool is_sick(guy_t *guy) {
	if (bitmap_check(guy->flags, f_sick) && !bitmap_check(guy->flags, f_treatment)) {
		return true;
	} else {
		return false;
	}
}

bool is_treatment(guy_t *guy) {
	if (bitmap_check(guy->flags, f_sick) && bitmap_check(guy->flags, f_treatment)) {
		return true;
	}else {
		return false;
	}
}

bool is_infected(guy_t *guy) {
	if (!bitmap_check(guy->flags, f_sick) && !bitmap_check(guy->flags, f_treatment)) {
		return true;
	}else {
		return false;
	}
}

bool is_treated(guy_t *guy) {
	if (!bitmap_check(guy->flags, f_sick) && bitmap_check(guy->flags, f_treatment)) {
		return true;
	}else {
		return false;
	}
}


double scan_list(guy_t *node, int n, double avg, int bound, int err) {

	long sum = 0, c = 0; 
	double variance = 0.0; 
	(void)n;(void)err;

	while (node != NULL) {
		sum = sum + pow((c-avg), 2);
		node = node->next;
		//if (iter > bound) printf("[lp %d] Scan (%d) over bound (%d) - %d\n", current_lp, iter, bound, err);
		if (c > bound*1) break;
		c++;
	}

	//printf("c %d\n", c);
	return variance; 
}


void scan_list_for_stats(guy_t *node, region_t *region, unsigned int type) {

	double old_avg_sick = region->avg_sick;
	double old_avg_infected = region->avg_infected;
	double old_avg_treatment = region->avg_treatment;
	double old_avg_treated = region->avg_treated;
	int prev_count_all = region->count_all;

	assert(node != NULL);

	switch (type) {
		case INSERT:

			region->count_all++;

			if (is_sick(node)) {

				region->count_sick++;
				region->avg_sick = (old_avg_sick * prev_count_all + 1) / region->count_all;
				//printf("SICK prev_count_all %d - count_all %d - old_avg_sick %f - avg sick %f - count sick %d\n", prev_count_all, region->count_all, old_avg_sick, region->avg_sick, region->count_sick);
				//region->variance_sick = scan_list(node, region->count_all, region->avg_sick, region->count_sick, 1);

			}else if (is_treatment(node)) {

				region->count_treatment++;
				region->avg_treatment = (old_avg_treatment * prev_count_all + 1) / region->count_all;
				//printf("TREATMENT\n");
				//region->variance_treatment = scan_list(node, region->count_all, region->avg_treatment, region->count_treatment, 2);

			}else if (is_infected(node)) {

				region->count_infected++;
				region->avg_infected = (old_avg_infected * prev_count_all + 1) / region->count_all;
				//printf("INFECTED\n");
				//region->variance_infected = scan_list(node, region->count_all, region->avg_infected, region->count_infected, 3);

			}else if (is_treated(node)) {

				region->count_treated++;
				region->avg_treated = (old_avg_treated * prev_count_all + 1) / region->count_all;
				//printf("TREATED\n");
				//region->variance_treated = scan_list(node, region->count_all, region->avg_treated, region->count_treated, 4);

			}
			region->variance_sick = scan_list(node, region->count_all, region->avg_sick, region->count_sick, 1);
			//region->variance_treatment = scan_list(node, region->count_all, region->avg_treatment, region->count_treatment, 2);
			//region->variance_infected = scan_list(node, region->count_all, region->avg_infected, region->count_infected, 3);
			//region->variance_treated = scan_list(node, region->count_all, region->avg_treated, region->count_treated, 4);
		break;

		case REMOVE:

			region->count_all--;

			if (is_sick(node)) {

				region->count_sick--;
				region->avg_sick = (old_avg_sick * prev_count_all - 1) / region->count_all;
				//region->variance_sick = scan_list(node, region->count_all, region->avg_sick, region->count_sick, 5);

			}else if (is_treatment(node)) {

				region->count_treatment--;
				region->avg_treatment = (old_avg_treatment * prev_count_all - 1) / region->count_all;
				//region->variance_treatment = scan_list(node, region->count_all, region->avg_treatment, region->count_treatment, 6);

			}else if (is_infected(node)) {

				region->count_infected--;
				region->avg_infected = (old_avg_infected * prev_count_all - 1) / region->count_all;
				//region->variance_infected = scan_list(node, region->count_all, region->avg_infected, region->count_infected, 7);

			}else if (is_treated(node)) {

				region->count_treated--;
				region->avg_treated = (old_avg_treated * prev_count_all - 1) / region->count_all;
				//region->variance_treated = scan_list(node, region->count_all, region->avg_treated, region->count_treated, 8);

			}

			region->variance_sick = scan_list(node, region->count_all, region->avg_sick, region->count_sick, 5);
			//region->variance_treatment = scan_list(node, region->count_all, region->avg_treatment, region->count_treatment, 6);
			//region->variance_infected = scan_list(node, region->count_all, region->avg_infected, region->count_infected, 7);
			//region->variance_treated = scan_list(node, region->count_all, region->avg_treated, region->count_treated, 8);

		break;

		default:
			break;
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

	assert(*next == NULL);

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



