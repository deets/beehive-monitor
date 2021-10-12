# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import pathlib
import argparse
import datetime as dt
from collections import defaultdict

from bokeh.models import ColumnDataSource
from bokeh.plotting import curdoc, figure
from bokeh.layouts import column, row
from bokeh.io import show

WIDTH, HEIGHT = 800, 800
TOOLS = "pan,wheel_zoom,box_zoom,reset,hover"


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
    timestamp = from_isoformat(timestamp)
    sensors = [process_sensor_payload(sensor) for sensor in sensors]
    return timestamp, sensors


def from_isoformat(timestamp):
    return dt.datetime.fromisoformat(timestamp.split("+")[0])


def load_data(path):
    path = pathlib.Path(path)
    res = defaultdict(list)
    for line in path.read_bytes().split(b"\n"):
        line = line.strip()
        if not line:
            continue
        timestamp, values = process_payload(line)
        for sensor_id, temperature, humidity in values:
            res[sensor_id].append(
                (timestamp,
                 raw2temperature(temperature),
                 raw2humidity(humidity))
            )
    return res


def filter_data(data, from_, to):
    res = {}
    for id_, values in data.items():
        res[id_] = [
            (timestamp, temperature, humidity)
            for timestamp, temperature, humidity in values
            if timestamp >= from_ and timestamp <= to
        ]
    return res


def from_testo_format(ts_string):
    return dt.datetime.strptime(ts_string[1:-1], "%Y-%m-%d %H:%M:%S")


def load_testo(path, from_, to):
    path = pathlib.Path(path)
    lines = path.read_text().split("\n")
    assert "Berlin CEST" in lines[0]
    offset = dt.timedelta(hours=-2)

    def process(line):
        ts, temperature, humidity = line.split(";")
        return (
            from_testo_format(ts) + offset,
            float(temperature),
            float(humidity)
            )

    res = [process(line) for line in lines[1:] if line.strip()]

    return [(ts, temperature, humidity)
            for ts, temperature, humidity in res
            if ts >= from_ and ts <= to
            ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--testo", help="Testo Information")
    parser.add_argument(
        "--reference-id",
        help="The reference sensor ID. "
             "If not given, defaults to lowest.")
    parser.add_argument(
        "--from",
        dest="from_",
        type=from_isoformat,
        help="Filter data to be greater or equal to this timepoint. "
             "Defaults to start.")
    parser.add_argument(
        "--to",
        type=from_isoformat,
        help="Filter data to be less or equal to this timepoint. "
             "Defaults to end.")
    parser.add_argument("data")

    opts = parser.parse_args()
    data = load_data(opts.data)

    if opts.from_ is None:
        opts.from_ = min(data[next(iter(data))])[0]
    if opts.to is None:
        opts.to = max(data[next(iter(data))])[0]

    data = filter_data(data, opts.from_, opts.to)
    reference_id = min(data.keys()) if opts.reference_id is None \
        else opts.reference_id

    ts_reference, temp_reference, hum_reference = zip(*data[reference_id])

    row_figures = []

    if opts.testo:
        ts_testo, temp_testo, hum_testo = zip(*load_testo(opts.testo, opts.from_, opts.to))
        testo_temperature = figure(
            width=WIDTH,
            height=HEIGHT,
            y_axis_label=f"Temperature {reference_id}",
            x_axis_label="Time",
            tools=TOOLS,
            )
        testo_temperature.line(
            x=ts_reference,
            y=temp_reference,
        )
        testo_temperature.scatter(
            x=ts_testo,
            y=temp_testo,
        )
        testo_humidity = figure(
            width=WIDTH,
            height=HEIGHT,
            y_axis_label=f"Humidity {reference_id}",
            x_axis_label="Time",
            tools=TOOLS,
            )
        testo_humidity.line(
            x=ts_reference,
            y=hum_reference,
        )
        testo_humidity.scatter(
            x=ts_testo,
            y=hum_testo,
        )
        row_figures.append(row(testo_temperature, testo_humidity))

    for id_, values in data.items():
        if id_ == reference_id:
            continue
        temp_y = [t for _ts, t, _h in data[id_]]
        hum_y = [h for _ts, _t, h in data[id_]]

        temperature_figure = figure(
            width=WIDTH,
            height=HEIGHT,
            y_axis_label=f"Sensor {id_}",
            x_axis_label="Reference",
            tools=TOOLS,
        )
        temperature_figure.line(
            x=temp_reference,
            y=temp_y,
        )
        humidity_figure = figure(
            width=WIDTH,
            height=HEIGHT,
            y_axis_label=f"Sensor {id_}",
            x_axis_label="Reference",
            tools=TOOLS,
        )
        humidity_figure.line(
            x=hum_reference,
            y=hum_y,
        )
        row_figures.append(row(temperature_figure, humidity_figure))

    layout = column(
        row_figures,
        sizing_mode="scale_width"
    )
    show(layout)


if __name__ == '__main__':
    main()
