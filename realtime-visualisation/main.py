# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.

import paho.mqtt.client as mqtt

import threading
import queue
import argparse
import datetime as dt

import pathlib

from bokeh.models import ColumnDataSource
from bokeh.plotting import curdoc, figure
from bokeh.layouts import column


def regroup_line(sdcard_data):
    # the V2 format contains a trailing , because it's easier to write that.
    sequence, timestamp, *rest = sdcard_data.rstrip(",").split(",")
    entries = []
    for index in range(0, len(rest), 4):
        busno, address, humidity, temperature = rest[index:index + 4]
        entry = f"{busno}{address},{temperature},{humidity}"
        entries.append(entry)

    return ";".join([f"{sequence},{timestamp}"] + entries)


def raw2humidity(humidity):
    return humidity * 100 / 65535.0


def raw2temperature(temperature):
    return temperature * 175.0 / 65535.0 - 45.0


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


class Visualisation:

    def __init__(self):
        opts = self._parse_args()

        self._topic = opts.topic

        self._size = opts.size

        if opts.input is None:
            self._acquisition_task = self._mqtt_task
        else:
            self._acquisition_task = lambda: self._acquire_from_file(opts.input)

        self._temperature_converter = lambda x: x
        self._humidity_converter = lambda x: x

        if opts.convert:
            self._temperature_converter = raw2temperature
            self._humidity_converter = raw2humidity

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
            height=200,
            y_axis_label="Temperature",
            x_axis_type="datetime",
        )

        self._humidity_figure = figure(
            width=600,
            height=200,
            y_axis_label="Humidity",
            x_axis_type="datetime",
        )

        children = [self._temperature_figure, self._humidity_figure]

        self._layout = column(
            children,
            sizing_mode="scale_width"
        )
        doc.add_root(self._layout)
        self._writer = lambda payload: None
        if opts.output:
            outf = open(opts.output, "wb")

            def writer(payload):
                outf.write(payload)
                if not payload[-1] == b"\n":
                    outf.write(b"\n")
                outf.flush()

            self._writer = writer

    def _parse_args(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("-o", "--output", help="Record data into file")
        parser.add_argument("-i", "--input", help="Load data from this file instead of MQTT")
        parser.add_argument("-s", "--size", type=int, default=None, help="The limit of measurements shown.")
        parser.add_argument("-c", "--convert", action="store_true", default=False, help="Convert acconding to SHT3XDIS datasheet.")
        parser.add_argument("--topic", default="beehive/beehive")
        return parser.parse_args()

    def start_acquisition(self):
        t = threading.Thread(target=self._acquisition_task)
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

    def _acquire_from_file(self, path):
        path = pathlib.Path(path)
        # Data from the SD-Card itself
        if path.name.startswith("BEE"):
            self._acquire_from_sdcard_data(path)
        else:
            with path.open("rb") as inf:
                for line in inf:
                    line = line.strip()
                    if line:
                        self._data_q.put(line)
                        self._doc.add_next_tick_callback(self._process_data)

    def _acquire_from_sdcard_data(self, path):
        all_files = sorted(path.parent.glob(path.name))
        for file_ in all_files:
            for line in file_.read_text().split("\n"):
                line = line.strip()
                if line:
                    assert line.startswith("#V2,")
                    line = regroup_line(line[4:]).encode("ascii")
                    self._data_q.put(line)
        self._doc.add_next_tick_callback(self._process_data)

    def _on_connect(self, client, userdata, flags, rc):
        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.
        client.subscribe(self._topic)

    def _on_message(self, client, userdata, msg):
        #print(msg.topic, str(msg.payload))
        self._data_q.put(msg.payload)
        self._writer(msg.payload)
        self._doc.add_next_tick_callback(self._process_data)

    def _add_graph(self, id_, temperature, humidity, data):
        data[f"temp-{id_}"] = [temperature] * len(data["time"])
        data[f"hum-{id_}"] = [humidity] * len(data["time"])
        self._temperature_figure.line(
            x="time",
            y=f"temp-{id_}",
            alpha=0.5,
            source=self._source,
            legend_label=f"{id_}C"
        )
        self._humidity_figure.line(
            x="time",
            y=f"hum-{id_}",
            alpha=0.5,
            source=self._source,
            legend_label=f"{id_}%"
        )

        for p in [self._temperature_figure, self._humidity_figure]:
            p.legend.click_policy="hide"

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

            if self._size is not None:
                data["time"] = data["time"][-self._size:]

            time_len = len(data["time"])

            for id_, temperature, humidity in sensor_data:
                temperature = self._temperature_converter(temperature)
                humidity = self._humidity_converter(humidity)
                if f"temp-{id_}" not in data:
                    self._add_graph(id_, temperature, humidity, data)
                data[f"temp-{id_}"].append(temperature)
                data[f"temp-{id_}"] = data[f"temp-{id_}"][-time_len:]
                data[f"hum-{id_}"].append(humidity)
                data[f"hum-{id_}"] = data[f"hum-{id_}"][-time_len:]

            source.update(data=data)


def main():
    visualisation = Visualisation()
    visualisation.start_acquisition()


main()
