# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import network
import json

import const


class NetworkManager:

    def __init__(self):
        self._ap = None
        self._networks = self._load_networks()

    def run_ap(self):
        ap = self._ap = network.WLAN(network.AP_IF)
        ap.config(essid=const.SSID)
        ap.config(max_clients=const.MAX_CLIENTS)
        ap.config(dhcp_hostname=const.HOSTNAME)
        ap.active(True)

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
