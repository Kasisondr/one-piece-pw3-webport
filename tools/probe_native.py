"""Bounded retail-game boot; fail on traps rather than pretending they succeeded."""
import json
import os
import subprocess
import argparse
import hashlib
import re
from bisect import bisect_right
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, default=ROOT / 'build/bringup/Release/port.exe')
p.add_argument('--frames', type=int, default=120)
p.add_argument('--timeout', type=int, default=60)
p.add_argument('--name',default='native-boot',help='Report basename within generated/opw3')
p.add_argument('--press', action='append', default=[], help='Guest button schedule, for example CROSS@1500-1510')
p.add_argument('--save-dir', type=Path, default=ROOT / 'generated/saves/PCSE00638')
p.add_argument('--capture-every', type=int, default=0, help='Capture intermediate frames at this vblank interval')
p.add_argument('--stop-after-bad', type=int, default=0, help='Stop at this invalid access and log the native call stack')
args=p.parse_args();exe=args.exe.resolve()
if not re.fullmatch(r'[a-zA-Z0-9_-]+',args.name):p.error('name must contain only letters, digits, _ and -')
elf = ROOT / 'generated/opw3/module.elf'
app = ROOT / 'generated/decrypted/PCSE00638'
saves = ROOT / 'generated/saves'
saves.mkdir(parents=True, exist_ok=True)
savedata=args.save_dir.resolve();savedata.mkdir(parents=True,exist_ok=True)
out = ROOT / f'generated/opw3/{args.name}.txt'
frame = ROOT / 'generated/opw3' / ('frame.ppm' if args.name=='native-boot' else f'{args.name}.ppm')
if frame.exists():frame.unlink()
env = dict(os.environ, VITARECOMP_GXM_GPU='1')
# Inherit no diagnostic log-and-continue mode. A trap must stop the probe.
env.pop('VITARECOMP_TRACE', None)
env['PORT_STOP_AFTER_BAD_ACCESS']=str(max(0,args.stop_after_bad))
command = [str(exe), str(elf), '--mount', f'app0={app}', '--mount', f'ux0={saves}',
           '--mount',f'savedata0={savedata}',
           '--frames', str(args.frames), '--virtual-clock', '--dump', str(frame)]
for press in args.press:
    command.extend(['--press', press])
if args.capture_every > 0:
    capture = ROOT / 'generated/opw3' / (args.name + '-frames')
    capture.mkdir(parents=True, exist_ok=True)
    command.extend(['--dump-dir', f'{capture}@{args.capture_every}'])
try:
    with out.open('w') as f:
        run = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT, env=env, timeout=args.timeout)
    result = {'exitCode': run.returncode, 'timedOut': False, 'gamePlayable': False}
except subprocess.TimeoutExpired:
    result = {'exitCode': None, 'timedOut': True, 'gamePlayable': False}
result['log'] = out.name
result['requestedVblanks']=args.frames
result['inputSchedule']=args.press
log=out.read_text(errors='replace')
fault=re.search(r'PORT memory fault (\d+): address ([\dA-F]+), size (\d+), guest SP ([\dA-F]+)',log)
if fault:
    result['fault']={'number':int(fault[1]),'address':'0x'+fault[2],
                     'size':int(fault[3]),'guestSp':'0x'+fault[4]}
    map_path=exe.parent.parent/'port.map'
    if map_path.exists():
        symbols=map_path.read_text(errors='replace')
        base_match=re.search(r'Preferred load address is ([\dA-Fa-f]+)',symbols)
        if base_match:
            base=int(base_match[1],16)
            entries=sorted((int(m[2],16)-base,m[1]) for m in re.finditer(
                r'^\s+0001:[\dA-Fa-f]+\s+(\S+)\s+([\dA-Fa-f]+)\s+f\s+',symbols,re.M))
            addresses=[address for address,_ in entries]
            if addresses:
                stack=[]
                for match in re.finditer(r'PORT stack RVA ([\dA-F]+)',log):
                    rva=int(match[1],16)-1
                    i=bisect_right(addresses,rva)-1
                    if i>=0 and rva<=addresses[-1]+0x10000:
                        stack.append({'function':entries[i][1],'offset':rva-addresses[i]})
                result['fault']['stack']=stack
summary=re.search(r'(\d+) vblanks, (\d+) flips, (\d+) traps, dispatch (\d+) hits / (\d+) misses, (\d+) bad accesses',log)
if summary:
    result.update(zip(['vblanks','flips','traps','dispatchHits','dispatchMisses','badAccesses'],map(int,summary.groups())))
if frame.exists():
    content=frame.read_bytes();pixels=content.split(b'\n',3)[3]
    result['frameSha256']=hashlib.sha256(content).hexdigest()
    result['nonBlackFrame']=any(pixels)
(ROOT / f'generated/opw3/{args.name}.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result))
print(out.read_text(errors='replace')[-5000:])
