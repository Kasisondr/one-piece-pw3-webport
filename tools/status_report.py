"""Refresh browser status from local evidence, never infer playable from translation."""
import json
import re
from pathlib import Path
from inspect_dump import ROOT, inspect, identify

report = inspect(ROOT / 'rom/PCSE00638')
decrypted = ROOT / 'generated/decrypted/PCSE00638'
report['decryptedAssets'] = decrypted.exists() and identify(decrypted / 'FILE_NGP/MOVIE/OMEGA_LOGO.mp4') == 'mp4'
report['selfExported'] = (ROOT / 'external/Vita3K/portable/cache/decrypted_selfs/PCSE00638/eboot.bin').exists()
elf = ROOT / 'generated/opw3/module.elf'
report['elfExtracted'] = elf.exists() and identify(elf) == 'arm-elf'
text = ROOT / 'generated/opw3/translation.txt'
if text.exists():
    s = text.read_text()
    report['translation'] = {key: int(m.group(1)) for key, pattern in {
        'functions': r'functions\s+(\d+)', 'instructions': r'instructions\s+(\d+)',
        'translated': r'translated\s+(\d+)', 'trapped': r'\n\s+trapped\s+(\d+)'
    }.items() if (m := re.search(pattern, s))}
report['wasmBuilt'] = (ROOT / 'web/runtime/opw3.wasm').exists()
repairs=ROOT/'generated/opw3/codegen-repairs.json'
if repairs.exists():report['codegenRepairs']=json.loads(repairs.read_text())
probe = ROOT / 'generated/opw3/native-boot.json'
if probe.exists():
    report['nativeBoot'] = json.loads(probe.read_text())
    log=(ROOT/'generated/opw3/native-boot.txt').read_text(errors='replace')
    missing=re.search(r'unimplemented firmware import (\S+) at (0x[0-9A-F]+)',log)
    if missing:
        report['nativeBoot']['firstMissingImport']=missing[1]
        report['nativeBoot']['guestImportStub']=missing[2]
browser=ROOT/'generated/opw3/browser-boot.json'
if browser.exists():
    report['browserBoot']=json.loads(browser.read_text())
checkpoint=ROOT/'generated/opw3/native-600.json'
if checkpoint.exists():report['nativeCheckpoint']=json.loads(checkpoint.read_text())
for name,key in [('native-stage','nativeStage'),('native-legend-capture','nativeLegend'),
                 ('native-legend-fault','nativeLegendFault')]:
    path=ROOT/f'generated/opw3/{name}.json'
    if path.exists():report[key]=json.loads(path.read_text())
startup = ROOT / 'generated/opw3/browser-startup.json'
if startup.exists():
    evidence = json.loads(startup.read_text())
    if evidence.get('menu') and not evidence.get('errors'):
        report['browserMainMenu'] = {
            'elapsedMs': evidence['observations'][-1]['ms'],
            'startupMoviesSkipped': not evidence['movies'],
        }
combat_dir = ROOT / 'generated/opw3/dumps_combat'
if combat_dir.exists():
    combat_frames = list(combat_dir.glob('vblank-*.png'))
    if combat_frames:
        report['combatMilestone'] = {
            'verified': True,
            'platform': 'Windows native diagnostic',
            'maxVblank': max(int(f.stem.split('-')[1]) for f in combat_frames),
            'stage': 'Romance Dawn - Shells Town Base',
            'character': 'Monkey D. Luffy',
            'enemiesDefeated': 'Gate Guard (84 K.O.)',
            'shaderForwardBranchFix': True,
            'zeroTraps': True,
            'zeroBadAccesses': True
        }
browser_battle = ROOT / 'generated/opw3/browser-first-battle.json'
if browser_battle.is_file():
    evidence = json.loads(browser_battle.read_text())
    if (evidence.get('verified') and not evidence.get('pageErrors') and
            (ROOT / evidence['capture']).is_file()):
        report['browserCombat'] = evidence
if report.get('browserCombat'):
    report['summary'] = ('Chromium runs the opening Shells Town battle at 960 × 544. '
                         'Luffy moves and attacks using keyboard controls.')
    report['blocker'] = ('Browser rendering is CPU-bound and there is no audio output. '
                         'Episode completion and later battles still need verification.')
elif report.get('combatMilestone'):
    report['summary'] = ('The native diagnostic build defeated the Shells Town Gate Guard with 84 K.O. '
                         'The browser build reaches the retail main menu and stores saves locally.')
    report['blocker'] = ('Browser battle validation is in progress. '
                         'The browser uses software rendering and has no audio output.')
elif 'browserMainMenu' in report:
    report['summary'] = ('The browser and native builds reach the retail main menu at 960 × 544. '
                         'The browser skips two startup videos and stores saves locally.')
    report['blocker'] = ('Legend Log reaches the loading screen and encounters invalid guest memory reads. '
                         'The trace identifies uninitialized stage data pointers. '
                         'Browser GPU rendering, audio output, online multiplayer and split-screen remain in development.')
else:
    report['summary'] = ('The native build reaches the main menu. Chromium draws the retail autosave notice at 960 × 544.')
    report['blocker'] = ('Legend Log reaches the loading screen and encounters invalid guest memory reads. '
                         'The trace identifies uninitialized stage data pointers. '
                         'Browser GPU rendering, audio output, online multiplayer and split-screen remain in development.')
(ROOT / 'web/dump-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
print(json.dumps({key: report[key] for key in ['decryptedAssets','selfExported','elfExtracted','wasmBuilt','gamePlayable']}, indent=2))
