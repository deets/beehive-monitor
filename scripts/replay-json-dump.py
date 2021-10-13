import json
import argparse
import time

import paho.mqtt.client as mqtt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mqtt-host", default="localhost")
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--loop", action="store_true")
    parser.add_argument("--wait", type=float, default=1.0)
    parser.add_argument("input")
    opts = parser.parse_args()

    client = mqtt.Client()
    client.connect(opts.mqtt_host, opts.mqtt_port, 60)

    while True:
        with open(opts.input) as inf:
            for line in inf:
                message = json.loads(line)
                client.publish(message["topic"], message["payload"])
                time.sleep(opts.wait)
            if not opts.loop:
                break


if __name__ == '__main__':
    main()
