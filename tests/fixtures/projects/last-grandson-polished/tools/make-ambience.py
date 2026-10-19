"""Original, sample-free winter motif. Standard-library PCM synthesis; no external recordings."""
import math
import struct
import wave
from pathlib import Path

rate = 22050
seconds = 24
# A sparse pentatonic phrase with long decays. This is an original placeholder score,
# not a recording or an imitation of music from another game.
notes = [(0.5, 293.665), (3, 349.228), (5.5, 440), (8.5, 391.995),
         (12.5, 293.665), (15, 261.626), (18, 220)]
destination = Path(__file__).resolve().parents[1] / 'assets' / 'winter-motif.wav'
samples = bytearray()
for i in range(rate * seconds):
    t = i / rate
    fade = min(1, t / 2, (seconds - t) / 3)
    base = 0.013 * (math.sin(2 * math.pi * 146.8325 * t) + 0.4 * math.sin(2 * math.pi * 220 * t))
    left = right = base * fade
    for j, (start, frequency) in enumerate(notes):
        d = t - start
        if 0 <= d <= 6:
            envelope = (1 - math.exp(-d * 80)) * math.exp(-d * 1.05)
            tone = sum(math.sin(2 * math.pi * frequency * n * d) / n**2.3 for n in range(1, 5))
            value = 0.075 * envelope * tone * fade
            left += value * (0.8 if j % 2 else 1)
            right += value * (1 if j % 2 else 0.8)
    samples += struct.pack('<hh', int(max(-1, min(1, left)) * 32767), int(max(-1, min(1, right)) * 32767))
with wave.open(str(destination), 'wb') as stream:
    stream.setnchannels(2)
    stream.setsampwidth(2)
    stream.setframerate(rate)
    stream.writeframes(samples)
print(destination)
