/* Read-only app0 streams. Runs synchronous range requests on the game pthread,
 * never on the browser UI thread, and caches one 256 KiB page per open file.
 * stdio for the ELF and save directory remains the Emscripten filesystem. */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <limits.h>
#define WEB_PAGE 262144u
typedef struct {
    int active,error;int64_t size,position,cached_at;size_t cached;
    char path[1024];uint8_t *page;
} web_stream;
static web_stream web_streams[32];
static web_stream *web_lookup(FILE *file) {
    for(int i=0;i<32;i++)if(web_streams[i].active && file==(FILE*)&web_streams[i])return &web_streams[i];
    return NULL;
}
static int web_path(const char *p){return !strncmp(p,"/app0/",6);}
typedef struct {
    char path[256];
    int64_t size;
} web_stat_cache_entry;
#define WEB_STAT_CACHE_MAX 512
static web_stat_cache_entry web_stat_cache[WEB_STAT_CACHE_MAX];
static int web_stat_cache_count = 0;

static int64_t web_size(const char *path) {
    for (int i = 0; i < web_stat_cache_count; i++) {
        if (!strcmp(web_stat_cache[i].path, path)) return web_stat_cache[i].size;
    }
    double size=EM_ASM_DOUBLE({
        try {
            const request=new XMLHttpRequest();
            request.open('HEAD','/__assets/'+UTF8ToString($0).slice(6).split('/').map(encodeURIComponent).join('/'),false);
            request.send();if(request.status!==200)return -1;
            if(request.getResponseHeader('X-Asset-Kind')==='directory')return -2;
            const n=Number(request.getResponseHeader('Content-Length'));
            return Number.isSafeInteger(n)&&n>=0?n:-1;
        }catch(e){return -1;}
    },path);
    int64_t s = (int64_t)size;
    if (web_stat_cache_count < WEB_STAT_CACHE_MAX) {
        snprintf(web_stat_cache[web_stat_cache_count].path, sizeof(web_stat_cache[0].path), "%s", path);
        web_stat_cache[web_stat_cache_count].size = s;
        web_stat_cache_count++;
    }
    return s;
}
static int web_stat(const char *path,struct stat *st) {
    if(!web_path(path))return stat(path,st);
    int64_t size=web_size(path);if(size==-1)return -1;
    memset(st,0,sizeof *st);st->st_mode=(size==-2?S_IFDIR:S_IFREG)|0444;
    st->st_size=size<0?0:size;return 0;
}
static FILE *web_open(const char *path,const char *mode) {
    if(!web_path(path))return fopen(path,mode);
    if(strcmp(mode,"rb"))return NULL;
    int64_t size=web_size(path);if(size<0)return NULL;
    for(int i=0;i<32;i++)if(!web_streams[i].active) {
        web_stream *s=&web_streams[i];memset(s,0,sizeof *s);
        s->page=malloc(WEB_PAGE);if(!s->page)return NULL;
        s->active=1;s->size=size;s->cached_at=-1;snprintf(s->path,sizeof s->path,"%s",path);
        return (FILE*)s;
    }
    return NULL;
}
static int web_close(FILE *f) {
    web_stream *s=web_lookup(f);if(!s)return fclose(f);
    free(s->page);memset(s,0,sizeof *s);return 0;
}
static int web_seek(FILE *f,int64_t offset,int whence) {
    web_stream *s=web_lookup(f);if(!s)return fseeko(f,offset,whence);
    int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?s->position:whence==SEEK_END?s->size:-1;
    if(base<0 || offset < -base || (offset>0 && base>INT64_MAX-offset))return -1;
    s->position=base+offset;return 0;
}
static int64_t web_tell(FILE *f){web_stream *s=web_lookup(f);return s?s->position:ftello(f);}
static size_t web_read(void *out,size_t size,size_t count,FILE *f) {
    web_stream *s=web_lookup(f);if(!s)return fread(out,size,count,f);
    if(!size || !count)return 0;
    if(count>SIZE_MAX/size){s->error=1;return 0;}
    size_t wanted=size*count,done=0;
    if(s->position>=s->size)return 0;
    if((uint64_t)wanted>(uint64_t)(s->size-s->position))wanted=(size_t)(s->size-s->position);
    while(done<wanted) {
        int64_t page=s->position&~(int64_t)(WEB_PAGE-1);
        if(page!=s->cached_at) {
            uint32_t bytes=(uint32_t)(s->size-page<WEB_PAGE?s->size-page:WEB_PAGE);
            int fetched=EM_ASM_INT({
                try {
                    const request=new XMLHttpRequest();
                    request.open('GET','/__assets/'+UTF8ToString($0).slice(6).split('/').map(encodeURIComponent).join('/'),false);
                    request.responseType='arraybuffer';
                    const start=$2;const end=start+$3-1;
                    request.setRequestHeader('Range',`bytes=${start}-${end}`);request.send();
                    if(request.status!==206 || request.getResponseHeader('Content-Range')!==`bytes ${start}-${end}/${$4}` || request.response.byteLength!==$3)return -1;
                    HEAPU8.set(new Uint8Array(request.response),$1);return $3;
                }catch(e){return -1;}
            },s->path,s->page,(double)page,bytes,(double)s->size);
            if(fetched!=(int)bytes){s->error=1;break;}
            s->cached_at=page;s->cached=bytes;
        }
        size_t within=(size_t)(s->position-page),n=s->cached-within;
        if(n>wanted-done)n=wanted-done;
        if(!n){s->error=1;break;}
        memcpy((uint8_t*)out+done,s->page+within,n);done+=n;s->position+=n;
    }
    return done/size;
}
static size_t web_write(const void *p,size_t size,size_t count,FILE *f) {
    web_stream *s=web_lookup(f);if(!s)return fwrite(p,size,count,f);
    s->error=1;return 0;
}
static int web_flush(FILE *f){web_stream *s=web_lookup(f);return s?0:fflush(f);}
static int web_error(FILE *f){web_stream *s=web_lookup(f);return s?s->error:ferror(f);}
static void web_clear(FILE *f){web_stream *s=web_lookup(f);if(s)s->error=0;else clearerr(f);}
#undef host_stat
#undef io_seek
#undef io_tell
#define host_stat web_stat
#define io_seek web_seek
#define io_tell web_tell
#define fopen web_open
#define fclose web_close
#define fread web_read
#define fwrite web_write
#define fflush web_flush
#define ferror web_error
#define clearerr web_clear
#endif
