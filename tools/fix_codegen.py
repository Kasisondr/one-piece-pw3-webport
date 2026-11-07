"""Repair measured emitter errors before compiling, without changing upstream.

Both loads in one ARM LDRD use the original effective address. The pinned
emitter instead evaluates it twice, after assigning the first destination.
Only aliasing pairs are changed; fixed blocks are idempotent.
"""
import argparse
import json
import re
import struct
import hashlib
from pathlib import Path

PAIR=re.compile(r'(?P<comment>    /\* [0-9A-F]+  ldrd +\*/\n)'
                r'    (?P<lo>r\d+|sp|lr) = vita_read32\((?P<base>[^\n]+)\);\n'
                r'    (?P<hi>r\d+|sp|lr) = vita_read32\(\((?P=base)\) \+ 4\);')


def recover_error_table(source, elf):
    """Recover one measured PCSE00638 switch whose range guard was missed.

    IDA confirms 30 TBH cases at 81307566. Every target is MVNS r0,#imm
    followed by B/BX to the common return. Decode those actual instructions;
    do not invent error codes or turn the failed file operation into success.
    The exact ELF hash and instruction forms are mandatory safety guards.
    """
    marker='    vita_trap_indirect(0x81307566, 0);'
    if marker not in source:return source,0
    data=elf.read_bytes()
    if hashlib.sha256(data).hexdigest()!='e8802ce6c410a009725332519040ebf4aa75f2e8bcce7bab22370a5d3bdcbfa6':
        raise ValueError('Unreviewed ELF for PCSE00638 error-table recovery')
    ph=struct.unpack_from('<I',data,28)[0]
    segments=[struct.unpack_from('<8I',data,ph+i*32) for i in range(struct.unpack_from('<H',data,44)[0])]
    def read(addr,size):
        for typ,off,base,_,length,*_ in segments:
            if typ==1 and base<=addr and addr+size<=base+length:return data[off+addr-base:off+addr-base+size]
        raise ValueError(f'Unmapped instruction {addr:08X}')
    assert read(0x81307566,4)==bytes.fromhex('dfe810f0')
    assert read(0x8130760e,2)==bytes.fromhex('7047')
    cases=[]
    for index in range(30):
        target=0x8130756a+2*struct.unpack('<H',read(0x8130756a+2*index,2))[0]
        h1,h2=struct.unpack('<HH',read(target,4))
        # MVNS.W r0, an unrotated imm8; the carry flag is preserved.
        if h1==0xf05f and h2==0x30ff: # MOVS.W r0,#0xFFFFFFFF (replicated imm8)
            expression='0xFFFFFFFFu'
        else:
            assert h1==0xf07f and h2<256, f'Unreviewed table leaf {target:08X}'
            expression=f'~0x{h2:X}u'
        if target+4!=0x8130760e:
            branch=struct.unpack('<H',read(target+4,2))[0]
            assert branch&0xf800==0xe000
            displacement=branch&0x7ff
            if displacement&0x400:displacement-=0x800
            assert target+8+2*displacement==0x8130760e
        cases.append(f'    case {index}: r0 = {expression}; vita_flags_nz(r0); return;')
    fixed=('    /* Recovered, hash-checked TBH targets; see docs/BRINGUP.md. */\n'
           '    switch (r0) {\n'+'\n'.join(cases)+'\n'
           '    default: vita_trap_indirect(0x81307566, r0); return;\n    }')
    return source.replace(marker,fixed),1


def recover_tbb_cutscene_table(source, elf):
    """Recover measured TBB switch at 812D0A16 in vita_func_812D0790.

    In the prologue sequence, the game evaluates a 21-case table branch (TBB)
    where r7 in [0x00EE8080, 0x00EE8094]. All 21 cases resolve (directly or via
    unconditional branch trampolines at 812D0A5C..812D0A6C) to L_812D0A30.
    """
    marker = ('    /* 812D0A14  add/sub          */\n'
              '    { uint32_t _a = r7, _b = r1;\n'
              '      r1 = _a - _b;\n'
              '      vita_flags_sub(_a, _b, r1); }\n'
              '    /* 812D0A16  tbb              */\n'
              '    vita_trap_indirect(0x812D0A16, 0);')
    if marker not in source: return source, 0
    data = elf.read_bytes()
    if hashlib.sha256(data).hexdigest() != 'e8802ce6c410a009725332519040ebf4aa75f2e8bcce7bab22370a5d3bdcbfa6':
        raise ValueError('Unreviewed ELF for PCSE00638 table recovery')
    ph = struct.unpack_from('<I', data, 28)[0]
    segments = [struct.unpack_from('<8I', data, ph + i * 32) for i in range(struct.unpack_from('<H', data, 44)[0])]
    def read(addr, size):
        for typ, off, base, _, length, *_ in segments:
            if typ == 1 and base <= addr and addr + size <= base + length: return data[off + addr - base : off + addr - base + size]
        raise ValueError(f'Unmapped instruction {addr:08X}')
    assert read(0x812D0A16, 4) == bytes.fromhex('dfe801f0')
    assert read(0x812D0A1A, 21) == bytes.fromhex('0b0b29292828272726262525242323222221210b0b')
    fixed = ('    /* 812D0A14  add/sub          */\n'
             '    { uint32_t _a = r7, _b = r1;\n'
             '      r1 = _a - _b;\n'
             '      vita_flags_sub(_a, _b, r1); }\n'
             '    /* 812D0A16  tbb (recovered, hash-checked table; see docs/BRINGUP.md) */\n'
             '    goto L_812D0A30;')
    return source.replace(marker, fixed), 1


def repair(source):
    count=0
    def replace(m):
        nonlocal count
        lo,hi,base=m['lo'],m['hi'],m['base']
        if not re.search(r'\b'+re.escape(lo)+r'\b',base):return m[0]
        count+=1
        return (m['comment']+'    { uint32_t _ldrd_addr = '+base+';\n'
                '      uint32_t _ldrd_lo = vita_read32(_ldrd_addr);\n'
                '      uint32_t _ldrd_hi = vita_read32(_ldrd_addr + 4);\n'
                f'      {lo} = _ldrd_lo; {hi} = _ldrd_hi; }}')
    return PAIR.sub(replace,source),count


def skip_browser_startup_movies(source, elf):
    """Use the game's no-movie transition for startup and the first episode."""
    call = ('    /* 81077ABE  bl               */\n'
            '    vita_func_81077838();')
    old_guard = ('    /* Browser fallback for OMEGA_LOGO and PM021_NA. */\n'
                 '#ifdef __EMSCRIPTEN__\n'
                 '    if (r1 == 23u || r1 == 21u) goto L_81077AC6;\n'
                 '#endif\n')
    guard = ('    /* Browser fallback for OMEGA_LOGO, PM021_NA and PM000. */\n'
             '#ifdef __EMSCRIPTEN__\n'
             '    if (r1 == 23u || r1 == 21u || r1 == 0u) goto L_81077AC6;\n'
             '#endif\n')
    if call not in source or guard in source:
        return source, 0
    expected = 'e8802ce6c410a009725332519040ebf4aa75f2e8bcce7bab22370a5d3bdcbfa6'
    if hashlib.sha256(elf.read_bytes()).hexdigest() != expected:
        raise ValueError('Unreviewed ELF for PCSE00638 browser startup fallback')
    if source.count(call) != 1 or 'L_81077AC6:' not in source:
        raise ValueError('Unreviewed emitted code for PCSE00638 browser startup fallback')
    return source.replace(old_guard + call, call).replace(call, guard + call), 1


def repair_directory(directory):
    changes={};loads={};tables={}
    for path in sorted(Path(directory).glob('recomp_funcs*.c')):
        text=path.read_text();fixed,count=repair(text)
        elf=Path(directory)/'module.elf'
        if elf.exists():
            fixed,recovered=recover_error_table(fixed,elf)
            count+=recovered
            fixed,recovered_tbb=recover_tbb_cutscene_table(fixed,elf)
            count+=recovered_tbb
            fixed,skipped_intro=skip_browser_startup_movies(fixed,elf)
            count+=skipped_intro
        if count:
            path.write_text(fixed)
            changes[path.name]=count
        load_count=fixed.count('uint32_t _ldrd_addr = ')
        table_count=len(re.findall(r'recovered,\s*hash-checked', fixed, re.I))
        if load_count:loads[path.name]=load_count
        if table_count:tables[path.name]=table_count
    report_path=Path(directory)/'codegen-repairs.json'
    report={'ldrdAliasing':{'count':sum(loads.values()),'files':loads},
            'recoveredErrorTables':{'count':sum(tables.values()),'files':tables},
            'total':sum(loads.values())+sum(tables.values())}
    report_path.write_text(json.dumps(report,indent=2)+'\n')
    return changes


def fixture(path):
    broken='''void regression_ldrd(void) {
    /* 81306F98  ldrd             */
    r0 = vita_read32(r0 + 0x38);
    r1 = vita_read32((r0 + 0x38) + 4);
}
void regression_reverse(void) {
    /* 81000000  ldrd             */
    r7 = vita_read32(r7 + r6);
    r8 = vita_read32((r7 + r6) + 4);
}
'''
    fixed,count=repair(broken)
    assert count==2
    assert repair(fixed)==(fixed,0)
    Path(path).write_text(fixed)


def game_fixture(directory,path):
    for part in Path(directory).glob('recomp_funcs*.c'):
        match=re.search(r'^void vita_func_81307514\(void\) \{.*?^\}',part.read_text(),re.M|re.S)
        if match:
            Path(path).write_text('#define PORT_HAS_ERROR_TABLE 1\n'+match[0]+'\n')
            return
    Path(path).write_text('#define PORT_HAS_ERROR_TABLE 0\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',nargs='?',type=Path)
    parser.add_argument('--fixture',type=Path);parser.add_argument('--game-fixture',type=Path);args=parser.parse_args()
    if args.fixture:fixture(args.fixture)
    elif args.game_fixture and args.directory:game_fixture(args.directory,args.game_fixture)
    elif args.directory:print(json.dumps(repair_directory(args.directory),indent=2))
    else:parser.error('provide a generated directory or --fixture')
