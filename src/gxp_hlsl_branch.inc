    /* Relaxed forward-branch translation for USSE shaders:
     * Overlapping or nested forward branches skip instructions using _skip_to.
     */
    int has_branch = 0;
    for (uint32_t i = 0; i < p->primary_count; i++) {
        if (p->primary[i].op == USSE_OP_BR) has_branch = 1;
    }
    if (has_branch) {
        put(&g.body, "    uint _skip_to = 0u;\n");
    }
    for (uint32_t i = 0; i < p->primary_count && !g.failed; i++) {
        const usse_insn *in = &p->primary[i];
        char line[1024];
        usse_state shown = g.st;                    /* the text of an instruction, with the increments as they stand */
        size_t n = usse_disassemble(in, &shown, line, sizeof(line));
        for (size_t at = 0; at < n;) {
            size_t end = at;
            while (end < n && line[end] != '\n') end++;
            put(&g.body, "    // %.*s\n", (int)(end - at), line + at);
            at = end + 1;
        }
        if (in->op == USSE_OP_PHAS || in->op == USSE_OP_NOP || in->op == USSE_OP_SPEC) continue;
        if (in->op == USSE_OP_BR) {
            int64_t target = (int64_t)i + (int32_t)in->imm;
            if ((int32_t)in->imm <= 0) { fail(&g, "a branch backward"); break; }
            if (target > (int64_t)p->primary_count) {
                fail(&g, "a branch past the end of the program");
                break;
            }
            const char *cond = "true";
            char cond_buf[32];
            switch (in->pred) {
            case USSE_PRED_P0: case USSE_PRED_P1: case USSE_PRED_P2: case USSE_PRED_P3:
                snprintf(cond_buf, sizeof(cond_buf), "PR%d", in->pred - USSE_PRED_P0);
                cond = cond_buf;
                break;
            case USSE_PRED_NOT_P0: cond = "!PR0"; break;
            case USSE_PRED_NOT_P1: cond = "!PR1"; break;
            default: cond = "true"; break;
            }
            put(&g.body, "    if (_skip_to <= %uu && %s) { if (%uu > _skip_to) _skip_to = %uu; }\n",
                i, cond, (uint32_t)target, (uint32_t)target);
            continue;
        }
        const char *cond = NULL;
        char cond_buf[32];
        switch (in->pred) {
        case USSE_PRED_P0: case USSE_PRED_P1: case USSE_PRED_P2: case USSE_PRED_P3:
            snprintf(cond_buf, sizeof(cond_buf), "PR%d", in->pred - USSE_PRED_P0);
            cond = cond_buf;
            break;
        case USSE_PRED_NOT_P0: cond = "!PR0"; break;
        case USSE_PRED_NOT_P1: cond = "!PR1"; break;
        default: cond = NULL; break;
        }
        if (has_branch && cond) {
            put(&g.body, "    if (_skip_to <= %uu && %s) {\n", i, cond);
        } else if (has_branch) {
            put(&g.body, "    if (_skip_to <= %uu) {\n", i);
        } else if (cond) {
            put(&g.body, "    if (%s) {\n", cond);
        } else {
            put(&g.body, "    {\n");
        }
        emit(&g, in);
        put(&g.body, "    }\n");
    }
