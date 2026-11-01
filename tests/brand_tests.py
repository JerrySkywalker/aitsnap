"""Verify that every production ICO frame is the corresponding master-derived PNG."""
import hashlib
import json
from pathlib import Path
import struct
import argparse

root = Path(__file__).resolve().parents[1] / 'assets/brand'
manifest = json.loads((root / 'derivation.json').read_text(encoding='utf-8'))
assert hashlib.sha256((root / manifest['master']).read_bytes()).hexdigest() == manifest['masterSha256']
icon = (root / 'aitsnap.ico').read_bytes()
reserved, kind, count = struct.unpack_from('<HHH', icon)
assert reserved == 0 and kind == 1 and count == len(manifest['sizes'])
for i, size in enumerate(manifest['sizes']):
    width, height, _, _, planes, bits, length, offset = struct.unpack_from('<BBBBHHII', icon, 6 + 16 * i)
    frame = icon[offset:offset + length]
    assert frame == (root / f'aitsnap-{size}.png').read_bytes()
    assert (width or 256) == (height or 256) == size and planes == 1 and bits == 32
    assert frame[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack_from('>II', frame, 16) == (size, size)
for name, digest in manifest['files'].items():
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest, name
print(f'BRAND_PASS one SVG, {count} matching ICO/PNG sizes')

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--pe', type=Path, nargs='*', default=[])
a = p.parse_args()
if a.pe:
    import ctypes as C
    from ctypes import wintypes as W
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [W.LPCWSTR, W.HANDLE, W.DWORD]; kernel.LoadLibraryExW.restype = W.HMODULE
    kernel.FindResourceW.argtypes = [W.HMODULE, W.LPCWSTR, W.LPCWSTR]; kernel.FindResourceW.restype = W.HANDLE
    kernel.LoadResource.argtypes = [W.HMODULE, W.HANDLE]; kernel.LoadResource.restype = W.HANDLE
    kernel.SizeofResource.argtypes = [W.HMODULE, W.HANDLE]; kernel.SizeofResource.restype = W.DWORD
    kernel.LockResource.argtypes = [W.HANDLE]; kernel.LockResource.restype = C.c_void_p
    kernel.FreeLibrary.argtypes = [W.HMODULE]
    def rid(value): return C.cast(C.c_void_p(value), W.LPCWSTR)
    def resource(module, kind, name):
        entry = kernel.FindResourceW(module, rid(name), rid(kind)); assert entry, C.get_last_error()
        data = kernel.LockResource(kernel.LoadResource(module, entry)); assert data
        return C.string_at(data, kernel.SizeofResource(module, entry))
    for path in a.pe:
        module = kernel.LoadLibraryExW(str(path.resolve()), None, 0x20 | 0x2)  # resources only; no executable code
        assert module, C.get_last_error()
        try:
            group = resource(module, 14, 101); assert struct.unpack_from('<HHH', group) == (0, 1, count)
            for i, size in enumerate(manifest['sizes']):
                width, height, _, _, _, _, length, identifier = struct.unpack_from('<BBBBHHIH', group, 6 + 14 * i)
                data = resource(module, 3, identifier)
                assert (width or 256) == (height or 256) == size and len(data) == length
                assert data == (root / f'aitsnap-{size}.png').read_bytes(), str(path)
            print(f'PE_BRAND_PASS {path.name} {count} exact master-derived frames')
        finally:
            kernel.FreeLibrary(module)
