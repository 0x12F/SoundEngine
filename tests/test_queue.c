#include "events.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

enum { STRESS_EVENTS = 500000 };
static EventQueue queue;

static void *produce(void *unused)
{
    (void)unused;
    for (int i = 0; i < STRESS_EVENTS; ++i) {
        Event event = {NOTE_ON, i % 16, i % 128, i};
        while (!queue_push(&queue, event)) sched_yield();
    }
    return NULL;
}

int main(void)
{
    queue_init(&queue);
    Event event;
    assert(!queue_pop(&queue, &event));
    for (int i = 0; i < EVENT_CAPACITY - 1; ++i)
        assert(queue_push(&queue, (Event){NOTE_ON, 0, i, 100}));
    assert(!queue_push(&queue, (Event){NOTE_OFF, 0, 60, 0}));
    assert(atomic_load(&queue.panic));
    assert(atomic_load(&queue.dropped) == 1);
    for (int i = 0; i < EVENT_CAPACITY - 1; ++i) {
        assert(queue_pop(&queue, &event));
        assert(event.key == i);
    }
    assert(!queue_pop(&queue, &event));
    queue_init(&queue);
    pthread_t producer;
    assert(pthread_create(&producer, NULL, produce, NULL) == 0);
    for (int i = 0; i < STRESS_EVENTS; ++i) {
        while (!queue_pop(&queue, &event)) sched_yield();
        assert(event.type == NOTE_ON && event.value == i);
        assert(event.channel == i % 16 && event.key == i % 128);
    }
    assert(pthread_join(producer, NULL) == 0);
    assert(!queue_pop(&queue, &event));
    puts("Queue: empty/full, overflow recovery request, ordering and threaded wraparound passed.");
    return 0;
}
