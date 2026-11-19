import array
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave


def chunk(tag, data):
    return tag + struct.pack('<I', len(data)) + data + b'\0' * (len(data) % 2)


def make_font(path):
    # A generated loop avoids network downloads and third-party sample licenses.
    rate, length, cycles, guard = 48000, 1200, 11, 46
    pcm = [round(20000 * math.sin(2 * math.pi * cycles * i / length))
           for i in range(length)] + [0] * guard
    samples = struct.pack('<' + 'h' * len(pcm), *pcm)
    info = chunk(b'ifil', struct.pack('<HH', 2, 1))
    info += chunk(b'INAM', b'M1 test tone\0\0')
    info += chunk(b'isng', b'EMU8000\0')
    phdr = struct.pack('<20sHHHIII', b'Test sine', 0, 0, 0, 0, 0, 0)
    phdr += struct.pack('<20sHHHIII', b'EOP', 0, 0, 1, 0, 0, 0)
    bags = struct.pack('<HHHH', 0, 0, 1, 0)
    inst = struct.pack('<20sH20sH', b'Sine', 0, b'EOI', 1)
    # SF2 generators: releaseVolEnv, sampleModes, overridingRootKey, sampleID.
    generators = [(38, -2400), (54, 1), (58, 69), (53, 0)]
    igen = b''.join(struct.pack('<Hh', op, value) for op, value in generators)
    igen += bytes(4)
    ibag = struct.pack('<HHHH', 0, 0, len(generators), 0)
    shdr = struct.pack('<20sIIIIIBbHH', b'Sine', 0, length, 0, length,
                       rate, 69, 0, 0, 1)
    shdr += struct.pack('<20sIIIIIBbHH', b'EOS', length, length, length,
                        length, rate, 0, 0, 0, 1)
    pdta = b''.join(chunk(tag, data) for tag, data in [
        (b'phdr', phdr), (b'pbag', bags), (b'pmod', bytes(10)),
        (b'pgen', struct.pack('<HHHH', 41, 0, 0, 0)), (b'inst', inst),
        (b'ibag', ibag), (b'imod', bytes(10)), (b'igen', igen), (b'shdr', shdr)])
    body = b'sfbk' + chunk(b'LIST', b'INFO' + info)
    body += chunk(b'LIST', b'sdta' + chunk(b'smpl', samples))
    body += chunk(b'LIST', b'pdta' + pdta)
    path.write_bytes(chunk(b'RIFF', body))


def run(*args, success=True):
    result = subprocess.run(args, capture_output=True, text=True)
    assert (result.returncode == 0) == success, (args, result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix='m1-test-') as directory:
    root = Path(directory)
    font, output = root / 'test.sf2', root / 'test.wav'
    make_font(font)
    result = run('./build/test_engine', str(font))
    print(result.stdout, end='')
    assert 'Test sine' in run('./build/m1', '--soundfont', str(font), '--presets').stdout
    for rate in (44100, 48000):
        run('./build/m1', '--soundfont', str(font), '--render', str(output),
            '--rate', str(rate), '--dry', '--velocity', '100')
        with wave.open(str(output), 'rb') as audio:
            assert (audio.getnchannels(), audio.getsampwidth(), audio.getframerate(),
                    audio.getnframes()) == (2, 2, rate, rate * 5)
            pcm = array.array('h', audio.readframes(audio.getnframes()))
            if sys.byteorder != 'little':
                pcm.byteswap()
            attack = pcm[:rate * 2]
            tail = pcm[-rate * 2:]
            assert max(abs(x) for x in attack) > 100
            assert max(abs(x) for x in tail) < 10
    for flag, value in [('--rate', 'bad'), ('--program', '128'), ('--channel', '17'),
                        ('--buffer', '0'), ('--velocity', '-1'), ('--velocity', '128'),
                        ('--gain', 'nan'), ('--gain', '0.3junk')]:
        run('./build/m1', '--soundfont', str(font), flag, value, success=False)
    run('./build/m1', '--soundfont', str(font), '--program', '1',
        '--render', str(output), success=False)
    run('./build/m1', '--soundfont', str(root / 'missing.sf2'),
        '--render', str(output), success=False)
    run('./build/m1', '--soundfont', str(font), '--render', str(root / 'absent' / 'x.wav'),
        success=False)
    run('./build/m1', '--unknown', 'x', success=False)
    run('./build/m1', '--soundfont', success=False)
    run('./build/m1', success=False)
print('Integration: preset listing, stereo WAV at two rates, release tail and errors passed.')
