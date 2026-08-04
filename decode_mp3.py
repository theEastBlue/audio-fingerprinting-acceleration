import miniaudio
import struct
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "test.mp3"
out = sys.argv[2] if len(sys.argv) > 2 else "raw_data"

decoded = miniaudio.decode_file(
    src,
    output_format=miniaudio.SampleFormat.SIGNED16,
    nchannels=1,
    sample_rate=22050
)

with open(out, "wb") as f:
    f.write(bytes(decoded.samples))

num_samples = len(decoded.samples) // 2
duration = num_samples / 22050
print(f"Decoded: {num_samples} samples, {duration:.2f}s, written to '{out}'")
