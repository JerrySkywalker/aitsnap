"""Derive all production icon sizes from the single SVG master, using Qt."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--renderer', type=Path, required=True)
a = p.parse_args()
brand = root / 'assets/brand'
master = brand / 'aitsnap.svg'
sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
frames = []
for size in sizes:
    output = brand / f'aitsnap-{size}.png'
    subprocess.run([str(a.renderer.resolve()), str(master), str(output), str(size)], check=True)
    frames.append(output.read_bytes())
offset = 6 + 16 * len(frames)
entries = []
for size, frame in zip(sizes, frames):
    entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(frame), offset))
    offset += len(frame)
(brand / 'aitsnap.ico').write_bytes(struct.pack('<HHH', 0, 1, len(frames)) + b''.join(entries) + b''.join(frames))
(brand / 'derivation.json').write_text(json.dumps(dict(
    master='aitsnap.svg', masterSha256=hashlib.sha256(master.read_bytes()).hexdigest(),
    sizes=sizes, license='Apache-2.0',
    files={f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(brand.glob('aitsnap-*png'))} |
          {'aitsnap.ico': hashlib.sha256((brand / 'aitsnap.ico').read_bytes()).hexdigest()}
), indent=2) + '\n', encoding='utf-8', newline='\n')
print('BRAND_DERIVED sizes=' + ','.join(map(str, sizes)))
