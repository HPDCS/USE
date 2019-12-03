
#ifndef IPIDES_SIMPLE_DYNAMIC_LIST_H
#define IPIDES_SIMPLE_DYNAMIC_LIST_H

struct node  {//struttura di un nodo della lista dinamica ordianta,la lista tiene traccia delle trasmissioni e delle ritrasmissioni
    void *data;
    struct node* next;//puntatore al prossimo
};

int insert_first(struct node *new_node, struct node **head);//inserisce il primo nodo
void print_list(struct node*head,void(*print_elem)(void*data));
//alloca e inizializza un nodo della lista dinamica ordinata
struct node* get_new_node(void*data);

int insert_at_head(struct node* new_node,struct node** head);//inserisce un nodo in testa alla lista
int count_element_linked_list(struct node*head);
void free_memory_list(struct node*head);
#endif //IPIDES_SIMPLE_DYNAMIC_LIST_H
