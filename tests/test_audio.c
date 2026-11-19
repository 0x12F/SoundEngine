/* Exercise the real callback without opening hardware or loading samples. */
#include "../src/audio.c"

#include <assert.h>

static unsigned events_seen, renders, panics;
static bool refill;
static EventQueue *active_queue;
static Event received[EVENT_CAPACITY];

void engine_event(Engine *engine, Event event)
{
    (void)engine;
    assert(events_seen < EVENT_CAPACITY);
    received[events_seen++] = event;
    /* A producer replenishing the queue must not postpone audio rendering. */
    if (refill) assert(queue_push(active_queue, event));
}

void engine_panic(Engine *engine)
{
    (void)engine;
    ++panics;
}

int engine_render(Engine *engine, float *stereo, int frames)
{
    (void)engine;
    ++renders;
    memset(stereo, 0, (size_t)frames * M1_STEREO * sizeof *stereo);
    return FLUID_OK;
}

int main(void)
{
    EventQueue queue;
    queue_init(&queue);
    active_queue = &queue;
    Engine engine = {0};
    Audio audio = { .engine = &engine, .queue = &queue };
    atomic_init(&audio.underruns, 0);
    atomic_init(&audio.failed, false);
    float output[128 * M1_STEREO];

    /* A nearly full queue must drain in order over multiple render calls. */
    for (unsigned i = 0; i < EVENT_CAPACITY - 1; ++i)
        assert(queue_push(&queue, (Event){NOTE_ON, 0, (int)(i % 128), (int)i}));
    while (events_seen < EVENT_CAPACITY - 1) {
        unsigned before = events_seen;
        unsigned previous_renders = renders;
        assert(callback(NULL, output, 128, NULL, 0, &audio) == paContinue);
        assert(events_seen > before && events_seen - before <= 64);
        assert(renders == previous_renders + 1);
    }
    for (unsigned i = 0; i < events_seen; ++i)
        assert(received[i].value == (int)i);
    assert(atomic_load(&queue.dropped) == 0);

    events_seen = 0;
    refill = true;
    assert(queue_push(&queue, (Event){NOTE_OFF, 0, 60, 0}));
    unsigned previous_renders = renders;
    assert(callback(NULL, output, 128, NULL, 0, &audio) == paContinue);
    assert(events_seen > 0 && events_seen <= 64);
    assert(renders == previous_renders + 1);
    refill = false;

    /* Panic bypasses the burst backlog and renders in the same callback. */
    atomic_store(&queue.panic, true);
    events_seen = 0;
    previous_renders = renders;
    assert(callback(NULL, output, 128, NULL, paOutputUnderflow, &audio) == paContinue);
    assert(panics == 1 && events_seen == 0);
    assert(renders == previous_renders + 1);
    assert(atomic_load(&audio.underruns) == 1);
    Event event;
    assert(!queue_pop(&queue, &event));
    puts("Audio: burst budget, FIFO delivery, continuous arrivals and panic passed.");
    return 0;
}
