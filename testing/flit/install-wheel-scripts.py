#!/usr/bin/env python3

import argparse
from pathlib import Path
from configparser import ConfigParser
from zipfile import ZipFile


parser = argparse.ArgumentParser(description="Install wheel scripts.")
parser.add_argument("wheel", type=Path, help="Wheel file")
parser.add_argument(
    "--prefix",
    type=Path,
    default=Path("/usr/local/"),
    help="Installation prefix (/usr/, ~/.local, …)",
)


def get_console_scripts(whl):
    with ZipFile(whl) as archive:
        n = next(
            iter(
                n
                for n in archive.namelist()
                if n.endswith(".dist-info/entry_points.txt")
            ),
            None,
        )
        if n is None:
            return {}
        ini = archive.read(n).decode("utf-8")
    parser = ConfigParser()
    parser.optionxform = str  # case sensitive
    parser.read_string(ini)
    return dict(parser["console_scripts"]) if "console_scripts" in parser else {}


def main():
    args = parser.parse_args()

    scripts = get_console_scripts(args.wheel)
    for name, defn in scripts.items():
        defn, *_ = defn.split(";")  # extra condition
        mod, fun = defn.split(":")
        if mod.endswith(".__main__"):
            mod = mod[: -len(".__main__")]
            code = f"""\
#!/bin/sh
exec /usr/bin/python3 -m {mod} "$@"
"""
        else:
            code = f"""\
#!/usr/bin/python3
from {mod} import {fun}
{fun}()
"""
        bin_path = args.prefix / "bin" / name
        bin_path.parent.mkdir(parents=True, exist_ok=True)
        bin_path.write_text(code)
        bin_path.chmod(0o755)


if __name__ == "__main__":
    main()
