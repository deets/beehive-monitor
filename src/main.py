# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import uselect

import wifi
import webserver


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


nm = wifi.NetworkManager()
nm.run_ap()

poller = Poller()
webserver.WebServer(poller, nm)

while True:
    print("entering event loop")
    poller.poll()
