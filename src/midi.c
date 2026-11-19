#include "midi.h"

#include <errno.h>
#include <stdio.h>

int midi_list(void)
{
    snd_seq_t *seq;
    int error = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
    if (error < 0) {
        fprintf(stderr, "ALSA MIDI unavailable: %s\n", snd_strerror(error));
        return -1;
    }
    snd_seq_client_info_t *client;
    snd_seq_port_info_t *port;
    snd_seq_client_info_alloca(&client);
    snd_seq_port_info_alloca(&port);
    puts("MIDI inputs (CLIENT:PORT):");
    snd_seq_client_info_set_client(client, -1);
    while (snd_seq_query_next_client(seq, client) >= 0) {
        int id = snd_seq_client_info_get_client(client);
        if (id == SND_SEQ_CLIENT_SYSTEM) continue;
        snd_seq_port_info_set_client(port, id);
        snd_seq_port_info_set_port(port, -1);
        while (snd_seq_query_next_port(seq, port) >= 0) {
            unsigned caps = snd_seq_port_info_get_capability(port);
            unsigned required = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
            if ((caps & required) == required)
                printf("  %d:%d  %s / %s\n", id, snd_seq_port_info_get_port(port),
                       snd_seq_client_info_get_name(client), snd_seq_port_info_get_name(port));
        }
    }
    snd_seq_close(seq);
    return 0;
}

int midi_open(Midi *m, const Options *o)
{
    *m = (Midi){ .channel = o->channel };
    int error = snd_seq_open(&m->seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
    if (error < 0) goto fail;
    error = snd_seq_set_client_name(m->seq, "M1 Sound Engine");
    if (error < 0) goto fail;
    m->port = snd_seq_create_simple_port(m->seq, "Keyboard input",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (m->port < 0) { error = m->port; goto fail; }
    error = snd_seq_connect_from(m->seq, m->port, SND_SEQ_CLIENT_SYSTEM,
                                 SND_SEQ_PORT_SYSTEM_ANNOUNCE);
    if (error < 0) goto fail;
    if (o->midi) {
        snd_seq_addr_t source;
        error = snd_seq_parse_address(m->seq, &source, o->midi);
        if (error < 0) goto fail;
        error = snd_seq_connect_from(m->seq, m->port, source.client, source.port);
        if (error < 0) goto fail;
    }
    printf("MIDI input: %d:%d%s%s\n", snd_seq_client_id(m->seq), m->port,
           o->midi ? " connected to " : "; connect with aconnect or --midi",
           o->midi ? o->midi : "");
    return 0;
fail:
    fprintf(stderr, "Cannot open MIDI input: %s\n", snd_strerror(error));
    midi_close(m);
    return -1;
}

static bool translate(const snd_seq_event_t *in, Event *out)
{
    *out = (Event){0};
    switch (in->type) {
    case SND_SEQ_EVENT_NOTEON: out->type = NOTE_ON; break;
    case SND_SEQ_EVENT_NOTEOFF: out->type = NOTE_OFF; break;
    case SND_SEQ_EVENT_KEYPRESS: out->type = KEY_PRESSURE; break;
    case SND_SEQ_EVENT_CONTROLLER: out->type = CONTROL; break;
    case SND_SEQ_EVENT_PITCHBEND: out->type = PITCH_BEND; break;
    case SND_SEQ_EVENT_CHANPRESS: out->type = PRESSURE; break;
    default: return false;
    }
    if (out->type == NOTE_ON || out->type == NOTE_OFF || out->type == KEY_PRESSURE) {
        out->channel = in->data.note.channel;
        out->key = in->data.note.note;
        out->value = in->data.note.velocity;
    } else {
        out->channel = in->data.control.channel;
        if (in->data.control.param > M1_MIDI_MAX && out->type == CONTROL) return false;
        out->key = out->type == CONTROL ? (int)in->data.control.param : 0;
        out->value = in->data.control.value;
        if (out->type == PITCH_BEND) {
            enum { PITCH_CENTER = 8192, PITCH_MAX = 16383 };
            if (out->value < -PITCH_CENTER || out->value > PITCH_MAX - PITCH_CENTER)
                return false;
            out->value += PITCH_CENTER;
        }
    }
    return out->channel >= 0 && out->channel < M1_CHANNELS &&
           out->key >= 0 && out->key <= M1_MIDI_MAX && out->value >= 0 &&
           (out->type == PITCH_BEND || out->value <= M1_MIDI_MAX);
}

/* I hope this thing works. */
int midi_read(Midi *m, EventQueue *queue)
{
    enum { MAX_EVENTS_PER_POLL = 256, BANK_MSB = 0, BANK_LSB = 32 };
    for (int count = 0; count < MAX_EVENTS_PER_POLL; ++count) {
        snd_seq_event_t *input = NULL;
        int result = snd_seq_event_input(m->seq, &input);
        if (result == -EAGAIN) return 0;
        if (result == -ENOSPC) {
            atomic_fetch_add_explicit(&queue->dropped, 1, memory_order_relaxed);
            atomic_store_explicit(&queue->panic, true, memory_order_release);
            continue;
        }
        if (result < 0) {
            fprintf(stderr, "MIDI read failed: %s\n", snd_strerror(result));
            return -1;
        }
        bool disconnected = input->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED &&
            input->data.connect.dest.client == snd_seq_client_id(m->seq) &&
            input->data.connect.dest.port == m->port;
        if (disconnected || input->type == SND_SEQ_EVENT_RESET)
            atomic_store_explicit(&queue->panic, true, memory_order_release);
        Event event;
        bool valid = translate(input, &event);
        snd_seq_free_event(input);
        if (!valid || (m->channel && event.channel != m->channel - 1)) continue;
        if (event.type == CONTROL && (event.key == BANK_MSB || event.key == BANK_LSB))
            continue;
        queue_push(queue, event);
    }
    return 0;
}

void midi_close(Midi *m)
{
    if (m->seq) snd_seq_close(m->seq);
    m->seq = NULL;
}
