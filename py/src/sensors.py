# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import const
import machine
import sht3xdis
import tca9548a


def handle_sht3xdis(sensor, id_, emit):
    values = sensor.raw_values
    emit("sht3xdis", id_, values)


KNOWN_ADDRESSES = {
    0x44: (sht3xdis.SHT3XDIS, handle_sht3xdis),
    0x45: (sht3xdis.SHT3XDIS, handle_sht3xdis),
}

SENSORS = {}


def setup():
    scl = machine.Pin(const.SCL, machine.Pin.IN, machine.Pin.PULL_UP)
    sda = machine.Pin(const.SDA, machine.Pin.IN, machine.Pin.PULL_UP)
    i2c = machine.SoftI2C(scl=scl, sda=sda, freq=400000)
    mux = tca9548a.TCA9548A(i2c, reset=13)

    try:
        for busno in range(8):
            bus = mux.bus(busno)
            scan = bus.scan()
            print(busno, scan)
            for address in scan:
                if address in KNOWN_ADDRESSES:
                    class_, moniker = KNOWN_ADDRESSES[address]
                    sensor = class_(bus, address=address)
                    id_ = (busno, address)
                    # SENSORS[id_] = \
                    #     lambda emit, sensor=sensor, id_=id_: moniker(sensor, id_, emit)
                    SENSORS[id_] = sensor
    except OSError:
        pass


def test(if_s):
    import umqtt.robust2 as mqtt
    try:
        setup()
    except OSError:
        print("No sensors found")
    print(SENSORS)

    def periodic_query():
        client = mqtt.MQTTClient("beehive", "192.168.1.108")
        client.connect()
        print("client connected")

        def emit(kind, id_, values):
            print(kind, id_, values)
            client.publish(b"foobar", b"value", qos=1)

        while True:
            machine.sleep(500)
            for handler in SENSORS.values():
                handler(emit)
            client.publish(b"foobar", b"value", qos=1)
            client.wait_msg()

    periodic_query()
