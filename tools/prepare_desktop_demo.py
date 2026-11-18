"""Wrap the viewer's material maps in the desktop renderer's PLTX format."""

import argparse
import json
import struct
from pathlib import Path


def prepare_materials(output: Path) -> None:
    assets = Path(__file__).resolve().parents[1] / "dist" / "assets"
    manifest = json.loads((assets / "manifest.json").read_text(encoding="utf-8"))
    size = manifest["texture_size"]
    header = struct.pack("<4sIII", b"PLTX", 1, size, size)
    expected_bytes = 2 * size * size * 4
    payloads = {}
    for name in ("ground", "bark", "rock"):
        pixels = (assets / f"{name}.rgba").read_bytes()
        if len(pixels) != expected_bytes:
            raise ValueError(f"Unexpected material size for {name}")
        payloads[name] = pixels

    output.mkdir(parents=True, exist_ok=True)
    for name, pixels in payloads.items():
        (output / f"{name}.pltex").write_bytes(header + pixels)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    prepare_materials(parser.parse_args().output)
