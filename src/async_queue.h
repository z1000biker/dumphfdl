/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stddef.h>
#include <pthread.h>

typedef struct async_queue_node {
	void *data;
	struct async_queue_node *next;
} async_queue_node_t;

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	async_queue_node_t *head;
	async_queue_node_t *tail;
	long length;
} GAsyncQueue;

GAsyncQueue *g_async_queue_new(void);
void g_async_queue_unref(GAsyncQueue *q);
void g_async_queue_push(GAsyncQueue *q, void *data);
void g_async_queue_push_front(GAsyncQueue *q, void *data);
void *g_async_queue_pop(GAsyncQueue *q);
void *g_async_queue_pop_unlocked(GAsyncQueue *q);
long g_async_queue_length(GAsyncQueue *q);
long g_async_queue_length_unlocked(GAsyncQueue *q);
void g_async_queue_lock(GAsyncQueue *q);
void g_async_queue_unlock(GAsyncQueue *q);
