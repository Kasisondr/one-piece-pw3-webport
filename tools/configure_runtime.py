"""Apply narrow source overlays without modifying the upstream checkout."""
import sys
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]

for target_arg in sys.argv[1:]:
    out_path = Path(target_arg)
    name = out_path.name
    if name == 'hle_threadmgr_port.c':
        source = (root / 'external/vitarecomp/src/hle_threadmgr.c').read_text()
        start = source.index('/* --- lightweight mutexes ')
        end = source.index('/* --- mutexes ', start)
        source = source[:start] + '\n' + (root / 'src/lwmutex.inc').read_text() + '\n' + source[end:]
        old = '? vita_read32(mwork) : 0;'
        assert source.count(old) == 1, 'Pinned upstream condition-wait implementation changed'
        source = source.replace(old, '? vita_read32(mwork + 8) : 0;')
        old = '#define MAX_EVFS      64'
        assert source.count(old) == 1, 'Pinned upstream event flag capacity changed'
        source = source.replace(old, '#define MAX_EVFS      256')
        out_path.write_text(source)
    elif name == 'sched_port.c':
        source = (root / 'external/vitarecomp/src/sched.c').read_text()
        signature = 'int vita_thread_start(uint32_t uid, uint32_t arglen, uint32_t argp) {'
        assert source.count(signature) == 1
        source = source.replace(signature, 'static int port_thread_start_mode(uint32_t uid, uint32_t arglen, uint32_t argp, int run_first) {')
        start = source.index('    /* The new thread runs first,')
        end = source.index('\nvoid vita_thread_exit(', start)
        block = source[start:end]
        old = '    mtx_lock(&g_mtx);\n    vthread *cur = g_cur;\n    cur->state = T_READY;\n    switch_to(cur, t);\n    mtx_unlock(&g_mtx);'
        assert block.count(old) == 1
        block = block.replace(old, '    if(run_first) {\n' + old + '\n    }')
        block += '''
int vita_thread_start(uint32_t uid, uint32_t arglen, uint32_t argp) {
    return port_thread_start_mode(uid,arglen,argp,1);
}
/* ULT jobs are queued until the creating guest thread yields. */
int port_thread_start_deferred(uint32_t uid) {
    return port_thread_start_mode(uid,0,0,0);
}
'''
        source = source[:start] + block + source[end:]
        source += '''
/* Allow a runtime-owned context to reuse its existing guest stack. */
int port_thread_stack(uint32_t uid, uint32_t base, uint32_t size) {
    init_once();
    vthread *t=by_uid(uid);
    if(!t || t->state!=T_NEW || size<512 || (base&7u) || (size&7u) || !vita_mem_ptr(base,size))return -1;
    t->stack_base=base;t->stack_size=size;return 0;
}
int port_thread_exit_status(uint32_t uid, uint32_t out) {
    init_once();
    vthread *t=by_uid(uid?uid:g_cur->uid);
    if(!t)return (int)0x80028021u;
    if(!vita_mem_ptr(out,4))return (int)0x80020006u;
    if(t->state==T_NEW)return (int)0x80028027u;
    if(t->state!=T_DONE)return (int)0x80028028u;
    vita_write32(out,(uint32_t)t->exit_status);return 0;
}
'''
        out_path.write_text(source)
    elif name == 'hle_io_port.c':
        source = (root / 'external/vitarecomp/src/hle_io.c').read_text()
        marker = '#define SCE_ERROR_ERRNO_ENOENT'
        assert source.count(marker) == 1
        at = source.index(marker)
        source = source[:at] + (root / 'src/io_web_stream.inc').read_text() + '\n' + source[at:]
        old = 'fwrite(p, 1, size, g_fds[i].stream);\n    fflush(g_fds[i].stream);\n    r0 = size;'
        assert source.count(old) == 1
        source = source.replace(old, 'size_t n=fwrite(p, 1, size, g_fds[i].stream);\n    int failed=fflush(g_fds[i].stream);\n    r0 = failed || ferror(g_fds[i].stream) ? 0x80010005u : (uint32_t)n;')
        old = 'r0 = (uint32_t)fread(p, 1, r2, g_fds[i].stream);'
        assert source.count(old) == 1
        source = source.replace(old, old + '\n    if(ferror(g_fds[i].stream))r0=0x80010005u;')
        source += '\n' + (root / 'src/io_positioned.inc').read_text()
        out_path.write_text(source)
    elif name == 'atrac9_port.c':
        source = (root / 'external/vitarecomp/src/atrac9.c').read_text()
        source += '\n' + (root / 'src/atrac9_frame.inc').read_text()
        out_path.write_text(source)
    elif name == 'hle_stubs_port.c':
        source = (root / 'external/vitarecomp/src/hle_stubs.c').read_text()
        for stub_name in ['sceNpInit', 'sceNpTerm', 'sceNpCheckCallback', 'sceNpRegisterServiceStateCallback', 'sceNpUnregisterServiceStateCallback']:
            pattern = r'void vita_hle_' + stub_name + r'\(void\)\s*\{[^}]*\}'
            source, count = re.subn(pattern, '/* ' + stub_name + ' supplied by the local signed-out NP backend. */', source)
            assert count == 1, stub_name
        out_path.write_text(source)
    elif name == 'hle_savedata_port.c':
        source = (root / 'external/vitarecomp/src/hle_savedata.c').read_text()
        old = 'if (vita_mem_ptr(info, 8)) {'
        assert source.count(old) == 1
        source = source.replace(old, 'if (info && vita_mem_avail(info) >= 8) {')
        old = 'void *param = exists ? vita_mem_ptr(vita_read32(info + 4), SLOT_PARAM_SIZE) : NULL;'
        assert source.count(old) == 1
        source = source.replace(old, 'uint32_t dst=vita_read32(info+4);\n        void *param = exists && dst && vita_mem_avail(dst)>=SLOT_PARAM_SIZE ? vita_mem_ptr(dst,SLOT_PARAM_SIZE) : NULL;')
        out_path.write_text(source)
    elif name == 'mem_port.c':
        source = (root / 'external/vitarecomp/src/mem.c').read_text()
        old = '    vita_mem_bad_access++;'
        assert source.count(old) == 1
        source = source.replace(old, old + '''
#ifdef _WIN32
    static int configured;
    static unsigned long stop_at;
    if (!configured) {
        const char *limit=getenv("PORT_STOP_AFTER_BAD_ACCESS");
        stop_at=limit ? strtoul(limit,NULL,10) : 0;
        configured=1;
    }
    if (stop_at && vita_mem_bad_access==stop_at) {
        void *stack[16];
        uintptr_t base=(uintptr_t)GetModuleHandleW(NULL);
        unsigned count=CaptureStackBackTrace(0,16,stack,NULL);
        fprintf(stderr,"PORT memory fault %u: address %08X, size %u, guest SP %08X\\n",
                vita_mem_bad_access,addr,len,sp);
        for (unsigned i=0;i<count;i++)
            if ((uintptr_t)stack[i]>=base)
                fprintf(stderr,"PORT stack RVA %llX\\n",(unsigned long long)((uintptr_t)stack[i]-base));
        exit(99);
    }
#endif
''')
        out_path.write_text(source)
    elif name == 'gxp_hlsl_port.c':
        source = (root / 'external/vitarecomp/src/gxp_hlsl.c').read_text()
        start_marker = '    /* A branch forward passes over the instructions up to its target: they'
        end_marker = '    if (g.failed || g.body.bad) {'
        assert source.count(start_marker) == 1 and source.count(end_marker) == 1
        start = source.index(start_marker)
        end = source.index(end_marker, start)
        replacement = (root / 'src/gxp_hlsl_branch.inc').read_text()
        source = source[:start] + replacement + '\n' + source[end:]
        out_path.write_text(source)
    elif name == 'hle_avplayer_port.c':
        source = (root / 'external/vitarecomp/src/hle_avplayer.c').read_text()
        old = '''    movie_reader reader = { p, 0, read_movie };
    if (wanted && wanted[0] == '0') snprintf(why, sizeof why, "VITARECOMP_MOVIES says none is");
    else if (!open_file(p, path_addr, path)) snprintf(why, sizeof why, "no such file");
    reader.size = p->file_bytes;
    movie_meanwhile = vita_clock_is_virtual() ? NULL : meanwhile;
    if (!why[0]) p->film = movie_open(&reader, why, sizeof why);
    if (!p->film) {'''
        replacements = [
            ('    movie          *film;', '    movie          *film;\n    int             skipped;'),
            ('    p->film = NULL;', '    p->film = NULL;\n    p->skipped = 0;'),
            ('    if (p->film) {', '    if (p->film || p->skipped) {'),
            (old, '''    /* This event handler identifies OPW3's archive-backed scene player.
     * Its file callbacks ignore the placeholder filename. Let the game
     * finish the scene through the ordinary Ready/Play/Stop callbacks. */
    p->skipped = p->event_function == 0x813A0EF1u && !strcmp(path, "dummy.mp4");
    if (p->skipped) {
        vita_hle_trace("sceAvPlayer: skipping archive-backed scene movie");
    } else {
        movie_reader reader = { p, 0, read_movie };
        if (wanted && wanted[0] == '0') snprintf(why, sizeof why, "VITARECOMP_MOVIES says none is");
        else if (!open_file(p, path_addr, path)) snprintf(why, sizeof why, "no such file");
        reader.size = p->file_bytes;
        movie_meanwhile = vita_clock_is_virtual() ? NULL : meanwhile;
        if (!why[0]) p->film = movie_open(&reader, why, sizeof why);
    }
    if (!p->film && !p->skipped) {'''),
            ('    if (!p->film || p->closing) return;',
             '    if ((!p->film && !p->skipped) || p->closing) return;'),
            ('''    if (!p || !p->film) { r0 = SCE_AVPLAYER_ERROR_INVALID_PARAM; return; }
    if (p->state == STATE_STOP) {''',
             '''    if (!p || (!p->film && !p->skipped)) { r0 = SCE_AVPLAYER_ERROR_INVALID_PARAM; return; }
    if (p->state == STATE_STOP) {'''),
            ('        (void)movie_rewind(p->film);', '        if (p->film) (void)movie_rewind(p->film);'),
            ('''        if (event == STATE_READY && p->auto_start && p->state == STATE_READY && !p->closing) begin(p);''',
             '''        if (event == STATE_READY && p->auto_start && p->state == STATE_READY && !p->closing) begin(p);
        if (event == STATE_PLAY && p->skipped && p->state == STATE_PLAY && !p->closing) {
            p->state = STATE_STOP;
            tell(p, STATE_STOP);
        }'''),
        ]
        for old, new in replacements:
            assert source.count(old) == 1, old
            source = source.replace(old, new)
        out_path.write_text(source)
