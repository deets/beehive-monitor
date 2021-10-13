# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import pathlib
import argparse
import datetime as dt
import json
from collections import defaultdict

from bokeh.plotting import figure
from bokeh.layouts import column, row
from bokeh.models.annotations import Label
from bokeh.io import show
import scipy.stats

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


def calibrate(current, reference, figure):
    r = scipy.stats.linregress(
        x=current,
        y=reference,
        alternative='two-sided'
    )
    a, b = current[0], current[-1]
    ca, cb = (
        a * r.slope + r.intercept,
        b * r.slope + r.intercept,
    )
    figure.line(
        x=[a, b],
        y=[ca, cb],
        color="red",
    )

    rps = Label(
        x=min(current),
        y=(max(reference) - min(reference)) * .8 + min(reference),
        x_units="data",
        y_units="data",
        text=f"slope={r.slope}\nintercept={r.intercept}\n"
             f"rvalue={r.rvalue}\npvalue={r.pvalue}\n"
             f"stderr={r.stderr}\nintercept_stderr={r.intercept_stderr}",
        render_mode='css',
        border_line_color='black', border_line_alpha=1.0,
        background_fill_color='white',
        background_fill_alpha=1.0
    )
    figure.add_layout(rps)
    return dict(slope=r.slope, intercept=r.intercept)


def compute(data, id_, temp_reference, hum_reference):
    temp_current = [t for _ts, t, _h in data[id_]]
    hum_current = [h for _ts, _t, h in data[id_]]

    temperature_figure = figure(
        width=WIDTH,
        height=HEIGHT,
        x_axis_label=f"Sensor {id_}",
        y_axis_label="Reference",
        tools=TOOLS,
    )
    temperature_figure.line(
        y=temp_reference,
        x=temp_current,
    )
    humidity_figure = figure(
        width=WIDTH,
        height=HEIGHT,
        x_axis_label=f"Sensor {id_}",
        y_axis_label="Reference",
        tools=TOOLS,
    )
    humidity_figure.line(
        y=hum_reference,
        x=hum_current,
    )

    calibration = dict(
        temperature=calibrate(temp_current, temp_reference, temperature_figure),
        humidity=calibrate(hum_current, hum_reference, humidity_figure)
    )

    return calibration, temperature_figure, humidity_figure


def pair(ts_testo, reference, ts_reference):
    res = []
    h = list(zip(ts_reference, reference))
    for ts_testo in ts_testo:
        _ts, reference_v = min((abs(ts_testo - ts), v) for ts, v in h)
        res.append(reference_v)
    return res


def compute_testo(testo_data, temp_reference, hum_reference, ts_reference):
    ts_testo, temp_testo, hum_testo = zip(*testo_data)
    temp_paired = pair(ts_testo, temp_reference, ts_reference)
    hum_paired = pair(ts_testo, hum_reference, ts_reference)

    testo_temperature = figure(
        width=WIDTH,
        height=HEIGHT,
        y_axis_label="Testo",
        x_axis_label="Temperature",
        tools=TOOLS,
        )
    testo_temperature.line(
        y=temp_testo,
        x=temp_paired,
    )
    testo_humidity = figure(
        width=WIDTH,
        height=HEIGHT,
        y_axis_label="Testo",
        x_axis_label="Humidity",
        tools=TOOLS,
        )
    testo_humidity.line(
        y=hum_testo,
        x=hum_paired,
    )
    calibration = dict(
        temperature=calibrate(temp_paired, temp_testo, testo_temperature),
        humidity=calibrate(hum_paired, hum_testo, testo_humidity),
    )
    return calibration, testo_temperature, testo_humidity


def make_meta(opts):
    meta = dict(data=opts.data)
    if opts.testo:
        meta["testo"] = opts.testo
    if opts.from_:
        meta["from"] = opts.from_.isoformat()
    if opts.to:
        meta["to"] = opts.to.isoformat()

    return meta


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

    calibration = dict(
        __meta__= make_meta(opts),
        reference_id=reference_id,
        )

    ts_reference, temp_reference, hum_reference = zip(*data[reference_id])

    row_figures = []

    if opts.testo:
        testo_data = load_testo(opts.testo, opts.from_, opts.to)
        calibration["testo-calibration"], testo_temperature, testo_humidity = \
            compute_testo(
                testo_data,
                temp_reference,
                hum_reference,
                ts_reference,
            )
        row_figures.append(row(testo_temperature, testo_humidity))

    calibration["calibrations"] = sensor_calibrations = {}
    for id_, values in data.items():
        if id_ == reference_id:
            continue

        sensor_calibrations[id_], temperature_figure, humidity_figure = \
            compute(
                data,
                id_,
                temp_reference,
                hum_reference,
            )
        row_figures.append(row(temperature_figure, humidity_figure))

    layout = column(
        row_figures,
        sizing_mode="scale_width"
    )
    show(layout)
    print(json.dumps(calibration, indent=4))


if __name__ == '__main__':
    main()
