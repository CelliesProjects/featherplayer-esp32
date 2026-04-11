#!/usr/bin/env python3

import sys
import configparser

def parse_pls(path):
    config = configparser.ConfigParser(interpolation=None)
    config.read(path, encoding="utf-8")

    if "playlist" not in config:
        raise ValueError("Invalid PLS file: missing [playlist] section")

    playlist = config["playlist"]

    entries = []
    i = 1
    while True:
        file_key = f"File{i}"
        title_key = f"Title{i}"

        if file_key not in playlist:
            break

        url = playlist[file_key].strip()
        title = playlist.get(title_key, f"Stream {i}").strip()

        entries.append((title, url))
        i += 1

    return entries


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <playlist.pls>")
        sys.exit(1)

    entries = parse_pls(sys.argv[1])

    for title, url in entries:
        # escape quotes just in case
        title = title.replace('"', '\\"')
        url = url.replace('"', '\\"')
        print(f'{{"{title}", "{url}"}},')


if __name__ == "__main__":
    main()
