#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "freq_gen.h"
#include "types.h"

#define F_COUNT 100000
#define COUNT_1MS 100

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

struct note {
	struct list_head node;
	uint32_t timestamp;
	uint32_t lambda;
	uint32_t duration;
};

struct freq_source {
	struct source base;

	uint32_t timestamp;
	int pin;
	bool high;

	struct note *current;
	struct list_head notes;
};

void list_init(struct list_head *list) {
	list->next = list->prev = list;
}

bool list_empty(struct list_head *list) {
	return (list->next == list) && (list->prev == list);
}

void list_add(struct list_head *list, struct list_head *node) {
	node->next = list->next;
	node->prev = list;
	list->next->prev = node;
	list->next = node;
}

struct list_head *list_del(struct list_head *node) {
	node->prev->next = node->next;
	node->next->prev = node->prev;

	node->next = node->prev = NULL;
	return node;
}

static uint32_t advance(struct freq_source *f, uint32_t amount) {
	f->timestamp += amount;
	return amount;
}

static uint32_t freq_gen_event(struct source *s, struct event *ev) {
	struct freq_source *f = (struct freq_source *)s;
	uint32_t delay = COUNT_1MS;

	if (!f->current) {
		if (list_empty(&f->notes)) {
			return advance(f, delay);
		}

		f->current = (struct note *)list_del(f->notes.next);
	}

	if (f->timestamp < f->current->timestamp) {
		return advance(f, f->current->timestamp - f->timestamp);
	}

	if (f->high) {
		ev->falling |= (1 << f->pin);
		f->high = false;
		if (f->current->duration < f->current->lambda) {
			// Make sure we finish with low
			f->current->duration = 0;
		}
	} else {
		ev->rising |= (1 << f->pin);
		f->high = true;
	}

	delay = f->current->lambda / 2;

	if (f->current->duration < delay) {
		free(f->current);
		f->current = NULL;
	} else {
		f->current->duration -= delay;
	}

	return advance(f, delay);
}

void freq_source_add_note(struct source *s, uint32_t timestamp, int note, uint32_t duration)
{
	struct freq_source *f = (struct freq_source *)s;
	struct note *node = calloc(1, sizeof(*node));

	node->timestamp = timestamp;
	node->lambda = 100;
	node->duration = duration;

	list_add(f->notes.prev, &node->node);
}

struct source *freq_source_create(int pin)
{
	struct freq_source *f = calloc(1, sizeof(*f));

	f->base.gen_event = freq_gen_event;
	f->pin = pin;
	list_init(&f->notes);

	return &f->base;
}
