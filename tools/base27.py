#!/usr/bin/env python3
"""Strict, invertible Base27 transport encoding (A-Z + SPACE -> three 0/1/2 trits).

This is NOT the CVNSS4.0 converter and is NOT a Vietnamese grapheme-to-phoneme
engine. Supply an independently audited, canonical ASCII payload as input.
"""
from __future__ import annotations

import argparse
import json
from typing import Sequence

ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ "
_CHAR_INDEX = {char: number for number, char in enumerate(ALPHABET)}


def encode(text: str) -> str:
    """Encode canonical ASCII payload to fixed-width base-3 symbols.

    Invalid Unicode, punctuation and lowercase text raise ValueError instead of
    silently losing data or transforming Vietnamese orthography.
    """
    output: list[str] = []
    for position, char in enumerate(text):
        if char not in _CHAR_INDEX:
            raise ValueError(f"invalid symbol at index {position}: {char!r}; only A-Z and SPACE")
        index = _CHAR_INDEX[char]
        output.append(f"{index // 9}{(index // 3) % 3}{index % 3}")
    return "".join(output)


def decode(trits: str) -> str:
    """Decode fixed-width base-3 string to canonical ASCII payload."""
    if len(trits) % 3:
        raise ValueError("trit count must be a multiple of 3")
    if any(char not in "012" for char in trits):
        raise ValueError("trit stream may only contain 0, 1 and 2")
    return "".join(
        ALPHABET[int(trits[i]) * 9 + int(trits[i + 1]) * 3 + int(trits[i + 2])]
        for i in range(0, len(trits), 3)
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=["encode", "decode", "check"])
    parser.add_argument("value", help="A-Z + SPACE for encode/check; trit stream for decode")
    parser.add_argument("--json", action="store_true", help="write a JSON object")
    args = parser.parse_args(argv)
    try:
        if args.operation == "encode":
            result = encode(args.value)
        elif args.operation == "decode":
            result = decode(args.value)
        else:
            result = "PASS" if decode(encode(args.value)) == args.value else "FAIL"
        if args.json:
            print(json.dumps({"operation": args.operation, "input": args.value, "output": result},
                             ensure_ascii=False))
        else:
            print(result)
        return 0 if result != "FAIL" else 1
    except ValueError as exc:
        parser.exit(2, f"ERROR: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
