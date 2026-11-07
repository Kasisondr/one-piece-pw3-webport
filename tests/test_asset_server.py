"""HTTP boundaries and actual bytes, with synthetic assets in a temporary root."""
import importlib.util
import tempfile
import threading
import unittest
from functools import partial
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer
from pathlib import Path

spec=importlib.util.spec_from_file_location('port_server',Path(__file__).resolve().parents[1]/'tools/serve.py')
server_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(server_module)

class AssetServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.root=Path(cls.temp.name)
        (cls.root/'FILE_NGP').mkdir();cls.data=bytes(range(256))*8200
        (cls.root/'FILE_NGP/range.bin').write_bytes(cls.data)
        (cls.root/'sce_sys/package').mkdir(parents=True)
        (cls.root/'sce_sys/package/work.bin').write_bytes(b'private licence fixture')
        server_module.ASSETS=cls.root
        class QuietHandler(server_module.Handler):
            def log_message(self,*args):pass
        cls.server=ThreadingHTTPServer(('127.0.0.1',0),partial(QuietHandler,directory=str(cls.root)))
        cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True);cls.thread.start()
    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown();cls.server.server_close();cls.thread.join();cls.temp.cleanup()
    def request(self,path,method='GET',range_=None):
        c=HTTPConnection('127.0.0.1',self.server.server_port,timeout=5)
        c.request(method,path,headers={'Range':range_} if range_ else {})
        r=c.getresponse();result=(r.status,dict(r.getheaders()),r.read());c.close();return result
    def test_metadata(self):
        code,h,body=self.request('/__assets/FILE_NGP/range.bin','HEAD')
        self.assertEqual((code,int(h['Content-Length']),body),(200,len(self.data),b''))
        self.assertEqual(h['Cross-Origin-Embedder-Policy'],'require-corp')
        self.assertEqual(h['X-Asset-Kind'],'file')
        code,h,_=self.request('/__assets/FILE_NGP','HEAD');self.assertEqual((code,h['X-Asset-Kind']),(200,'directory'))
    def test_ranges_cross_pages_and_eof(self):
        for start,end in [(0,31),(262139,262151),(len(self.data)-16,len(self.data)-1)]:
            code,h,body=self.request('/__assets/FILE_NGP/range.bin',range_=f'bytes={start}-{end}')
            self.assertEqual(code,206);self.assertEqual(body,self.data[start:end+1])
            self.assertEqual(h['Content-Range'],f'bytes {start}-{end}/{len(self.data)}')
    def test_rejects_unbounded_and_invalid_ranges(self):
        for value in [None,'bytes=0-','bytes=-1-30','bytes=4-3','bytes=0-99999999','bytes=0-1048576','bytes=0-3,8-9','bytes=4294967296-4294967300']:
            self.assertEqual(self.request('/__assets/FILE_NGP/range.bin',range_=value)[0],416)
    def test_private_files_and_traversal(self):
        for path in ['sce_sys/package/work.bin','sce_pfs/files.db','eboot.bin','sce_module/libult.suprx','FILE_NGP/%2e%2e/sce_sys/package/work.bin','FILE_NGP/..%5csecret','FILE_NGP/C:%5csecret']:
            self.assertEqual(self.request('/__assets/'+path,'HEAD')[0],403)
    def test_missing_and_read_only(self):
        self.assertEqual(self.request('/__assets/FILE_NGP/missing','HEAD')[0],404)
        self.assertEqual(self.request('/__assets/FILE_NGP/range.bin','POST')[0],501)

if __name__=='__main__':unittest.main()
