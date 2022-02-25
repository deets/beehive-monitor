# Copyright: 2021, Diez B. Roggisch, Berlin . All rights reserved.
import tkinter as tk
import urllib.request
import threading
import queue
import json
import time


class App:

    def __init__(self, root):
        self._root = root
        self._default_color = self._root["background"]
        upper = tk.Frame(root)
        upper.pack(side=tk.TOP)
        self._system_name = tk.StringVar()
        self._system_name.set("beehive")
        system_name = tk.Entry(upper, textvariable=self._system_name)
        system_name.pack(side=tk.LEFT)
        tk.Label(upper, text=".local").pack(side=tk.RIGHT)
        self._lower = tk.Frame(root)
        self._lower.pack(side=tk.TOP)

        self._configuration = dict(
            app_version=(False, tk.StringVar()),
            mqtt_hostname=(True, tk.StringVar()),
            sleeptime=(True, tk.IntVar()),
            system_name=(True, tk.StringVar()),
        )
        for row, (name, (editable, var)) in enumerate(self._configuration.items()):
            tk.Label(self._lower, text=f"{name}:").grid(row=row, column=0)
            if editable:
                control = tk.Entry(self._lower, textvariable=var)
            else:
                control = tk.Label(self._lower, textvariable=var)
            control.grid(row=row, column=1)

        tk.Button(self._lower, text="Load", command=self._load).grid(
            row=row + 1, column=0
        )
        self._save_button = tk.Button(self._lower, text="Save", command=self._save)
        self._save_button.grid(
            row=row + 1, column=1
        )
        self._loaded = False

        self._last_config = None
        self._enable(False)
        self._config_queue = queue.Queue()
        t = threading.Thread(target=self._poll_configuration)
        t.daemon = True
        t.start()
        self._check_configuration()

    @property
    def _url(self):
        # This can be called from the background thread because
        # tk vars are alledgedly thread safe
        return f"http://{self._system_name.get()}.local/configuration"

    def _poll_configuration(self):
        while True:
            data = None
            try:
                req = urllib.request.urlopen(self._url)
            except (UnicodeError, urllib.error.URLError):
                # UnicodeError occurs on system_name being empty
                pass
            else:
                try:
                    data = json.loads(req.read())
                except json.decoder.JSONDecodeError:
                    pass
            self._config_queue.put(data)
            time.sleep(.5)

    def _check_configuration(self):
        for _ in range(self._config_queue.qsize()):
            data = self._config_queue.get()
            self._enable(data is not None)
            self._last_config = data
        self._root.after(200, self._check_configuration)

    def _enable(self, enable):
        state = "normal" if enable else "disabled"
        for child in self._lower.winfo_children():
            child.configure(state=state)
        self._save_button.configure(
            state="normal" if enable and self._loaded else "disabled"
        )

    def _load(self):
        if self._last_config is not None:
            for name, value in self._last_config.items():
                self._configuration[name][1].set(value)
        self._loaded = True
        self._enable(True)

    def _save(self):
        data = {
            name : var.get()
            for name, (_, var) in self._configuration.items()
        }
        req = urllib.request.Request(self._url)
        req.add_header("Content-Type", "application/json")
        jsondata = json.dumps(data)
        jsondataasbytes = jsondata.encode("utf-8")   # needs to be bytes
        req.add_header("Content-Length", len(jsondataasbytes))
        try:
            urllib.request.urlopen(req, jsondataasbytes)
            self._flash("green")
        except:
            self._flash("red")

    def _flash(self, color):
        self._colorize(self._root, color)
        self._root.after(
            2000,
            lambda: self._colorize(self._root, self._default_color)
        )

    def _colorize(self, widget, color):
        widget["background"] = color
        for child in widget.winfo_children():
            self._colorize(child, color)


def main():
    root = tk.Tk()
    app = App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
