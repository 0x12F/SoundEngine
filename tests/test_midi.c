#include "midi.h"

#include <assert.h>
#include <stdio.h>

static void send_event(snd_seq_t *source, int port, Midi *destination,
                       snd_seq_event_t event)
{
    event.source.port = (unsigned char)port;
    event.dest.client = (unsigned char)snd_seq_client_id(destination->seq);
    event.dest.port = (unsigned char)destination->port;
    event.queue = SND_SEQ_QUEUE_DIRECT;
    assert(snd_seq_event_output_direct(source, &event) >= 0);
}

int main(void)
{
    snd_seq_t *source;
    if (snd_seq_open(&source, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        puts("MIDI test skipped: ALSA sequencer unavailable.");
        return 0;
    }
    int port = snd_seq_create_simple_port(source, "Test keyboard",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    assert(port >= 0);
    char address[32];
    snprintf(address, sizeof address, "%d:%d", snd_seq_client_id(source), port);
    Options options = { .midi = address, .channel = 1 };
    Midi midi;
    assert(midi_open(&midi, &options) == 0);
    EventQueue queue;
    queue_init(&queue);
    const snd_seq_event_t events[] = {
        {.type = SND_SEQ_EVENT_NOTEON, .data.note = {.channel = 0, .note = 60, .velocity = 100}},
        {.type = SND_SEQ_EVENT_NOTEON, .data.note = {.channel = 1, .note = 61, .velocity = 100}},
        {.type = SND_SEQ_EVENT_CONTROLLER, .data.control = {.channel = 0, .param = 64, .value = 127}},
        {.type = SND_SEQ_EVENT_CONTROLLER, .data.control = {.channel = 0, .param = 0, .value = 12}},
        {.type = SND_SEQ_EVENT_PGMCHANGE, .data.control = {.channel = 0, .value = 7}},
        {.type = SND_SEQ_EVENT_PITCHBEND, .data.control = {.channel = 0, .value = -8192}},
        {.type = SND_SEQ_EVENT_PITCHBEND, .data.control = {.channel = 0, .value = 8191}},
        {.type = SND_SEQ_EVENT_NOTEON, .data.note = {.channel = 0, .note = 60, .velocity = 0}}
    };
    for (size_t n = 0; n < sizeof events / sizeof events[0]; ++n)
        send_event(source, port, &midi, events[n]);
    assert(midi_read(&midi, &queue) == 0);
    Event received;
    const Event expected[] = {
        {NOTE_ON, 0, 60, 100}, {CONTROL, 0, 64, 127},
        {PITCH_BEND, 0, 0, 0}, {PITCH_BEND, 0, 0, 16383}, {NOTE_ON, 0, 60, 0}
    };
    for (size_t n = 0; n < sizeof expected / sizeof expected[0]; ++n) {
        assert(queue_pop(&queue, &received));
        assert(received.type == expected[n].type);
        assert(received.channel == expected[n].channel);
        assert(received.key == expected[n].key);
        assert(received.value == expected[n].value);
    }
    assert(!queue_pop(&queue, &received));
    midi_close(&midi);
    options.velocity = 100;
    assert(midi_open(&midi, &options) == 0);
    const snd_seq_event_t fixed_events[] = {
        {.type = SND_SEQ_EVENT_NOTEON, .data.note = {.channel = 0, .note = 60, .velocity = 16}},
        {.type = SND_SEQ_EVENT_NOTEON, .data.note = {.channel = 0, .note = 64, .velocity = 99}},
        {.type = SND_SEQ_EVENT_NOTEON, .data.note = {.channel = 0, .note = 60, .velocity = 0}},
        {.type = SND_SEQ_EVENT_NOTEOFF, .data.note = {.channel = 0, .note = 64, .velocity = 32}},
        {.type = SND_SEQ_EVENT_CONTROLLER, .data.control = {.channel = 0, .param = 11, .value = 48}},
        {.type = SND_SEQ_EVENT_KEYPRESS, .data.note = {.channel = 0, .note = 64, .velocity = 24}}
    };
    const Event fixed_expected[] = {
        {NOTE_ON, 0, 60, 100}, {NOTE_ON, 0, 64, 100},
        {NOTE_ON, 0, 60, 0}, {NOTE_OFF, 0, 64, 32},
        {CONTROL, 0, 11, 48}, {KEY_PRESSURE, 0, 64, 24}
    };
    for (size_t n = 0; n < sizeof fixed_events / sizeof fixed_events[0]; ++n)
        send_event(source, port, &midi, fixed_events[n]);
    assert(midi_read(&midi, &queue) == 0);
    for (size_t n = 0; n < sizeof fixed_expected / sizeof fixed_expected[0]; ++n) {
        assert(queue_pop(&queue, &received));
        assert(received.type == fixed_expected[n].type);
        assert(received.channel == fixed_expected[n].channel);
        assert(received.key == fixed_expected[n].key);
        assert(received.value == fixed_expected[n].value);
    }
    assert(!queue_pop(&queue, &received));
    assert(snd_seq_disconnect_from(midi.seq, midi.port, snd_seq_client_id(source), port) >= 0);
    assert(midi_read(&midi, &queue) == 0);
    assert(atomic_load(&queue.panic));
    midi_close(&midi);
    snd_seq_close(source);
    puts("MIDI: virtual subscription, channel filter, controller/pitch conversion and disconnect passed.");
    return 0;
}
