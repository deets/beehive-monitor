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

        data = dict(
            time=[],
        )

        self._source = ColumnDataSource(
            data=data,
        )

        self._temperature_figure = figure(
            width=600,
            height=100,
            y_axis_label="Temperature",
            x_axis_type="datetime",
        )

        self._humidity_figure = figure(
            width=600,
            height=100,
            y_axis_label="Humidity",
            x_axis_type="datetime",
        )

        children = [self._temperature_figure, self._humidity_figure]

        self._layout = column(
            children,
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

    def _on_connect(self, client, userdata, flags, rc):
        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.
        client.subscribe("beehive/beehive")

    def _on_message(self, client, userdata, msg):
        #print(msg.topic, str(msg.payload))
        self._data_q.put(msg.payload)
        self._doc.add_next_tick_callback(self._process_data)

    def _add_graph(self, id_, temperature, humidity, data):
        data[f"temp-{id_}"] = [temperature] * len(data["time"])
        data[f"hum-{id_}"] = [humidity] * len(data["time"])
        self._temperature_figure.line(
            x="time",
            y=f"temp-{id_}",
            alpha=0.5,
            source=self._source,
        )
        self._humidity_figure.line(
            x="time",
            y=f"hum-{id_}",
            alpha=0.5,
            source=self._source,
        )

    def _process_data(self):
        # For some reason we get multiple callbacks
        # in the mainloop. So instead of relying on
        # one callback per line, we gather them
        # and process as many of them as we find.

        source = self._source

        for _ in range(self._data_q.qsize()):
            incoming_data = self._data_q.get()

            timestamp, sensor_data = process_payload(incoming_data)

            # this is needed to "clean up" the data
            # as bokeh otherwise complains in the update
            data = dict(source.data)
            data["time"].append(timestamp)
            data["time"] = data["time"][-SIZE:]
            time_len = len(data["time"])

            for id_, temperature, humidity in sensor_data:
                if f"temp-{id_}" not in data:
                    self._add_graph(id_, temperature, humidity, data)
                data[f"temp-{id_}"].append(temperature)
                data[f"temp-{id_}"] = data[f"temp-{id_}"][-time_len:]
                data[f"hum-{id_}"].append(humidity)
                data[f"hum-{id_}"] = data[f"hum-{id_}"][-time_len:]

            source.update(data=data)


def main():
    visualisation = Visualisation()
    visualisation.start_mqtt()


main()
