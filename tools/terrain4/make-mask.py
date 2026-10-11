"""Convert an authored RGBA terrain weight PNG into a linear, mipmapped VTF.

Requires Pillow and numpy. R gravel, G loam, B mud, A rock (not transparency).
Black pixels become gravel. Output is uncompressed RGBA8888 and clamp-to-edge.
"""
import argparse
from pathlib import Path
import struct
import numpy as np
from PIL import Image


def normalize_weights(image):
    weights = np.asarray(image, dtype=np.float32) / 255
    total = weights.sum(axis=-1, keepdims=True)
    empty = total[..., 0] == 0
    weights[empty] = (1, 0, 0, 0)
    weights /= weights.sum(axis=-1, keepdims=True)
    return Image.fromarray(np.round(weights * 255).astype(np.uint8), 'RGBA')


def write_vtf(path, image):
    width, height = image.size
    mips = []
    current = image
    while True:
        mips.append(current.tobytes())
        if current.size == (1, 1):
            break
        current = normalize_weights(current.resize(
            (max(1, current.width // 2), max(1, current.height // 2)), Image.Resampling.BOX))
    header = bytearray(80)
    struct.pack_into('<4sIIIHHIHH', header, 0, b'VTF\0', 7, 2, 80, width, height, 0x0c, 1, 0)
    struct.pack_into('<3f', header, 32, .3, .3, .3)
    struct.pack_into('<fIBI', header, 48, 1, 0, len(mips), 0xffffffff)
    struct.pack_into('<BBH', header, 61, 0, 0, 1)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + b''.join(reversed(mips)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('png', type=Path)
    parser.add_argument('vtf', type=Path)
    parser.add_argument('--normalized-png', type=Path)
    options = parser.parse_args()
    image = Image.open(options.png)
    if image.mode != 'RGBA':
        parser.error('Input must be RGBA: alpha is the fourth material weight. Do not flatten it.')
    if any(n < 1 or n > 4096 or n & (n - 1) for n in image.size):
        parser.error('Each dimension must be a power of two between 1 and 4096.')
    image = normalize_weights(image)
    write_vtf(options.vtf, image)
    if options.normalized_png:
        image.save(options.normalized_png)
    print(f'Wrote {options.vtf}: {image.width} x {image.height}, linear RGBA weights')


if __name__ == '__main__':
    main()
