"""Loopback web host and bounded, read-only decrypted game-asset ranges."""
import argparse
import re
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'generated/decrypted/PCSE00638'


class Handler(SimpleHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm',
                      '.js': 'text/javascript', '.json': 'application/json'}

    def log_message(self, format, *args):
        # Silence asset streaming logs to prevent high console I/O latency
        if hasattr(self, 'path') and self.path.startswith('/__assets/'):
            return
        super().log_message(format, *args)

    def do_HEAD(self):
        if self.path.startswith('/__assets/'):
            self.asset(head=True)
        else:
            super().do_HEAD()

    def do_GET(self):
        if self.path.startswith('/__assets/'):
            self.asset(head=False)
        else:
            super().do_GET()

    def asset(self, head):
        name=unquote(urlsplit(self.path).path[len('/__assets/'):])
        parts=name.split('/')
        if not name or any(p in ('..','.') or '\\' in p or '\x00' in p or ':' in p for p in parts):
            self.send_error(403);return
        # Never expose licences, PFS metadata, bundled modules or executable.
        if parts[0] not in ('FILE_NGP','LINKDATA_NGP') and name!='sce_sys/param.sfo':
            self.send_error(403);return
        target=(ASSETS/name).resolve()
        if not target.is_relative_to(ASSETS.resolve()):
            self.send_error(403);return
        if parts[0] in ('FILE_NGP','LINKDATA_NGP'):
            allowed=(ASSETS/parts[0]).resolve()
            if not allowed.is_relative_to(ASSETS.resolve()) or not target.is_relative_to(allowed):
                self.send_error(403);return
        elif target!=(ASSETS/'sce_sys/param.sfo').absolute():
            self.send_error(403);return
        if not target.exists():
            self.send_error(404);return
        directory=target.is_dir();size=0 if directory else target.stat().st_size
        if head:
            self.send_response(200);self.send_header('Content-Length',str(size))
            self.send_header('X-Asset-Kind','directory' if directory else 'file')
            self.send_header('Accept-Ranges','bytes');self.end_headers();return
        match=re.fullmatch(r'bytes=(\d+)-(\d+)',self.headers.get('Range',''))
        if directory or not match:
            self.send_error(416,'An explicit bounded byte range is required');return
        start,end=map(int,match.groups())
        if start>end or start>=size or end>=size or end-start+1>1024*1024:
            self.send_response(416);self.send_header('Content-Range',f'bytes */{size}')
            self.send_header('Content-Length','0');self.end_headers();return
        try:
            with target.open('rb') as f:
                f.seek(start);chunk=f.read(end-start+1)
        except OSError:
            self.send_error(404);return
        if len(chunk)!=end-start+1:
            self.send_error(409,'Asset changed during read');return
        self.send_response(206);self.send_header('Content-Type','application/octet-stream')
        self.send_header('Content-Length',str(len(chunk)))
        self.send_header('Content-Range',f'bytes {start}-{end}/{size}')
        self.send_header('Accept-Ranges','bytes');self.end_headers();self.wfile.write(chunk)

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        if hasattr(self, 'path') and self.path.startswith('/__assets/'):
            self.send_header('Cache-Control', 'public, max-age=3600')
        else:
            self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def list_directory(self, path):
        self.send_error(403, 'Directory listing disabled')
        return None


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--port', type=int, default=8080)
    args = p.parse_args()
    root = ROOT / 'web'
    server = ThreadingHTTPServer(('127.0.0.1', args.port), partial(Handler, directory=str(root)))
    print(f'Browser host: http://127.0.0.1:{args.port} (Ctrl+C to stop)', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.server_close()
