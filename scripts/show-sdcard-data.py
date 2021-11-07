# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import pathlib
import argparse
import datetime as dt
from collections import defaultdict

from bokeh.plotting import figure
from bokeh.layouts import column, row
from bokeh.models.annotations import Label
from bokeh.io import show
import scipy.stats

WIDTH, HEIGHT = 800, 800
TOOLS = "pan,wheel_zoom,box_zoom,reset,hover,save"


def raw2humidity(humidity):
    return humidity * 100 / 65535.0


def raw2temperature(temperature):
    return temperature * 175.0 / 65535.0 - 45.0


def from_isoformat(timestamp):
    return dt.datetime.fromisoformat(timestamp.split("+")[0])


def process_sensors(sensor_parts):
    if sensor_parts:
        res = {}
        for index in range(0, len(sensor_parts), 4):
            bus, address, humidity, temperature = sensor_parts[index:index+4]
            assert humidity.startswith("H")
            assert temperature.startswith("T")
            res[f"{bus}{address}"] = dict(
                humidity=raw2humidity(int(humidity[1:], 16)),
                temperature=raw2temperature(int(temperature[1:], 16)),
            )
        return res


def load_data_file(logfile):
    res = []
    with logfile.open("r") as inf:
        for line in inf:
            line = line.strip()
            if line:
                assert line.startswith("#V2")
                # strip off the trailing comma, as it othewise confguses
                # the rest of the process
                _, _, timestamp, *sensors = line.rstrip(",").split(",")
                timestamp = from_isoformat(timestamp)
                sensors = process_sensors(sensors)
                if sensors and timestamp.year != 1970:
                    res.append((timestamp, sensors))
    return res


def transpose(data):
    res = defaultdict(list)
    for timestamp, sensor_values in data:
        for id_, readings in sensor_values.items():
            res[id_].append(
                dict(
                    timestamp=timestamp,
                    temperature=readings["temperature"],
                    humidity=readings["humidity"],
                )
            )
    return res


def load_data(path):
    path = pathlib.Path(path)
    data = []
    for logfile in path.glob("BEE*.TXT"):
        data.extend(load_data_file(logfile))
    data.sort()
    data = transpose(data)
    return data


def from_testo_format(ts_string):
    return dt.datetime.strptime(ts_string[1:-1], "%Y-%m-%d %H:%M:%S")


def load_testo(path):
    path = pathlib.Path(path)
    lines = path.read_text().split("\n")
    if "Berlin CEST" in lines[0]:
        pass
    elif "Berlin CET" in lines[0]:
        pass
    else:
        assert False, "No timezone in TESTO-data!"

    # The offset should be calculated based on the actual
    # timepoint, but I recorded the data before the CEST -> CET
    # switch, so I can hard-code
    offset = dt.timedelta(hours=-2)

    def process(line):
        ts, temperature, humidity = line.split(";")
        return (
            from_testo_format(ts) + offset,
            float(temperature),
            float(humidity)
            )

    res = [process(line) for line in lines[1:] if line.strip()]

    return [
        dict(timestamp=ts, temperature=temperature, humidity=humidity)
        for ts, temperature, humidity in res
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--testo", help="Testo Information")
    parser.add_argument("--index-of", type=int)
    parser.add_argument("directory")

    opts = parser.parse_args()
    data = load_data(opts.directory)

    if opts.testo:
        testo_data = load_testo(opts.testo)
        data["testo"] = testo_data

    temperature_figure = figure(
        width=WIDTH,
        height=HEIGHT,
        tools=TOOLS,
        x_axis_type="datetime",
    )
    humidity_figure = figure(
        width=WIDTH,
        height=HEIGHT,
        tools=TOOLS,
        x_axis_type="datetime",
    )

    index_of = opts.index_of

    for id_, values in data.items():
        x = [entry["timestamp"] for entry in values]
        humidities = [entry["humidity"] for entry in values]
        temperatures = [entry["temperature"] for entry in values]

        start_index = index_of if id_ != "testo" else None
        humidity_figure.line(
            x=x[start_index:],
            y=humidities[start_index:],
            legend_label=f"{id_}%",
            line_color="blue" if id_ != "testo" else "orange",
        )
        temperature_figure.line(
            x=x[start_index:],
            y=temperatures[start_index:],
            legend_label=f"{id_}C",
            line_color="blue" if id_ != "testo" else "orange",
        )

    for p in [temperature_figure, humidity_figure]:
        p.legend.click_policy = "hide"

    layout = column(
        [
         temperature_figure,
         humidity_figure
        ],
        sizing_mode="scale_width"
    )
    show(layout)


if __name__ == '__main__':
    main()
