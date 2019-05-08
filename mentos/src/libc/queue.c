///                MentOS, The Mentoring Operating system project
/// @file queue.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "queue.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

queue_t queue_create(size_t data_size)
{
	queue_t queue = malloc(sizeof(struct queue_t));
	if (queue == NULL) {
		printf("Cannot create the queue.\n");
		return NULL;
	}
	queue->data_size = data_size;
	queue->front = NULL;
	queue->back = NULL;

	return queue;
}

bool_t queue_destroy(queue_t queue)
{
	if (queue == NULL) {
		printf("Queue is NULL.\n");
		return false;
	}
	queue_clear(queue);
	free(queue);

	return true;
}

bool_t queue_is_empty(queue_t queue)
{
	if (queue == NULL) {
		printf("Queue is NULL.\n");

		return false;
	}

	return (bool_t)(queue->front == NULL);
}

bool_t queue_enqueue(queue_t queue, void *data)
{
	if (queue == NULL) {
		printf("Queue is NULL.\n");

		return false;
	}
	if (data == NULL) {
		printf("Data is NULL.\n");

		return false;
	}
	queue_node_t *node = malloc(sizeof(struct queue_node_t));

	if (node == NULL) {
		printf("New node of the queue is NULL.\n");

		return false;
	}
	// Initialize the new node.
	node->data = data;
	// Since the new node is the tail, nobody is behind it.
	node->next = NULL;
	// If the queue is empty, the data is placed also on the front.
	if (queue->front == NULL) {
		queue->front = node;
	} else {
		queue->back->next = node;
	}
	queue->back = node;

	return true;
}

bool_t queue_dequeue(queue_t queue)
{
	struct queue_node_t *front_node = queue->front;
	if (front_node == NULL) {
		printf("Queue is Empty\n");

		return false;
	}
	if (queue->front == queue->back) {
		queue->front = queue->back = NULL;
	} else {
		queue->front = queue->front->next;
	}
	free(front_node);

	return true;
}

bool_t queue_front(queue_t queue, void *data)
{
	if (queue == NULL) {
		printf("Queue is NULL.\n");

		return false;
	}
	if (queue->front == NULL) {
		printf("Queue is empty\n");

		return false;
	}
	memcpy(data, queue->front->data, queue->data_size);

	return true;
}

bool_t queue_back(queue_t queue, void *data)
{
	if (queue == NULL) {
		printf("Queue is NULL.\n");

		return false;
	}
	if (queue->back == NULL) {
		printf("Queue is empty\n");

		return false;
	}
	memcpy(data, queue->back->data, queue->data_size);

	return true;
}

bool_t queue_front_and_dequeue(queue_t queue, void *data)
{
	if (!queue_front(queue, data)) {
		return false;
	}
	if (!queue_dequeue(queue)) {
		return false;
	}

	return true;
}

bool_t queue_clear(queue_t queue)
{
	if (queue == NULL) {
		printf("Queue is NULL.\n");

		return false;
	}
	if (queue->front == NULL) {
		return true;
	}
	while (queue->front) {
		queue_dequeue(queue);
	}

	return true;
}
