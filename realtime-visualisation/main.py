# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import paho.mqtt.client as mqtt

import threading
import queue
import argparse
import datetime as dt

import numpy as np

from bokeh.models import ColumnDataSource
from bokeh.plotting import curdoc, figure
from bokeh.layouts import column, row
from bokeh.models import FactorRange, Range1d
from bokeh.models import Select


def process_sensor_payload(sensor_payload):
    id_, temperature, humidity = sensor_payload.split(",")
    assert temperature[0] == "T"
    assert humidity[0] == "H"
    return id_, int(temperature[1:], 16), int(humidity[1:], 16)


def process_payload(payload):
    header, *sensors = payload.decode("ascii").split(";")
    _sequence, timestamp = header.split(",")
    timestamp = dt.datetime.fromisoformat(timestamp.split("+")[0])
    sensors = [process_sensor_payload(sensor) for sensor in sensors]
    return timestamp, sensors


SIZE = 100


class Visualisation:

    def __init__(self):
        opts = self._parse_args()

        self._data_q = queue.Queue()
        doc = self._doc = curdoc()

        self._sources = {}
        self._layout = column(
            [],
            sizing_mode="scale_width"
        )
        doc.add_root(self._layout)

    def _parse_args(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("-o", "--output", help="Record data into file")
        return parser.parse_args()

    def start_mqtt(self):
        t = threading.Thread(target=self._mqtt_task)
        t.daemon = True
        t.start()

    def _mqtt_task(self):
        client = mqtt.Client()
        client.on_connect = self._on_connect
        client.on_message = self._on_message

        client.connect("singlemalt.local", 1883, 60)

        # Blocking call that processes network traffic, dispatches callbacks and
        # handles reconnecting.
        # Other loop*() functions are available that give a threaded interface and a
        # manual interface.
        client.loop_forever()

    # The callback for when the client receives a CONNACK response from the server.
    def _on_connect(self, client, userdata, flags, rc):
        print("Connected with result code "+str(rc))

        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.
        client.subscribe("beehive/beehive")

    # The callback for when a PUBLISH message is received from the server.
    def _on_message(self, client, userdata, msg):
        print(msg.topic, str(msg.payload))
        self._data_q.put(msg.payload)
        self._doc.add_next_tick_callback(self._process_data)

    def _add_graph(self, id_, timestamp, temperature):
        raw_data = dict(
            time=[timestamp],
            temperature=[temperature]
        )

        source = ColumnDataSource(
                data=raw_data,
        )
        raw_figure = figure(
            width=600,
            height=100,
        )
        raw_figure.line(
            x="time",
            y="temperature",
            alpha=0.5,
            source=source,
        )
        children = self._layout.children
        children.append(raw_figure)
        self._layout.update(children=children)
        self._sources[id_] = source

    def _process_data(self):
        # For some reason we get multiple callbacks
        # in the mainloop. So instead of relying on
        # one callback per line, we gather them
        # and process as many of them as we find.

        for _ in range(self._data_q.qsize()):
            data = self._data_q.get()

            timestamp, sensor_data = process_payload(data)
            for id_, t, h in sensor_data:
                print(timestamp, t)

                if id_ not in self._sources:
                    self._add_graph(id_, timestamp, t)
                else:
                    source = self._sources[id_]
                    data = dict(source.data)
                    if len(data["time"]) >= SIZE:
                        data["time"] = data["time"][-SIZE:]
                        data["temperature"] = data["temperature"][-SIZE:]
                    data["time"].append(timestamp)
                    data["temperature"].append(t)
                    source.update(data=data)


def main():
    visualisation = Visualisation()
    visualisation.start_mqtt()


main()
