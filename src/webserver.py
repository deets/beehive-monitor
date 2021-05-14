# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import socket


def serve():
    html = """<html><head> <title>ESP Web Server</title> <meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,"> <style>html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center;}
h1{color: #0F3376; padding: 2vh;}p{font-size: 1.5rem;}.button{display: inline-block; background-color: #e7bd3b; border: none;
border-radius: 4px; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
.button2{background-color: #4286f4;}</style></head><body> <h1>ESP Web Server</h1>
<p>GPIO state: <strong>  </strong></p><p><a href="/?led=on"><button class="button">ON</button></a></p>
<p><a href="/?led=off"><button class="button button2">OFF</button></a></p></body></html>"""
    return html


def serve_request(poller, conn):
    request = conn.recv(1024)
    request = str(request)
    response = serve()
    conn.sendall('HTTP/1.1 200 OK\n')
    conn.sendall('Content-Type: text/html\n')
    conn.sendall('Connection: close\n\n')
    conn.sendall(response)
    poller.unregister(conn)
    conn.close()


def incoming_connection(poller, s):
    conn, _addr = s.accept()
    poller.register(conn, serve_request)


def register(poller):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('', 80))
    s.listen(5)
    poller.register(s, incoming_connection)
