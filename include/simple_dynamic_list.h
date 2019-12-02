
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

int remove_after_node(struct node**ppos);
int insert_at_head(struct node* new_node,struct node** head);//inserisce un nodo in testa alla lista
char first_is_smaller(struct node node1, struct node node2);//verifica se il primo nodo contiene tempi più piccoli del secondo nodo

int count_element_linked_list(struct node*head);
int delete_head(struct node** head);//non è importante il valore iniziale di oldhead
void free_memory_list(struct node*head);
#endif //IPIDES_SIMPLE_DYNAMIC_LIST_H
