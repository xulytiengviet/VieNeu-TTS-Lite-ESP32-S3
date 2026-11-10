#!/usr/bin/env python3
"""Audit CVNSS4.0 *exported triples*, without pretending to implement its rules.

CSV schema: unicode_source,cvnss_ascii,unicode_restored
The exporter must be the real, independently verified CVNSS4.0 converter.
This tool checks Base27-compatible transport and NFC round-trip, NOT the
correctness of the underlying rulebook, acoustic phonemes, or ambiguity handling.
"""
from __future__ import annotations

import argparse
import csv
import json
import unicodedata
from pathlib import Path
from typing import Iterable, Mapping

from tools.base27 import encode, decode

COLUMNS = ("unicode_source", "cvnss_ascii", "unicode_restored")


def audit_rows(rows: Iterable[Mapping[str, str | None]]) -> dict:
    count = 0
    valid_transport = 0
    exact_unicode = 0
    errors: list[dict] = []
    for line, row in enumerate(rows, start=2):
        count += 1
        original = row.get("unicode_source")
        payload = row.get("cvnss_ascii")
        restored = row.get("unicode_restored")
        if original is None or payload is None or restored is None:
            errors.append({"line": line, "error": "missing CSV field"})
            continue
        try:
            if decode(encode(payload)) != payload:
                errors.append({"line": line, "error": "ASCII transport mismatch"})
                continue
            valid_transport += 1
        except ValueError as e:
            errors.append({"line": line, "error": f"invalid Base27 input: {e}"})
            continue
        if unicodedata.normalize("NFC", original) == unicodedata.normalize("NFC", restored):
            exact_unicode += 1
        else:
            errors.append({"line": line, "error": "Unicode NFC mismatch"})
    return {
        "rows": count,
        "valid_base27_roundtrips": valid_transport,
        "unicode_nfc_exact_matches": exact_unicode,
        "pass": count > 0 and count == exact_unicode == valid_transport,
        "errors": errors,
        "scope": "I/O transport + Unicode NFC only; CVNSS4.0 rule correctness/G2P not audited",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path, help="UTF-8 CSV exported by a real converter")
    args = parser.parse_args()
    try:
        with args.csv_path.open(encoding="utf-8-sig", newline="") as f:
            reader = csv.DictReader(f)
            if not reader.fieldnames or not set(COLUMNS).issubset(reader.fieldnames):
                parser.error(f"CSV must contain: {', '.join(COLUMNS)}")
            report = audit_rows(reader)
    except OSError as exc:
        parser.error(str(exc))
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
