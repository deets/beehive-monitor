# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import sys
import pathlib
import pandas as pd
import plotly.express as px
from itertools import count
from collections import defaultdict

# Show the raw values or apply datasheet correction
RAW = False
# if existing, calibrate
CALIBRATE = True

K = 1 / (2**16 - 1)

CALIBRATION = {
    # The values I measured with my thermometer/hygrometer
    "temperature": [23.1, 17.2],
    # The values the sensor observed
    "sensors": {
        "04:44": {
            "temperature": [23.80102, 17.11719],
        },
        "04:45": {
            "temperature": [23.90784, 17.27207],
        },
        "05:44": {
            "temperature": [24.26032, 17.29343],
        },
        "05:45": {
            "temperature": [24.6789, 17.07179],
        },
        "06:44": {
            "temperature": [23.86511, 17.70199]
        },
        "06:45": {
            "temperature": [24.41787, 17.88357]
        },
        "07:44": {
            "temperature": [24.60479, 17.54444],
        },
        "07:45": {
            "temperature": [24.7837, 17.3132],
        },

    }
}


def compute_humidity(h):
    assert h.startswith("H")
    h = int(h[1:], 16)
    return h if RAW else 100 * K * h


def compute_temperature(id_, t):
    assert t.startswith("T")
    t = int(t[1:], 16)
    t = t if RAW else 175.0 * t * K - 45.0
    if not RAW and CALIBRATE and id_ in CALIBRATION["sensors"]:
        measured_a, measured_b = CALIBRATION["sensors"][id_]["temperature"]
        real_a, real_b = CALIBRATION["temperature"]
        slope = (real_b - real_a) / (measured_b - measured_a)
        intercept = real_a - slope * measured_a
        print((measured_a * slope + intercept) - real_a)
        print((measured_b * slope + intercept) - real_b)
        t = intercept + t * slope
    return t

def parse_data(filename):
    d = defaultdict(list)
    number_generator = defaultdict(count)
    for line in pathlib.Path(filename).open():
        line = line.strip()
        if line:
            assert line.startswith("#")
            parts = [p for p in line.split(",") if p.strip()][2:]
            for i in range(0, len(parts), 4):
                busno, address, humidity, temperature = parts[i:i+4]
                id_ = f"{busno}:{address}"
                humidity, temperature = compute_humidity(humidity), compute_temperature(id_, temperature)
                d["id"].append(id_)
                d["humidity"].append(humidity)
                d["temperature"].append(temperature)
                d["number"].append(next(number_generator[id_]))

    return pd.DataFrame(d)


def main():
    filename = sys.argv[1]
    data = parse_data(filename)

    for kind in ["temperature", "humidity"]:
        fig = px.line(
            data,
            x="number",
            y=kind,
            color="id",
            title=f"{kind}: {filename}"
        )
        fig.show()


if __name__ == '__main__':
    main()
