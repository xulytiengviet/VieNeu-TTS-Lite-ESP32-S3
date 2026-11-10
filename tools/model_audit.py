#!/usr/bin/env python3
"""Inspect actual VieNeu v3 Nano ONNX graph files WITHOUT loading models."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

REQUIRED = ("text_encoder.onnx", "duration_predictor.onnx",
            "vector_estimator.onnx", "codec_decoder.onnx",
            "config.json", "constants.npz")


def audit(model_dir: Path) -> dict:
    files = {}
    missing = []
    for name in REQUIRED:
        path = model_dir / name
        if path.is_file():
            files[name] = {"bytes": path.stat().st_size,
                           "MB": round(path.stat().st_size / 1_000_000, 3)}
        else:
            missing.append(name)
    graph_bytes = sum(data["bytes"] for key, data in files.items() if key.endswith(".onnx"))
    return {
        "model_dir": str(model_dir),
        "required_missing": missing,
        "graph_files": files,
        "onnx_graph_MB": round(graph_bytes / 1_000_000, 3),
        "note": "Graph sizes are disk assets, not peak RAM and not an ESP32-S3 benchmark.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model-dir", type=Path, default=Path("models/VieNeu-TTS-v3-Nano"))
    args = parser.parse_args()
    report = audit(args.model_dir)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 1 if report["required_missing"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
