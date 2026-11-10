#!/usr/bin/env python3
"""Theoretical *weight-only* memory estimates. NOT a runtime or speed benchmark."""
from __future__ import annotations

import argparse
import json
import math


def calculate(parameter_count: int) -> dict[str, float]:
    if not isinstance(parameter_count, int) or parameter_count <= 0:
        raise ValueError("parameter_count must be a positive integer")
    return {
        "fp32_MB": 4 * parameter_count / 1_000_000,
        "int8_MB": parameter_count / 1_000_000,
        "ternary_2bit_MB": 2 * parameter_count / 8_000_000,
        "ternary_entropy_lower_bound_MB": math.log2(3) * parameter_count / 8_000_000,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parameters", type=int, default=48_000_000)
    args = parser.parse_args()
    output = {"parameters": args.parameters, "weights_only": True, **calculate(args.parameters)}
    output["disclaimer"] = ("Ternary ideal is an information-theoretic lower bound; "
                            "activation, operator overhead, model graphs, vocoder, "
                            "workspaces and alignment are excluded.")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
