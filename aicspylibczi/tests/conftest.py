#!/usr/bin/env python
# -*- coding: utf-8 -*-

import threading
from functools import partial
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import pytest


@pytest.fixture
def data_dir() -> Path:
    return Path(__file__).parent / "resources"


class RangeRequestHandler(BaseHTTPRequestHandler):
    """Serves a directory over http with byte-range support."""

    protocol_version = "HTTP/1.1"

    def __init__(self, *args, directory: Path, **kwargs):
        self._root = directory.resolve()
        super().__init__(*args, **kwargs)

    def _resolve(self):
        candidate = (self._root / self.path.lstrip("/")).resolve()
        if self._root not in candidate.parents or not candidate.is_file():
            return None
        return candidate

    def do_GET(self):
        target = self._resolve()
        if target is None:
            self.send_error(404)
            return

        data = target.read_bytes()
        range_header = self.headers.get("Range")

        if range_header is None:
            body = data
            status = 200
            content_range = None
        else:
            # libCZI sends "bytes=<start>-<end>" with an inclusive end.
            start_text, _, end_text = range_header.partition("=")[2].partition("-")
            start = int(start_text)
            end = int(end_text) if end_text else len(data) - 1
            end = min(end, len(data) - 1)
            if start > end:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{len(data)}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            stop = end + 1
            body = data[start:stop]
            status = 206
            content_range = f"bytes {start}-{end}/{len(data)}"

        self.send_response(status)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Accept-Ranges", "bytes")
        if content_range is not None:
            self.send_header("Content-Range", content_range)
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


@pytest.fixture
def data_server(data_dir):
    """Serve the test resources over http, yielding the base URL."""
    handler = partial(RangeRequestHandler, directory=data_dir)
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    server.daemon_threads = True
    # libCZI keeps the connection alive, so a handler thread is always parked waiting for
    # the next request and server_close() would join it and never return
    server.block_on_close = False
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{server.server_address[1]}"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
