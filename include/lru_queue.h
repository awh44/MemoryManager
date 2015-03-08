#ifndef _LRU_QUEUE_H_
#define _LRU_QUEUE_H_

typedef struct lru_node_t
{
	int data;
	struct lru_node_t *next;
	struct lru_node_t *prev;
} lru_node_t;

typedef struct
{
	lru_node_t *head;
	lru_node_t *tail;
} lru_queue_t;

void lru_queue_initialize(lru_queue_t *queue);
void lru_queue_uninitialize(lru_queue_t *queue);
void lru_queue_insert_new(lru_queue_t *queue, int data);
void lru_queue_update_existing(lru_queue_t *queue, int data);
void lru_queue_remove(lru_queue_t *queue);
int lru_queue_get(lru_queue_t *queue);
int lru_queue_poll(lru_queue_t *queue);
unsigned short lru_queue_empty(lru_queue_t *queue);
#endif
