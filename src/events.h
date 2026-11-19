#ifndef M1_EVENTS_H
#define M1_EVENTS_H

#include <stdbool.h>
#include <stdatomic.h>

enum { EVENT_CAPACITY = 1024 };
typedef enum { NOTE_ON, NOTE_OFF, CONTROL, PITCH_BEND, PRESSURE, KEY_PRESSURE } EventType;
typedef struct { EventType type; int channel; int key; int value; } Event;

/* One midi producer, one audio consumer. full queues request a pedal or note reset. */
typedef struct {
    Event events[EVENT_CAPACITY];
    atomic_uint head;
    atomic_uint tail;
    atomic_uint dropped;
    atomic_bool panic;
} EventQueue;

static inline void queue_init(EventQueue *q)
{
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->dropped, 0);
    atomic_init(&q->panic, false);
}

static inline bool queue_push(EventQueue *q, Event event)
{
    unsigned head = atomic_load_explicit(&q->head, memory_order_relaxed);
    unsigned next = (head + 1) % EVENT_CAPACITY;
    if (next == atomic_load_explicit(&q->tail, memory_order_acquire)) {
        atomic_fetch_add_explicit(&q->dropped, 1, memory_order_relaxed);
        atomic_store_explicit(&q->panic, true, memory_order_release);
        return false;
    }
    q->events[head] = event;
    atomic_store_explicit(&q->head, next, memory_order_release);
    return true;
}

static inline bool queue_pop(EventQueue *q, Event *event)
{
    unsigned tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    if (tail == atomic_load_explicit(&q->head, memory_order_acquire)) return false;
    *event = q->events[tail];
    atomic_store_explicit(&q->tail, (tail + 1) % EVENT_CAPACITY, memory_order_release);
    return true;
}

#endif
