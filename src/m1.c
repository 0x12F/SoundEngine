#define _POSIX_C_SOURCE 200809L
#include "audio.h"
#include "midi.h"
#include "render.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile sig_atomic_t stopping;

static void stop(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

int main(int argc, char **argv)
{
    Options options;
    int parsed = options_parse(&options, argc, argv);
    if (parsed) return parsed > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    Engine engine = {0};
    if (!options.list) {
        if (engine_open(&engine, &options)) return EXIT_FAILURE;
        if (options.presets || options.render) {
            int result = 0;
            if (options.presets) engine_presets(&engine);
            else result = render_demo(&engine, &options);
            engine_close(&engine);
            return result ? EXIT_FAILURE : EXIT_SUCCESS;
        }
    }
    int result = EXIT_FAILURE;
    PaError error = Pa_Initialize();
    if (error != paNoError) {
        fprintf(stderr, "Cannot initialize audio: %s\n", Pa_GetErrorText(error));
        engine_close(&engine);
        return result;
    }
    if (options.list) {
        int audio_result = audio_list();
        int midi_result = midi_list();
        Pa_Terminate();
        return audio_result || midi_result ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    EventQueue queue;
    queue_init(&queue);
    Midi midi = {0};
    Audio audio = {0};
    struct sigaction action = { .sa_handler = stop };
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) {
        perror("Cannot install signal handlers");
        goto done;
    }
    if (midi_open(&midi, &options) || audio_open(&audio, &engine, &queue, &options)) goto done;
    puts("Ready. Ctrl-C to stop.");
    result = EXIT_SUCCESS;
    while (!stopping) {
        if (midi_read(&midi, &queue) || Pa_IsStreamActive(audio.stream) != 1 ||
            atomic_load_explicit(&audio.failed, memory_order_acquire)) {
            fprintf(stderr, "MIDI/audio stream stopped unexpectedly.\n");
            result = EXIT_FAILURE;
            break;
        }
        const struct timespec poll_interval = { .tv_nsec = 1000000 };
        nanosleep(&poll_interval, NULL);
    }
done:
    audio_close(&audio);
    midi_close(&midi);
    if (audio.engine)
        fprintf(stderr, "Audio underruns: %u; dropped MIDI events: %u\n",
                atomic_load(&audio.underruns), atomic_load(&queue.dropped));
    Pa_Terminate();
    engine_close(&engine);
    return result;
}
