#!/usr/bin/python3
import sys
import pathlib
import logging
import re
import subprocess
from itertools import count


BASE = pathlib.Path(__file__).parent.parent.resolve()

CORE_DUMP_START = b"================= CORE DUMP START ================="
CORE_DUMP_END = b"================= CORE DUMP END ================="
APP_VERSION = b"App version:"
ESCAPE_REX = br'\x1b\[[0-9;]*m'

def coredump_file_creator(logfile):
    for i in count(1):
        dump = logfile.parent / f"{logfile.name}-{i}.core"
        logging.info(f"Creating {dump}")
        yield dump.open("wb")


def analyse_coredump(coredump, app_version):
    if app_version is None:
        logging.warn(f"Can't decode {coredump}, no app_version")
        return
    elf_file = BASE / f"idf/releases/{app_version}-ttgo-lora/beehive-ttgo-lora.elf"
    if not elf_file.exists():
        logging.warn(f"Can't decode {coredump}, no {elf_file}")
        return

    logging.info(f"Analysing {coredump}")
    cmd = [
        "espcoredump.py", "info_corefile",
        "--core", coredump.relative_to(BASE),
        "--core-format", "b64",
        elf_file,
    ]
    logging.debug(" ".join(str(c) for c in cmd))
    p = subprocess.run(cmd, check=True, capture_output=True, cwd=BASE)
    (coredump.parent / f"{coredump.name}.analysis").write_bytes(p.stdout)


def main():
    logging.basicConfig(level=logging.DEBUG)

    for logfile in (BASE / "logfiles").glob("*.TXT"):
        logging.info(f"Processing {logfile}")
        coredump_file_gen = coredump_file_creator(logfile)
        coredump = None
        app_version = None
        with logfile.open("rb") as inf:
            for line in inf:
                if APP_VERSION in line:
                    try:
                        app_version = re.sub(
                            ESCAPE_REX,
                            b"", line).decode().partition(
                                APP_VERSION.decode())[2].strip()
                    except UnicodeDecodeError:
                        pass # Can happen due to malformed logfiles
                if line.strip() == CORE_DUMP_START:
                    coredump = next(coredump_file_gen)
                    # ignore start line!
                elif line.strip() == CORE_DUMP_END:
                    # ignore end line!
                    coredump.close()
                    # ugly hack, but I don't care
                    analyse_coredump(pathlib.Path(coredump.name), app_version)
                    coredump = None
                    app_version = None
                elif coredump is not None:
                    coredump.write(line)



if __name__ == '__main__':
    main()
