/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include "async_queue.h"

static async_queue_node_t *node_new(void *data) {
	async_queue_node_t *node = malloc(sizeof(*node));
	if(node == NULL) {
		abort();
	}
	node->data = data;
	node->next = NULL;
	return node;
}

GAsyncQueue *g_async_queue_new(void) {
	GAsyncQueue *q = calloc(1, sizeof(*q));
	if(q == NULL || pthread_mutex_init(&q->mutex, NULL) != 0 ||
			pthread_cond_init(&q->cond, NULL) != 0) {
		abort();
	}
	return q;
}

void g_async_queue_unref(GAsyncQueue *q) {
	if(q == NULL) {
		return;
	}
	async_queue_node_t *node = q->head;
	while(node != NULL) {
		async_queue_node_t *next = node->next;
		free(node);
		node = next;
	}
	pthread_cond_destroy(&q->cond);
	pthread_mutex_destroy(&q->mutex);
	free(q);
}

void g_async_queue_push(GAsyncQueue *q, void *data) {
	async_queue_node_t *node = node_new(data);
	pthread_mutex_lock(&q->mutex);
	if(q->tail != NULL) {
		q->tail->next = node;
	} else {
		q->head = node;
	}
	q->tail = node;
	q->length++;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

void g_async_queue_push_front(GAsyncQueue *q, void *data) {
	async_queue_node_t *node = node_new(data);
	pthread_mutex_lock(&q->mutex);
	node->next = q->head;
	q->head = node;
	if(q->tail == NULL) {
		q->tail = node;
	}
	q->length++;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

void *g_async_queue_pop_unlocked(GAsyncQueue *q) {
	async_queue_node_t *node = q->head;
	if(node == NULL) {
		return NULL;
	}
	q->head = node->next;
	if(q->head == NULL) {
		q->tail = NULL;
	}
	q->length--;
	void *data = node->data;
	free(node);
	return data;
}

void *g_async_queue_pop(GAsyncQueue *q) {
	pthread_mutex_lock(&q->mutex);
	while(q->head == NULL) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}
	void *data = g_async_queue_pop_unlocked(q);
	pthread_mutex_unlock(&q->mutex);
	return data;
}

long g_async_queue_length_unlocked(GAsyncQueue *q) {
	return q->length;
}

long g_async_queue_length(GAsyncQueue *q) {
	pthread_mutex_lock(&q->mutex);
	long length = q->length;
	pthread_mutex_unlock(&q->mutex);
	return length;
}

void g_async_queue_lock(GAsyncQueue *q) {
	pthread_mutex_lock(&q->mutex);
}

void g_async_queue_unlock(GAsyncQueue *q) {
	pthread_mutex_unlock(&q->mutex);
}
