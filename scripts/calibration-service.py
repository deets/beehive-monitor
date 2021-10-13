import json
import argparse
import pathlib
import datetime as dt
from functools import partial

import paho.mqtt.client as mqtt

from dataclasses import dataclass

PREFIX = "beehive"
ROLAND_TOPIC = "B-value"

# This suffix is appended to the system name to indicate
# the values have been calibrated
SUFFIX = "calibrated"

# Change this mapping to alias the names of the columns
# as well as the sensor aliases
COLUMN_MAPPING = dict(
    # Column one
    klima=[
        ("0545", "oben"),
        ("0544", "mitte-oben"),
        ("0445", "mitte-unten"),
        ("0444", "unten")
    ],
    # Column two
    kontrolle=[
        ("0745", "oben"),
        ("0744", "mitte-oben"),
        ("0645", "mitte-unten"),
        ("0644", "unten"),
    ],
)


def IDENTITY(x):
    return x


@dataclass
class SensorValues:
    temperature: float
    humidity: float

    @classmethod
    def from_raw(cls, temperature, humidity):
        return cls(
            temperature=raw2temperature(temperature),
            humidity=raw2humidity(humidity),
        )


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
    sequence, timestamp = header.split(",")
    timestamp = dt.datetime.fromisoformat(timestamp.split("+")[0])
    sensors = {
        id_: SensorValues.from_raw(temperature, humidity)
        for id_, temperature, humidity in
        (process_sensor_payload(sensor) for sensor in sensors)
    }
    return sequence, timestamp, sensors


def produce_roland_messages(name, cvs, sequence):
    res = []
    for column_name, ids in COLUMN_MAPPING.items():
        system_name = f"{name}-{column_name}-{SUFFIX}"
        values = [
            f"{alias},{cvs[id_].temperature},{cvs[id_].humidity}"
            for id_, alias in ids
        ]
        payload = ":".join([f"{system_name},{sequence}"] + values)
        res.append((ROLAND_TOPIC, payload))
    return res


def clamp16bit(value):
    return max(0, min(int(value), 65535))


def temperature2raw(temperature):
    # temperature * 175.0 / 65535.0 - 45.0
    return clamp16bit((temperature + 45) / 175.0 * 65535.0)


def humidity2raw(humidity):
    return clamp16bit(humidity / 100.0 * 65535.0)


def native_remapping(cvs):
    res = []
    for id_, values in sorted(cvs.items()):
        res.append(
            f"{id_},"
            f"T{temperature2raw(values.temperature):04x},"
            f"H{humidity2raw(values.humidity):04x}"
        )
    return res


def produce_calibrated_native_message(name, cvs, timestamp, sequence):
    topic = f"{PREFIX}-{SUFFIX}/{name}"
    payload = ";".join(
        [f"{sequence},{timestamp.isoformat()}"] +
        native_remapping(cvs)
    )
    return topic, payload


def generate_calibrated_messages(name, payload, calibrations):
    sequence, timestamp, sensor_values = process_payload(payload)
    cvs = {}
    for id_, values in sensor_values.items():
        cvs[id_] = calibrations[id_](values)

    messages = produce_roland_messages(name, cvs, sequence)
    messages.append(produce_calibrated_native_message(name, cvs, timestamp, sequence))

    return messages


def apply_calibration(calibration, v):
    return SensorValues(
        temperature=calibration["temperature"]["slope"] * v.temperature +
        calibration["temperature"]["intercept"],
        humidity=calibration["humidity"]["slope"] * v.humidity +
        calibration["humidity"]["intercept"]
    )


def load_calibrations(path):
    with path.open() as inf:
        data = json.load(inf)

    reference_id = data["reference_id"]

    testo_calibration = IDENTITY
    if "testo-calibration" in data:
        testo_calibration = partial(
            apply_calibration,
            data["testo-calibration"],
        )

    assert reference_id not in data["calibrations"]

    calibrations = {
        reference_id: testo_calibration,
    }

    for id_, calibration in data["calibrations"].items():
        inner_function = partial(
            apply_calibration,
            calibration,
        )
        calibrations[id_] = lambda values: testo_calibration(inner_function(values))

    return calibrations


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mqtt-host", default="localhost")
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--mqtt-topic", default=f"{PREFIX}/#")
    parser.add_argument("--calibration", required=True, type=pathlib.Path)
    opts = parser.parse_args()

    calibrations = load_calibrations(opts.calibration)

    def on_connect(*_args):
        client.subscribe(opts.mqtt_topic)

    def on_message(client, userdata, msg):
        name = msg.topic[len(f"{PREFIX}/"):]
        for topic, payload in generate_calibrated_messages(
                name, msg.payload, calibrations
            ):
            print(topic, payload)
            client.publish(topic, payload)

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(opts.mqtt_host, opts.mqtt_port, 60)
    client.loop_forever()


if __name__ == '__main__':
    main()
