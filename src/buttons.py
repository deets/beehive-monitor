# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import machine

import const

wifi_mode_pin = None


def setup():
    global wifi_mode_pin
    wifi_mode_pin = machine.Pin(
        const.WIFI_MODE_PIN,
        machine.Pin.IN,
        machine.Pin.PULL_UP,
    )


def wifi_mode():
    if wifi_mode_pin.value():
        return "sta"
    return "ap"
