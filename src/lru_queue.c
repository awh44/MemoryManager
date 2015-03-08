#include <stdio.h>
#include <stdlib.h>
#include "../include/lru_queue.h"

void lru_queue_initialize(lru_queue_t *queue)
{
	//create an empty/"artificial" head, to make insertion and deletion much simpler
	queue->head = malloc(sizeof *queue->head);
	queue->head->next = NULL;
	queue->head->prev = NULL;
	queue->tail = queue->head;
}


void lru_queue_uninitialize(lru_queue_t *queue)
{
	while (!lru_queue_empty(queue))
	{
		lru_queue_remove(queue);
	}

	//free the artificial head
	free(queue->head);
}

void lru_queue_insert_new(lru_queue_t *queue, int data)
{
	lru_node_t *node = malloc(sizeof *node);
	node->data = data;
	node->next = NULL;
	node->prev = queue->head;
	queue->head->next = node;
	queue->head = node;
}

void lru_queue_update_existing(lru_queue_t *queue, int data)
{
	if (queue->head->data == data)
	{
		return;
	}

	lru_node_t *curr = queue->head->prev;
	while (curr->data != data)
	{
		curr = curr->prev;
	}

	curr->prev->next = curr->next;
	curr->next->prev = curr->prev;
	curr->prev = queue->head;
	queue->head->next = curr;
	curr->next = NULL;
	queue->head = curr;
}

void lru_queue_remove(lru_queue_t *queue)
{
	lru_node_t *node = queue->tail;
	queue->tail = node->next;
	queue->tail->prev= NULL;
	free(node);
}

int lru_queue_get(lru_queue_t *queue)
{
	return queue->tail->next->data;
}

int lru_queue_poll(lru_queue_t *queue)
{
	int data = lru_queue_get(queue);
	lru_queue_remove(queue);
	return data;
}

unsigned short lru_queue_empty(lru_queue_t *queue)
{
	return queue->head == queue->tail;
}
