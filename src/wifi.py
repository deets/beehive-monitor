# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import network
import json

import const


class NetworkManager:

    def __init__(self):
        self._ap = None
        self._sta = None
        self._networks = self._load_networks()

    def run_ap(self):
        if self._ap is None:
            self._shutdown()
            ap = self._ap = network.WLAN(network.AP_IF)
            ap.active(True)
            ap.config(essid=const.SSID)
            ap.config(max_clients=const.MAX_CLIENTS)
            ap.config(dhcp_hostname=const.HOSTNAME)
            ap.active(True)

    def run_sta(self):
        if self._sta is None:
            self._shutdown()
            sta = self._sta = network.WLAN(network.STA_IF)
            # optimize if we are already connected
            if not sta.isconnected():
                print('connecting to network...')
                sta.active(True)
                ssid, password = self._determine_network()
                if ssid is not None:
                    sta.connect(ssid, password)
                    while not sta.isconnected():
                        pass
                else:
                    print("no network configured!")
                    return
            print('network config:', sta.ifconfig())

    def _determine_network(self):
        scanned_networks = self._sta.scan()
        for candidate in scanned_networks:
            ssid = candidate[0].decode("ascii")
            if ssid in self._networks["networks"]:
                return ssid, self._networks["networks"][ssid]
        return None, None

    def _shutdown(self):
        if self._sta is not None:
            self._sta.disconnect()
            self._sta = None
        if self._ap is not None:
            self._ap = None

    def _load_networks(self):
        try:
            with open("networks.json") as inf:
                return json.load(inf)
        except (OSError, ValueError):
            return {
                "last": None,
                "networks": {}
            }

    def _save(self):
        with open("networks.json", "wb") as outf:
            json.dump(self._networks, outf)

    @property
    def networks(self):
        return self._networks["networks"]

    def remove_network(self, ssid):
        if ssid in self._networks["networks"]:
            del self._networks["networks"][ssid]
            self._save()

    def add_network(self, ssid, password=None):
        self._networks["networks"][ssid] = password
        self._save()
