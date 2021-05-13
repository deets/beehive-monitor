import machine


class SHT3XDIS:

    STATUS = bytearray([0xF3, 0x2D])

    def __init__(self, i2c, address=0x44):
        self._i2c = i2c
        self._address = address

    @property
    def status(self):
        self._i2c.writeto(self._address, self.STATUS)
        v = self._i2c.readfrom(self._address, 3)
        return v


def test():
    scl = machine.Pin(12, machine.Pin.IN, machine.Pin.PULL_UP)
    sda = machine.Pin(14, machine.Pin.IN, machine.Pin.PULL_UP)
    i2c = machine.SoftI2C(scl=scl, sda=sda, freq=400000)
    print(i2c.scan())
    sensor = SHT3XDIS(i2c)
    print(sensor.status)
