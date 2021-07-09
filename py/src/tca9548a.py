# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import time
import machine


class Bus:

    def __init__(self, mux, channel):
        self._mux = mux
        self._channel = channel

    def writeto(self, address, data):
        self._mux.select_channel(self._channel)
        return self._mux._i2c.writeto(address, data)

    def readfrom(self, address, count):
        self._mux.select_channel(self._channel)
        return self._mux._i2c.readfrom(address, count)

    def scan(self):
        self._mux.select_channel(self._channel)
        return self._mux._i2c.scan()


class TCA9548A:

    def __init__(self, i2c, address=0x70, reset=-1):
        self._i2c = i2c
        self._address = address
        self._channels = 0
        self._reset = machine.Pin(
            reset,
            machine.Pin.OUT,
            machine.Pin.PULL_UP) if reset != -1 else None
        self.reset()

    def reset(self):
        if self._reset is not None:
            self._reset.value(0)
            time.sleep_ms(10)
            self._reset.value(1)

    def select_channel(self, channel):
        channel = 1 << channel
        if channel != self._channels:
            self._i2c.writeto(self._address, bytearray([channel]))
            time.sleep_ms(10)
            self._channels = channel

    def selected_channels(self):
        bits = self._i2c.readfrom(self._address, 1)[0]
        res = []
        for i in range(8):
            if (1 << i) & bits:
                res.append(i)
        return tuple(res)

    def bus(self, channel):
        return Bus(self, channel)


def test():
    import machine
    import sht3xdis
    import const

    scl = machine.Pin(const.SCL, machine.Pin.IN, machine.Pin.PULL_UP)
    sda = machine.Pin(const.SDA, machine.Pin.IN, machine.Pin.PULL_UP)
    i2c = machine.SoftI2C(scl=scl, sda=sda, freq=400000)
    mux = TCA9548A(i2c, reset=13)
    sensor = sht3xdis.SHT3XDIS(mux.bus(7))

    reset = True
    while True:
        try:
            if reset:
                sensor.reset()
                sensor.clear()
                reset = False
            print(sensor.values)
            print(sensor.status)
            machine.sleep(100)
        except OSError:
            reset = True
