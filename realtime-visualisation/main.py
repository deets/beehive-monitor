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

        raw_data = dict(
            time=[dt.datetime.now()] * SIZE,
            temperature=[20.0] * SIZE
        )

        self._raw_source = ColumnDataSource(
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
            source=self._raw_source,
        )

        children = [raw_figure]

        layout = column(
            children,
            sizing_mode="scale_width"
        )
        doc.add_root(layout)

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

    def _process_data(self):
        # For some reason we get multiple callbacks
        # in the mainloop. So instead of relying on
        # one callback per line, we gather them
        # and process as many of them as we find.

        for _ in range(self._data_q.qsize()):
            data = self._data_q.get()

            temperature = self._raw_source.data['temperature']
            # slice of oldest value
            temperature = temperature[1:]

            time = self._raw_source.data['time']
            # slice of oldest value
            time = time[1:]

            timestamp, sensor_data = process_payload(data)
            _id, t, h = sensor_data[0]
            print(timestamp, t)

            # append the new readings to the existing array
            temperature.append(t)
            temperature = np.array(temperature, dtype=np.single)
            time.append(timestamp)

            patch = dict(
                temperature=[(slice(0, len(temperature)), temperature)],
                time=[(slice(0, len(time)), time)],
            )
            self._raw_source.patch(patch)


def main():
    visualisation = Visualisation()
    visualisation.start_mqtt()


main()
