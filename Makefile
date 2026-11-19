CC ?= cc
PKGS = fluidsynth portaudio-2.0 alsa sndfile
CPPFLAGS += -Isrc $(shell pkg-config --cflags $(PKGS))
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDLIBS += $(shell pkg-config --libs $(PKGS))
SOURCES = src/m1.c src/options.c src/engine.c src/midi.c src/audio.c src/render.c
OBJECTS = $(SOURCES:src/%.c=build/%.o)

.PHONY: all clean test
all: build/m1
build/m1: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
build/test_queue: tests/test_queue.c src/events.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -pthread -o $@
build/test_engine: tests/test_engine.c build/engine.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
build/test_midi: tests/test_midi.c build/midi.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
build/test_audio: tests/test_audio.c src/audio.c src/audio.h src/engine.h src/events.h src/options.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@
test: build/m1 build/test_queue build/test_engine build/test_midi build/test_audio
	./build/test_queue
	./build/test_midi
	./build/test_audio
	python3 tests/integration.py
clean:
	rm -rf build
-include $(OBJECTS:.o=.d)
