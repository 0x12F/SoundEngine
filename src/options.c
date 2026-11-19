#include "options.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void options_usage(const char *name)
{
    printf("Usage: %s --soundfont FILE [options]\n"
           "  --list                List MIDI inputs and audio outputs\n"
           "  --presets             List the SoundFont's banks and programs\n"
           "  --midi CLIENT:PORT    Subscribe to an ALSA MIDI input\n"
           "  --audio INDEX         PortAudio output (default: system output)\n"
           "  --bank N              SoundFont bank (default: 0)\n"
           "  --program N           SoundFont program, 0..127 (default: 0)\n"
           "  --channel N           Listen to 1..16, or 0 for all (default: 0)\n"
           "  --velocity N          Fixed note velocity, 1..127; 0 uses touch (default: 0)\n"
           "  --rate HZ             Sample rate (default: 48000)\n"
           "  --buffer FRAMES       Callback size (default: 128)\n"
           "  --polyphony N         Sample voices (default: 256)\n"
           "  --gain N              Synth gain, 0..1 (default: 0.3)\n"
           "  --dry                 Disable reverb and chorus\n"
           "  --render FILE.wav     Render a test chord without audio/MIDI hardware\n"
           "  --help                Show this help\n", name);
}

static bool integer(const char *text, int minimum, int maximum, int *value)
{
    char *end;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno || end == text || *end || parsed < minimum || parsed > maximum)
        return false;
    *value = (int)parsed;
    return true;
}

int options_parse(Options *o, int argc, char **argv)
{
    *o = (Options){ .audio_device = -1, .sample_rate = 48000,
        .buffer_frames = 128, .polyphony = 256, .gain = 0.3, .effects = true };
    for (int i = 1; i < argc; ++i) {
        const char *key = argv[i];
        if (!strcmp(key, "--help")) { options_usage(argv[0]); return 1; }
        if (!strcmp(key, "--list")) { o->list = true; continue; }
        if (!strcmp(key, "--presets")) { o->presets = true; continue; }
        if (!strcmp(key, "--dry")) { o->effects = false; continue; }
        if (++i == argc) { fprintf(stderr, "Missing value for %s\n", key); return -1; }
        const char *value = argv[i];
        if (!strcmp(key, "--soundfont")) o->soundfont = value;
        else if (!strcmp(key, "--midi")) o->midi = value;
        else if (!strcmp(key, "--render")) o->render = value;
        else if (!strcmp(key, "--gain")) {
            char *end;
            errno = 0;
            o->gain = strtod(value, &end);
            if (errno || end == value || *end || !isfinite(o->gain) ||
                o->gain < 0 || o->gain > 1) goto invalid;
        } else {
            struct { const char *name; int *target; int min; int max; } fields[] = {
                {"--audio", &o->audio_device, 0, 65535},
                {"--bank", &o->bank, 0, 16383},
                {"--program", &o->program, 0, M1_MIDI_MAX},
                {"--channel", &o->channel, 0, M1_CHANNELS},
                {"--velocity", &o->velocity, 0, M1_MIDI_MAX},
                {"--rate", &o->sample_rate, 8000, 192000},
                {"--buffer", &o->buffer_frames, 32, 4096},
                {"--polyphony", &o->polyphony, 1, 4096}
            };
            bool found = false;
            for (size_t n = 0; n < sizeof fields / sizeof fields[0]; ++n) {
                if (strcmp(key, fields[n].name)) continue;
                found = true;
                if (!integer(value, fields[n].min, fields[n].max, fields[n].target))
                    goto invalid;
                break;
            }
            if (!found) { fprintf(stderr, "Unknown option: %s\n", key); return -1; }
        }
        continue;
invalid:
        fprintf(stderr, "Invalid value for %s: %s\n", key, value);
        return -1;
    }
    if (!o->list && !o->soundfont) {
        fprintf(stderr, "Provide --soundfont FILE; use --help for usage.\n");
        return -1;
    }
    return 0;
}
