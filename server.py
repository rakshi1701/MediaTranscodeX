#!/usr/bin/env python3
"""
Media_TranscodeX Web Local Dev Server
Serves the web/ directory with Cross-Origin-Opener-Policy (COOP) 
and Cross-Origin-Embedder-Policy (COEP) headers required for SharedArrayBuffer / WebAssembly.
"""

import http.server
import socketserver
import os

PORT = 8080
DIRECTORY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")

class CrossOriginIsolatedHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

if __name__ == "__main__":
    # Allow port reuse
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), CrossOriginIsolatedHandler) as httpd:
        print(f"🚀 Media_TranscodeX Web server running at http://localhost:{PORT}")
        print("🔒 Cross-Origin Isolation (COOP/COEP) enabled for SharedArrayBuffer WebAssembly support.")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")
