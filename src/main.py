# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import uselect

import wifi
import webserver
import buttons


class Poller:

    def __init__(self):
        self._poll = uselect.poll()
        self._events = {}

    def register(self, fd, callback, flags=uselect.POLLIN):
        """
        callback must take Poller and fd as arguments
        """
        self._events[fd.fileno()] = callback
        self._poll.register(fd, flags)

    def unregister(self, fd):
        del self._events[fd.fileno()]
        self._poll.unregister(fd)

    def poll(self, timeout=-1):
        for event in self._poll.ipoll(timeout):
            print(event)
            fd = event[0]
            self._events[fd.fileno()](self, fd)


buttons.setup()
nm = wifi.NetworkManager()
poller = Poller()
webserver.WebServer(poller, nm)

print("entering event loop")

while True:
    if buttons.wifi_mode() == "ap":
        nm.run_ap()
    else:
        nm.run_sta()
    poller.poll(timeout=500)
