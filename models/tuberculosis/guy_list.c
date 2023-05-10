#include "guy.h"


//insert in list
//delete from list



void insert_guy_in_list(guy_t **next,  guy_t **prev, guy_t *new_guy) {


	new_guy->next = *next;
	new_guy->prev = *prev;
	*prev = new_guy;
	*next = new_guy;

}

void try_to_insert_guy(guy_t **head, guy_t **tail, guy_t *guy) {


	if (*head == NULL) { //first insertion
		insert_guy_in_list(head, tail, guy);
	} else { //append 
		insert_guy_in_list(&((*tail)->next), tail, guy);
	}
}





//todo: free the removed nodes
guy_t *remove_guy_from_list(guy_t **pnext, guy_t **pprev, guy_t *node) {

	/*printf(" BEFORE REMOVE NODE %p --- NODE NEXT %p -- NODE PREV %p\n", node, *pnext, *pprev);
	fflush(stdout);*/
	*pnext = node->next;
	*pprev = node->prev;
	/*printf("NODE %p --- NODE NEXT %p -- NODE PREV %p\n", node, *pnext, *pprev);
	fflush(stdout);*/

	return node;


}


guy_t *remove_head_guy_from_list(guy_t **head) {

	guy_t *d_head = *head;

	return remove_guy_from_list(head, &(d_head->next->prev), *head);

}


guy_t *remove_tail_guy_from_list(guy_t **tail) {

	guy_t *d_tail = *tail;

	return remove_guy_from_list(&(d_tail->prev->next), tail, *tail);
}


guy_t *remove_guy(guy_t **node) {

	guy_t *removed;

	if (*node == NULL) {
		printf("node null\n");
		fflush(stdout);
		return NULL;
	}else if ((*node)->prev == NULL && (*node)->next == NULL) { //one element
		/*printf("ONE ELEMENT\n");
		fflush(stdout);*/
		removed = remove_guy_from_list(&((*node)->next), &((*node)->prev), *node);
	} else if ((*node)->prev == NULL) { //first element
		/*printf("HEAD REMOVE → HEAD %p ------ GUY TO REMOVE %p\n", *node, *node);
		fflush(stdout);*/
		removed = remove_guy_from_list(node, &((*node)->next->prev), *node);
		/*printf("AFTER  REMOVE → HEAD %p ------ GUY REMOVED %p\n", *node, removed);
		fflush(stdout);*/
	} else if ((*node)->next == NULL) { //last element
		/*printf("TAIL REMOVE\n");
		fflush(stdout);*/
		removed = remove_guy_from_list(&((*node)->prev->next), node, *node);
	} else { //any element
		/*printf("REMOVE ANY\n");
		fflush(stdout);*/
		removed = remove_guy_from_list(&((*node)->prev->next), &((*node)->next->prev), *node);
	}

	return removed;

}


guy_t *try_to_remove_guy(guy_t **head, guy_t **tail, guy_t *guy) {

	guy_t *removed;

	if (guy == NULL) return NULL;

	if (*head == NULL) { //list empty
		/*printf("EMPTY\n");
		fflush(stdout);*/
		return NULL;
	} else if (*head == *tail) { //one element
		/*printf("ONE ELEMENT\n");
		fflush(stdout);*/
		removed = remove_guy_from_list(head, tail, guy);
	} else if (guy->prev == NULL) { //first element
		/*printf("HEAD REMOVE → HEAD %p ------ GUY TO REMOVE %p\n", *head, guy);
		fflush(stdout);*/
		removed = remove_head_guy_from_list(head);
		/*printf("AFTER  REMOVE → HEAD %p ------ GUY REMOVED %p\n", *head, removed);
		fflush(stdout);*/
	} else if (guy->next == NULL) { //last element
		/*printf("TAIL REMOVE\n");
		fflush(stdout);*/
		removed = remove_tail_guy_from_list(tail);
	} else { //any element
		/*printf("REMOVE ANY\n");
		fflush(stdout);*/
		removed = remove_guy_from_list(&(guy->prev->next), &(guy->next->prev), guy);
	}

	return removed;
}


void printlist(guy_t *node) {

	while(node != NULL) {
		printf("guy %p next %p\n", node, node->next);
		node = node->next;
	}
	printf("\n");
}