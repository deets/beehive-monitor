# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import network

import const

AP = None


def run_ap():
    global AP
    AP = network.WLAN(network.AP_IF)
    AP.config(essid=const.SSID)
    AP.config(max_clients=const.MAX_CLIENTS)
    AP.config(dhcp_hostname=const.HOSTNAME)
    AP.active(True)
