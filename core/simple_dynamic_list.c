#include "simple_dynamic_list.h"
#include <stdlib.h>
#include <stdio.h>

int count_element_linked_list(struct node*head){
    int count=0;
    if (head==NULL){
        return count;
    }
    struct node *p=head;
    while(p!=NULL){
        count++;
        p=p->next;
    }
    return count;
}

int insert_first(struct node *new_node, struct node **head){//inserisce il primo nodo
    if(new_node==NULL || head==NULL){
        return -1;
    }
    *head = new_node;
    return 0;
}


//alloca e inizializza un nodo della lista dinamica ordinata,data è un pointer a memoria già allocata
struct node* get_new_node(void*data) {
    if(data==NULL){
        return NULL;
    }
    struct node* new_node = (struct node*)malloc(sizeof(struct node));
    if(new_node==NULL){
        return NULL;
    }
    new_node->data=data;
    new_node->next = NULL;
    return new_node;
}


int insert_at_head(struct node* new_node,struct node** head) {//inserisce un nodo in testa alla lista
    if(head==NULL){
        return -1;
    }
    if(*head == NULL) {
        insert_first(new_node, head);
        return 0;
    }
    new_node->next = *head;
    *head = new_node;
    return 0;
}

void free_memory_list(struct node*head){
    if(head==NULL){
        return;
    }
    struct node*p=head;
    struct node*q;
    while(p!=NULL){
        q=p->next;
        free(p);
        p=q;
    }
    return;
}

void print_list(struct node*head,void(*print_elem)(void*data)){
    if(head==NULL){
        printf("empty list\n");
        return;
    }
    struct node*p=head;
    while(p!=NULL){
        print_elem(p->data);
        p=p->next;
    }
    return;
}