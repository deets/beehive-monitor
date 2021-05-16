# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import socket
import index_tpl


def urlparse(url):
    protocol = None
    host = None
    path = None
    query = None
    if "://" in url:
        protocol, url = protocol.split("://")
        host, url = url.split("/", 1)
    if "?" in url:
        url, query = url.split("?")
    path = url
    return protocol, host, path, query


def qparse(query):
    res = {}
    if query is not None:
        for parameter in query.split("&"):
            key, value = parameter.split("=")
            res[key] = value
    return res


class WebServer:

    def __init__(self, poller, network_manager):
        self._nm = network_manager
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', 80))
        s.listen(5)
        poller.register(s, self.incoming_connection)

    def render(self, conn, message):
        for block in index_tpl.render(self._nm.networks, message):
            conn.sendall(block)

    def serve_request(self, poller, conn):
        request = conn.recv(1024)
        request = str(request)

        message = self._process_request(request)

        conn.sendall('HTTP/1.1 200 OK\n')
        conn.sendall('Content-Type: text/html\n')
        conn.sendall('Connection: close\n\n')
        self.render(conn, message)

        poller.unregister(conn)
        conn.close()

    def incoming_connection(self, poller, s):
        conn, _addr = s.accept()
        poller.register(conn, self.serve_request)

    def _process_request(self, request):
        try:
            path = request.split("\r\n")[0].split()[1]
            print(path)
            _protocol, _host, path, query = urlparse(path)
            query = qparse(query)
            return self._dispatch(path, query)
        except IndexError:
            pass

    def _dispatch(self, path, query):
        handler = self.HANDLERS.get(path, lambda self, query: None)
        try:
            return handler(self, query)
        except Exception as e:
            return '<p class="error">{}</p>'.format(str(e))

    def _remove_network(self, query):
        network = query.get("network")
        if network in self._nm.networks:
            self._nm.remove_network(network)
            return "Removed <b>{}</b>".format(network)

    HANDLERS = {
        "/remove-network": _remove_network
    }
