import machine
import ustruct


class SHT3XDIS:

    STATUS = bytearray([0xF3, 0x2D])
    MEASUREMENT = bytearray([0x24, 0x00])
    MEASUREMENT_TIME = 20 # according to datasheet, 15. I add a bit extra.
    RESET = bytearray([0x30, 0xA2])
    CLEAR = bytearray([0x30, 0x41])

    def __init__(self, i2c, address=0x44):
        self._i2c = i2c
        self._address = address

    def reset(self):
        self._i2c.writeto(self._address, self.RESET)

    def clear(self):
        self._i2c.writeto(self._address, self.CLEAR)

    @property
    def status(self):
        self._i2c.writeto(self._address, self.STATUS)
        v = self._i2c.readfrom(self._address, 3)
        return v

    @property
    def values(self):
        self._i2c.writeto(self._address, self.MEASUREMENT)
        machine.sleep(self.MEASUREMENT_TIME)
        v = self._i2c.readfrom(self._address, 6)
        humidity, _, temp, _ = ustruct.unpack(">HBHB", v)
        return (
            float(humidity) / 65535.0 * 100,
            -45.0 + 175.0 * float(temp) / 65535.0
        )


def test():
    scl = machine.Pin(27, machine.Pin.IN, machine.Pin.PULL_UP)
    sda = machine.Pin(26, machine.Pin.IN, machine.Pin.PULL_UP)
    i2c = machine.SoftI2C(scl=scl, sda=sda, freq=400000)

    sensor = SHT3XDIS(i2c)
    sensor.reset()
    machine.sleep(20)
    sensor.clear()
    machine.sleep(20)
    print(sensor.status)
    print(sensor.values)
