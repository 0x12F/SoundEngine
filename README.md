Play SoundFont instruments from a Kurzweil CUP M1 through a Linux PC.

## Build

Requires a C11 compiler, Make, pkg-config, and development packages for
FluidSynth 2.x, PortAudio, ALSA, and libsndfile.

```sh
make
```

## Use

Connect the piano's USB TO HOST port to your PC and set LOCAL to Off to avoid
hearing its built-in sound too. You'll need an SF2 SoundFont; none are included.

```sh
./build/m1 --list
./build/m1 --soundfont /path/to/soundfont.sf2 --presets
./build/m1 --soundfont /path/to/soundfont.sf2 --midi 24:0 --audio 3 --bank 0 --program 0
```

Replace the MIDI and audio numbers with those from `--list`, and choose bank and
program numbers from `--presets`. Omit `--audio` to use your default output.
Press Ctrl-C to stop. Restore LOCAL to On when playing without the PC.

Run `./build/m1 --help` for all options or `make test` to run tests (requires Python 3).

