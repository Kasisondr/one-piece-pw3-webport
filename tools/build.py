"""Reproducible native recompiler -> generated C -> Emscripten pipeline."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from inspect_dump import identify
from fix_codegen import repair_directory

ROOT = Path(__file__).resolve().parents[1]
PIN = '5a095d7f429b1e51ffca3af438b18ad1502f07fb'
SDK = '6.0.13'
PCSE00638_ELF_SHA256 = 'e8802ce6c410a009725332519040ebf4aa75f2e8bcce7bab22370a5d3bdcbfa6'


def run(*args):
    print('>', ' '.join(str(x) for x in args), flush=True)
    subprocess.run([str(x) for x in args], cwd=ROOT, check=True)


def tool(reviewed_tables=False):
    build = ROOT / ('build/recompiler' if reviewed_tables else 'build/native')
    source = ROOT / ('tools/recompiler' if reviewed_tables else 'external/vitarecomp')
    target_dir = build / ('upstream/tools/armrecomp' if reviewed_tables else 'tools/armrecomp')
    exe = target_dir / ('Release/armrecomp.exe' if os.name == 'nt' else 'armrecomp')
    if reviewed_tables or not exe.exists():
        args = ['cmake', '-S', source, '-B', build]
        args += ['-G', 'Visual Studio 17 2022', '-A', 'x64'] if os.name == 'nt' else ['-DCMAKE_BUILD_TYPE=Release']
        run(*args)
        run('cmake', '--build', build, '--config', 'Release', '--target', 'armrecomp', '-j', '6')
    return exe


def prepare(source, name):
    source = source.resolve()
    actual = subprocess.check_output(['git', '-C', str(ROOT / 'external/vitarecomp'), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != PIN:
        raise ValueError(f'Toolkit revision {actual} differs from pinned {PIN}. Review before recompiling.')
    kind = identify(source)
    if kind not in ('arm-elf', 'self-container'):
        raise ValueError(f'{source.name}: {kind}; need a decrypted ARM ELF or supported plaintext SELF. '
                         'PFS-encrypted input cannot be recompiled.')
    arm = tool()
    out = ROOT / 'generated' / name
    out.mkdir(parents=True, exist_ok=True)
    elf = out / 'module.elf'
    if kind == 'arm-elf':
        shutil.copyfile(source, elf)
    else:
        run(arm, 'extract', source, elf)
    if name == 'opw3' and hashlib.sha256(elf.read_bytes()).hexdigest() == PCSE00638_ELF_SHA256:
        arm = tool(reviewed_tables=True)
    db = ROOT / 'external/vita-headers/db/360'
    if not db.is_dir():
        raise ValueError('Missing NID database. Run tools/setup.ps1 first.')
    with (out / 'imports.txt').open('w') as f:
        subprocess.run([str(arm), 'funcs', str(elf), str(db)], stdout=f, check=True)
    with (out / 'translation.txt').open('w') as f:
        subprocess.run([str(arm), 'emit', str(elf), str(out / 'recomp_funcs.c'), '0', str(db)],
                       stdout=f, check=True)
    (out / 'provenance.json').write_text(json.dumps({
        'module': name, 'vitarecomp': PIN, 'emsdk': SDK,
        'source': source.name, 'sourceSha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'gamePlayable': False,
    }, indent=2))
    print((out / 'translation.txt').read_text()[-3000:])
    print('Codegen repairs:', repair_directory(out))
    return out


def build_web(out, name):
    print('Codegen repairs:', repair_directory(out))
    sdk = Path(os.environ.get('EMSDK', ROOT / 'external/emsdk'))
    em = sdk / 'upstream/emscripten'
    if not (em / 'emcc.py').exists():
        raise ValueError('Emscripten SDK missing; run tools/setup.ps1')
    # emcmake's platform wrapper imports the SDK config installed by activate.
    os.environ['EMSDK'] = str(sdk)
    os.environ['EM_CONFIG'] = str(sdk / '.emscripten')
    run(sys.executable, em / 'emcmake.py', 'cmake', '-S', ROOT, '-B', ROOT / f'build/web-{name}',
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', f'-DRECOMP_DIR={out}',
        f'-DMODULE_ELF={out / "module.elf"}', f'-DPORT_NAME={name}')
    run('cmake', '--build', ROOT / f'build/web-{name}', '--target', 'port', 'web_io_probe', '-j', '6')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('action', choices=['demo', 'prepare', 'web'])
    p.add_argument('--module', type=Path)
    p.add_argument('--name', choices=['fbdemo', 'opw3'], default='opw3')
    a = p.parse_args()
    try:
        if a.action == 'demo':
            out = prepare(ROOT / 'external/vitarecomp/samples/fbdemo/prebuilt/eboot.bin', 'fbdemo')
            build_web(out, 'fbdemo')
        elif a.action == 'prepare':
            if not a.module:
                p.error('prepare requires --module PATH')
            prepare(a.module, a.name)
        else:
            build_web(ROOT / 'generated' / a.name, a.name)
    except (ValueError, subprocess.CalledProcessError) as e:
        sys.exit(str(e))
