/* The 64-bit fourth argument is aligned onto the guest stack (AAPCS).
 * The scheduler serializes guest execution and none of these stdio calls
 * yields, so restoring the stream position is atomic with respect to guests.
 */
static void port_positioned_io(int writing){
    int i=fd_index(r0);
    if(i<0 || !g_fds[i].is_file){r0=SCE_ERROR_ERRNO_EBADF;return;}
    uint32_t buffer=r1,size=r2;
    if(!vita_mem_ptr(sp,8)){r0=SCE_ERROR_ERRNO_EINVAL;return;}
    int64_t offset=(int64_t)(((uint64_t)vita_read32(sp+4)<<32)|vita_read32(sp));
    if(offset<0 || size>0x7FFFFFFFu){r0=SCE_ERROR_ERRNO_EINVAL;return;}
    if(!size){r0=0;return;}
    void *bytes=vita_mem_ptr(buffer,size);
    if(!bytes){r0=SCE_ERROR_ERRNO_EINVAL;return;}
    FILE *f=g_fds[i].stream;
    int64_t saved=io_tell(f);
    if(saved<0 || io_seek(f,offset,SEEK_SET)){r0=SCE_ERROR_ERRNO_EINVAL;return;}
    clearerr(f);
    size_t n=writing?fwrite(bytes,1,size,f):fread(bytes,1,size,f);
    int failed=ferror(f);
    if(writing && fflush(f))failed=1;
    if(io_seek(f,saved,SEEK_SET))failed=1;
    r0=failed?0x80010005u:(uint32_t)n;
}
void vita_hle_sceIoPread(void){port_positioned_io(0);}
void vita_hle_sceIoPwrite(void){port_positioned_io(1);}
