#include "audio.h"

#include <stdio.h>
#include <string.h>

/* Spread midi bursts across callbacks so synthesis gets regular render time. */
enum { MAX_EVENTS_PER_CALLBACK = 64 };

int audio_list(void)
{
    PaDeviceIndex count = Pa_GetDeviceCount();
    if (count < 0) {
        fprintf(stderr, "Cannot list audio devices: %s\n", Pa_GetErrorText(count));
        return -1;
    }
    puts("Audio outputs (INDEX):");
    for (PaDeviceIndex i = 0; i < count; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (info->maxOutputChannels < M1_STEREO) continue;
        printf("  %d  %s [%s], default rate %.0f Hz%s\n", i, info->name,
               Pa_GetHostApiInfo(info->hostApi)->name, info->defaultSampleRate,
               i == Pa_GetDefaultOutputDevice() ? " (system default)" : "");
    }
    return 0;
}

static int callback(const void *input, void *output, unsigned long frames,
                    const PaStreamCallbackTimeInfo *time,
                    PaStreamCallbackFlags flags, void *context)
{
    (void)input;
    (void)time;
    Audio *a = context;
    if (flags & paOutputUnderflow)
        atomic_fetch_add_explicit(&a->underruns, 1, memory_order_relaxed);
    if (atomic_exchange_explicit(&a->queue->panic, false, memory_order_acq_rel)) {
        unsigned head = atomic_load_explicit(&a->queue->head, memory_order_acquire);
        atomic_store_explicit(&a->queue->tail, head, memory_order_release);
        engine_panic(a->engine);
    }
    Event event;
    for (unsigned n = 0; n < MAX_EVENTS_PER_CALLBACK && queue_pop(a->queue, &event); ++n)
        engine_event(a->engine, event);
    if (engine_render(a->engine, output, (int)frames) != FLUID_OK) {
        memset(output, 0, frames * M1_STEREO * sizeof(float));
        atomic_store_explicit(&a->failed, true, memory_order_release);
        return paAbort;
    }
    return paContinue;
}

int audio_open(Audio *a, Engine *engine, EventQueue *queue, const Options *o)
{
    *a = (Audio){ .engine = engine, .queue = queue };
    atomic_init(&a->underruns, 0);
    atomic_init(&a->failed, false);
    if (!atomic_is_lock_free(&queue->head) || !atomic_is_lock_free(&queue->panic) ||
        !atomic_is_lock_free(&a->underruns)) {
        fprintf(stderr, "This platform does not provide lock-free audio atomics.\n");
        return -1;
    }
    PaDeviceIndex device = o->audio_device < 0 ? Pa_GetDefaultOutputDevice() : o->audio_device;
    const PaDeviceInfo *info = Pa_GetDeviceInfo(device);
    if (!info || info->maxOutputChannels < M1_STEREO) {
        fprintf(stderr, "No stereo audio output at index %d; use --list.\n", device);
        return -1;
    }
    PaStreamParameters output = {
        .device = device, .channelCount = M1_STEREO, .sampleFormat = paFloat32,
        .suggestedLatency = info->defaultLowOutputLatency
    };
    PaError error = Pa_IsFormatSupported(NULL, &output, o->sample_rate);
    if (error != paFormatIsSupported) goto fail;
    error = Pa_OpenStream(&a->stream, NULL, &output, o->sample_rate,
                          (unsigned long)o->buffer_frames, paNoFlag, callback, a);
    if (error != paNoError) goto fail;
    error = Pa_StartStream(a->stream);
    if (error != paNoError) goto fail;
    printf("Audio output: %s, %d Hz, %d frames, reported output latency %.1f ms\n",
           info->name, o->sample_rate, o->buffer_frames,
           Pa_GetStreamInfo(a->stream)->outputLatency * 1000.0);
    return 0;
fail:
    fprintf(stderr, "Cannot start audio: %s (try --rate 44100 or --buffer 256).\n",
            Pa_GetErrorText(error));
    audio_close(a);
    return -1;
}

void audio_close(Audio *a)
{
    if (!a->stream) return;
    PaError active = Pa_IsStreamActive(a->stream);
    if (active == 1) Pa_StopStream(a->stream);
    Pa_CloseStream(a->stream);
    a->stream = NULL;
}
