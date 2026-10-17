#!/usr/bin/env python3
"""Build the bundled English emoji annotations from pinned CLDR XML files."""

import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def load_annotations(paths: list[Path]) -> dict[str, dict[str, str]]:
    annotations: dict[str, dict[str, str]] = {}
    for path in paths:
        for element in ET.parse(path).iter("annotation"):
            symbol = element.attrib["cp"].replace("\ufe0f", "")
            field = "name" if element.attrib.get("type") == "tts" else "keywords"
            value = (element.text or "").strip()
            if not value:
                continue
            if "\t" in value or "\n" in value or "\r" in value:
                raise ValueError(f"annotation contains a tab or newline: {symbol!r}")
            record = annotations.setdefault(symbol, {})
            if field in record and record[field] != value:
                raise ValueError(f"conflicting {field} for {symbol!r}")
            record[field] = value
    return annotations


def generate(emoji_test: Path, annotation_files: list[Path], output: Path) -> None:
    annotations = load_annotations(annotation_files)
    headers = [
        "# CLDR 48.2 English annotations. SPDX-License-Identifier: Unicode-3.0",
        "# Columns: emoji-test code points, CLDR short name, CLDR keywords (tab-separated)",
    ]
    rows = headers.copy()
    catalog_count = 0
    for line in emoji_test.read_text(encoding="utf-8").splitlines():
        if line.lstrip().startswith("#") or "; fully-qualified" not in line:
            continue
        code_points = line.split(";", 1)[0].split()
        symbol = "".join(chr(int(point, 16)) for point in code_points)
        catalog_count += 1
        record = annotations.get(symbol.replace("\ufe0f", ""))
        if record:
            rows.append(
                "\t".join((" ".join(code_points), record.get("name", ""),
                           record.get("keywords", "")))
            )
    output.write_text("\n".join(rows) + "\n", encoding="utf-8")
    covered = len(rows) - len(headers)
    print(f"Wrote {covered} annotations; {catalog_count - covered} use Unicode names")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("emoji_test", type=Path)
    parser.add_argument("annotations", type=Path)
    parser.add_argument("annotations_derived", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.emoji_test, [args.annotations, args.annotations_derived], args.output)


if __name__ == "__main__":
    main()
