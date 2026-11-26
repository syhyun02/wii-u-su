#!/usr/bin/env python3
"""Static server for web/ with the headers SharedArrayBuffer needs (COOP/COEP).
usage: scripts/serve-web.py [port]   -> http://localhost:8080/"""
import http.server, os, sys
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'web')
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=ROOT, **k)
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()
H.extensions_map['.wasm'] = 'application/wasm'
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
print(f'serving {os.path.abspath(ROOT)} on http://localhost:{port}/')
http.server.ThreadingHTTPServer(('', port), H).serve_forever()
