# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import socket
import index_tpl


class WebServer:

    def __init__(self, poller, network_manager):
        self._nm = network_manager
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 80))
        s.listen(5)
        poller.register(s, self.incoming_connection)

    def render(self, conn):
        for block in index_tpl.render(self._nm.networks):
            conn.sendall(block)

    def serve_request(self, poller, conn):
        request = conn.recv(1024)
        request = str(request)
        conn.sendall('HTTP/1.1 200 OK\n')
        conn.sendall('Content-Type: text/html\n')
        conn.sendall('Connection: close\n\n')
        self.render(conn)
        poller.unregister(conn)
        conn.close()

    def incoming_connection(self, poller, s):
        conn, _addr = s.accept()
        poller.register(conn, self.serve_request)
