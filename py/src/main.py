# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import uselect
import gc
import wifi
import webserver
import buttons
import sensors
import machine

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
nm.run_sta()
poller = Poller()
webserver.WebServer(poller, nm)

print("entering event loop")

sensors.setup()

import micropython
import umqtt.robust2 as mqtt
client = mqtt.MQTTClient("beehive", "192.168.1.108")
client.connect()
print("client connected")

while True:
    for i in range(10):
        print("start", i)
        client.publish(b"foobar", b"start")
        machine.sleep(100)

    # for sensor in sensors.SENSORS.values():
    #     humidity, temp = sensor.raw_values
    #     print(humidity, temp)
    #     client.publish(b"foobar", str(humidity), qos=1)
    machine.sleep(100)
    gc.collect()
    #micropython.mem_info(1)

while True:
    if buttons.wifi_mode() == "ap":
        nm.run_ap()
    else:
        nm.run_sta()
    poller.poll(timeout=500)
