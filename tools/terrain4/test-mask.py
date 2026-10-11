"""Checks that authored layer weights survive normalization and VTF mip export."""
import importlib.util
import struct
import tempfile
from pathlib import Path
import numpy as np
from PIL import Image

spec = importlib.util.spec_from_file_location('masktool', Path(__file__).with_name('make-mask.py'))
masktool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(masktool)
pixels = np.array([[(255,0,0,0), (0,255,0,0), (0,0,255,0), (0,0,0,255)],
                   [(0,0,0,0), (200,100,0,0), (0,0,40,80), (5,5,5,5)]], dtype=np.uint8)
image = masktool.normalize_weights(Image.fromarray(pixels, 'RGBA'))
normalized = np.array(image)
assert np.array_equal(normalized[0], pixels[0]), 'Pure layer channels changed'
assert tuple(normalized[1,0]) == (255,0,0,0), 'Black must fall back to gravel'
assert normalized[1,2,3] > normalized[1,2,2] > 0, 'Rock alpha was lost'
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'weights.vtf'
    masktool.write_vtf(path, image)
    data = path.read_bytes()
    assert struct.unpack_from('<HH', data, 16) == (4,2), 'VTF dimensions changed'
    assert struct.unpack_from('<I', data, 20)[0] == 0x0c, 'Mask must be linear and clamp-to-edge'
    assert struct.unpack_from('<I', data, 52)[0] == 0, 'Mask must be RGBA8888'
    assert data[-32:] == image.tobytes(), 'Full-resolution RGBA changed on export'
    assert data[80:84] != bytes(4), 'Smallest mip must retain material coverage'
print('PASS: pure layers, mixtures, rock alpha, black fallback, VTF dimensions/flags/mips')
