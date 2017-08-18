#!/usr/bin/env python3

"""
So far, just a server to test things with
"""

import random
import re

from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import unquote_plus

def main():
    handler = SimpleHTTPRequestHandler
    server_address = ('', 8192)
    httpd = HTTPServer(server_address, handler)
    httpd.serve_forever()

if __name__ == '__main__':
	main()
