"""Inspect a local dump without modifying it or printing licence material."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sfo_values(path):
    data = path.read_bytes()
    if data[:4] != b'\0PSF':
        return {}
    _, _, keys, values, count = struct.unpack_from('<5I', data)
    result = {}
    for i in range(count):
        key, fmt, size, _, offset = struct.unpack_from('<HHIII', data, 20 + i * 16)
        name = data[keys + key:].split(b'\0', 1)[0].decode('utf8', 'replace')
        value = data[values + offset:values + offset + size]
        if name in ('TITLE', 'TITLE_ID', 'APP_VER', 'VERSION'):
            result[name] = value.rstrip(b'\0').decode('utf8', 'replace')
    return result


def identify(path):
    with path.open('rb') as f:
        h = f.read(32)
    if h[:4] == b'\x7fELF':
        if len(h) < 20 or h[4:6] != b'\x01\x01':
            return 'unsupported-elf'
        return 'arm-elf' if struct.unpack_from('<H', h, 18)[0] == 40 else 'non-arm-elf'
    if h[:4] == b'SCE\0':
        return 'self-container'
    if h[:8] == b'\x89PNG\r\n\x1a\n':
        return 'png'
    if h[4:8] == b'ftyp':
        return 'mp4'
    return 'unrecognized'


def inspect(root):
    root = Path(root).resolve()
    if not (root / 'eboot.bin').is_file():
        raise ValueError(f'Missing eboot.bin under {root}')
    files = [p for p in root.rglob('*') if p.is_file()]
    eboot = root / 'eboot.bin'
    probes = ['eboot.bin', 'sce_module/libc.suprx',
              'FILE_NGP/MOVIE/OMEGA_LOGO.mp4', 'sce_sys/icon0.png']
    result = {
        'schema': 1, 'metadata': sfo_values(root / 'sce_sys/param.sfo')
        if (root / 'sce_sys/param.sfo').exists() else {},
        'files': len(files), 'bytes': sum(p.stat().st_size for p in files),
        'ebootSha256': hashlib.sha256(eboot.read_bytes()).hexdigest(),
        'pfsMetadataPresent': (root / 'sce_pfs').is_dir(),
        'probes': {p: identify(root / p) for p in probes if (root / p).exists()},
        'gamePlayable': False,
        'features': {'keyboardMouse': 'browser-host', 'controller': 'browser-host',
                     'touch': 'browser-host', 'onlineCoop': 'not-implemented',
                     'splitScreen': 'not-implemented', '1080p': 'presentation-scaling-only',
                     '60fps': 'unverified-for-game'},
    }
    kind = identify(eboot)
    result['executableReady'] = kind in ('arm-elf', 'self-container')
    result['blocker'] = (
        'Executable format recognized. Translation, imports, rendering and gameplay still require validation.'
        if result['executableReady'] else
        'Executable has no ELF/SELF header. With the PFS metadata and unreadable media, this is consistent '
        'with a PFS-encrypted dump. Supply the decrypted executable AND matching decrypted app0 data.')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('dump', nargs='?', type=Path, default=ROOT / 'rom/PCSE00638')
    parser.add_argument('--out', type=Path, default=ROOT / 'web/dump-report.json')
    args = parser.parse_args()
    report = inspect(args.dump)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2))
