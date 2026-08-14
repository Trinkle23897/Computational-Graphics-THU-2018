#!/usr/bin/env python3
"""Compare independent-seed noise convergence of the CUDA rendering algorithms."""
import argparse
import csv
import json
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, sobel


REGIONS = {
    "vase": {"white": (162, 184, 378, 398), "flower": (154, 192, 404, 444),
             "vase": (125, 252, 375, 469), "rainbow": (193, 249, 166, 271),
             "floor": (305, 351, 23, 156)},
    "balls": {"background": (20, 130, 10, 195), "floor": (248, 332, 218, 365),
              "reflection": (273, 345, 372, 478), "glass": (40, 158, 397, 590),
              "stripe": (125, 191, 260, 370)},
}


def run_job(job, options, directory):
    scene, algorithm, iterations, seed, gpu = job
    cached = algorithm in ("cache", "cache_hq", "cache_ultra")
    label = f"{scene}-{algorithm}-i{iterations:03d}-s{seed}"
    image = directory / f"{label}.ppm"
    command = [str(options.binary), str(options.width), str(options.height), str(image)]
    if algorithm == "pt":
        command += [str(iterations * 8)]
    else:
        photons = "5" if scene == "vase" else "8"
        if cached and options.width >= 1920:
            photons = "16" if scene == "vase" else "10" if iterations <= 8 else "8"
        command += [str(iterations), photons, ".5", "1"]
    command += ["--scene", scene, "--nearest", "--seed", str(seed)]
    if algorithm in ("hybrid", "reuse", "guided") or cached:
        samples = iterations * 16
        if cached and options.width >= 1920:
            samples = ({"cache": 192, "cache_hq": 576, "cache_ultra": 1536}[algorithm]
                       if scene == "balls" else 64)
        command += ["--hybrid-samples", str(samples), "--reconstruction-radius",
                    "4" if scene == "vase" or cached and options.width >= 1920 else "8"]
    if algorithm == "reuse":
        command.append("--reuse")
    if algorithm == "guided":
        command.append("--guided")
    if cached:
        command.append("--cache")
        if scene == "vase" and options.width >= 1920:
            command += ["--shadow-samples", "3"]
    result = subprocess.run(command, cwd=options.binary.parent,
                            env=dict(os.environ, CUDA_VISIBLE_DEVICES=str(gpu)),
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (directory / f"{label}.log").write_text(result.stderr)
    match = re.search(r"render: ([\d.]+) ms", result.stderr)
    if result.returncode or not match:
        raise RuntimeError(f"{label}: {result.stderr[-1200:]}")
    record = {"scene": scene, "algorithm": algorithm, "iterations": iterations,
              "seed": seed, "milliseconds": float(match.group(1)), "image": str(image)}
    print(json.dumps(record), flush=True)
    return record


def summarize(records, width, height, references=None):
    weights = np.array([.2126, .7152, .0722], dtype=np.float32)
    summaries = []
    for scene, algorithm, iterations in sorted(
            {(item["scene"], item["algorithm"], item["iterations"]) for item in records}):
        group = [item for item in records if
                 (item["scene"], item["algorithm"], item["iterations"]) ==
                 (scene, algorithm, iterations)]
        images = np.stack([np.asarray(Image.open(item["image"]), dtype=np.float32) @ weights
                           for item in group])
        summary = {"scene": scene, "algorithm": algorithm, "iterations": iterations,
                   "seeds": len(group),
                   "milliseconds": round(np.mean([item["milliseconds"] for item in group]), 3)}
        bias = None
        if references and scene in references:
            target = Image.open(references[scene]).convert("RGB")
            if target.size != (width, height):
                target = target.resize((width, height), Image.Resampling.LANCZOS)
            linear = sum((np.asarray(Image.open(item["image"]), dtype=np.float32) / 255) ** 2.2
                         for item in group) / len(group)
            average = np.maximum(linear, 0) ** (1 / 2.2) * 255
            difference = average - np.asarray(target, dtype=np.float32)
            bias = gaussian_filter(difference, (max(1., width / 640),) * 2 + (0,))
            summary["reference_mae"] = round(float(np.mean(np.abs(difference))), 3)
            summary["reference_rmse"] = round(float(np.sqrt(np.mean(difference ** 2))), 3)
            summary["reference_bias"] = round(float(np.sqrt(np.mean(bias ** 2))), 3)
            summary["reference_p95"] = round(float(np.quantile(
                np.sqrt(np.mean(bias ** 2, axis=2)), .95)), 3)
        noises = []
        for region, (top, bottom, left, right) in REGIONS[scene].items():
            rows = slice(int(top * height / 360), int(bottom * height / 360))
            columns = slice(int(left * width / 640), int(right * width / 640))
            crop = images[:, rows, columns]
            average = crop.mean(axis=0)
            smooth = gaussian_filter(average, 2.5)
            gradient = np.hypot(sobel(smooth, axis=0), sobel(smooth, axis=1))
            quiet = gradient < np.quantile(gradient, .55)
            noise = float(np.sqrt(np.mean(np.var(crop, axis=0, ddof=1)[quiet])))
            edges = gradient > np.quantile(gradient, .90)
            sharp = np.hypot(sobel(gaussian_filter(average, .65), axis=0),
                             sobel(gaussian_filter(average, .65), axis=1))[edges].mean()
            summary[f"{region}_noise"] = round(noise, 3)
            summary[f"{region}_mean"] = round(float(average.mean()), 2)
            summary[f"{region}_edge"] = round(float(sharp), 2)
            if bias is not None:
                summary[f"{region}_bias"] = round(float(np.sqrt(np.mean(
                    bias[rows, columns] ** 2))), 3)
            noises.append(noise)
        if bias is not None and scene == "balls":
            caustic = bias[int(230 * height / 360):int(290 * height / 360),
                           int(105 * width / 640):int(245 * width / 640)]
            summary["caustic_bias"] = round(float(np.sqrt(np.mean(caustic ** 2))), 3)
        summary["noise"] = round(float(np.sqrt(np.mean(np.square(noises)))), 3)
        summary["variance_seconds"] = round(summary["noise"] ** 2 *
                                             summary["milliseconds"] / 1000, 3)
        if bias is not None:
            summary["total_error"] = round(float(np.hypot(summary["noise"],
                                                           summary["reference_bias"])), 3)
        summaries.append(summary)
        print("summary", json.dumps(summary), flush=True)
    return summaries


def plot(summaries, destination):
    import matplotlib.pyplot as plt

    colors = {"pt": "#d15f4a", "sppm": "#c79a35", "hybrid": "#657bc2", "reuse": "#2b9e77",
              "guided": "#9146ad", "cache_blur": "#c78145", "cache_legacy": "#c25443",
              "cache_previous": "#899aab", "cache_previous_hq": "#58697c",
              "cache": "#138c9e", "cache_hq": "#075c69", "cache_ultra": "#083d47"}
    names = {"pt": "Path tracing", "sppm": "SPPM", "hybrid": "Hybrid SPPM",
             "reuse": "Stratified reuse", "guided": "Guided decomposition",
             "cache_blur": "Previous over-smoothed cache", "cache_legacy": "Biased cache",
             "cache_previous": "Previous half-resolution correction",
             "cache_previous_hq": "Previous half-resolution correction (HQ)",
             "cache": "Path-adaptive correction", "cache_hq": "Path-adaptive correction (HQ)",
             "cache_ultra": "Path-adaptive correction (ultra)"}
    figure, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    for row, scene in enumerate(("vase", "balls")):
        metric = "total_error" if any(item["scene"] == scene and "total_error" in item
                                       for item in summaries) else "noise"
        for algorithm in colors:
            values = sorted((item for item in summaries if item["scene"] == scene and
                             item["algorithm"] == algorithm and metric in item),
                            key=lambda item: item["iterations"])
            if not values:
                continue
            for column, key in enumerate(("iterations", "milliseconds")):
                axis = axes[row, column]
                x = [item[key] / (1000 if key == "milliseconds" else 1) for item in values]
                axis.plot(x, [item[metric] for item in values], "o-", color=colors[algorithm],
                          label=names[algorithm], linewidth=2, markersize=4)
                axis.set(xscale="log", yscale="log", xlabel="Iterations" if column == 0
                         else "GPU kernel time (seconds)", ylabel="Noise + physical bias RMS"
                         if metric == "total_error" else "Independent-seed noise RMS")
                axis.set_title(scene.capitalize())
                axis.grid(alpha=.2, which="both")
    axes[1, 1].legend(frameon=False, fontsize=8)
    figure.savefig(destination, dpi=180, facecolor="white")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=Path("./render-cuda"))
    parser.add_argument("--output", type=Path, default=Path("/tmp/sppm-convergence"))
    parser.add_argument("--algorithms", default="pt,sppm,hybrid,reuse,guided,cache")
    parser.add_argument("--iterations", default="4,8,16,32,64")
    parser.add_argument("--seeds", default="0,1,2,3")
    parser.add_argument("--gpus", default="0")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--reference", action="append", default=[], metavar="SCENE=IMAGE",
                        help="measure systematic bias against a converged reference image")
    options = parser.parse_args()
    options.binary = options.binary.resolve()
    options.output = options.output.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    seeds = [int(value) for value in options.seeds.split(",")]
    if len(seeds) < 2:
        parser.error("at least two independent seeds are required")
    references = {}
    for value in options.reference:
        scene, separator, filename = value.partition("=")
        if not separator or scene not in REGIONS or not Path(filename).is_file():
            parser.error(f"invalid reference {value!r}; expected SCENE=existing-image")
        references[scene] = Path(filename)
    gpus = options.gpus.split(",")
    raw = [(scene, algorithm, iteration, seed)
           for scene in REGIONS for algorithm in options.algorithms.split(",")
           for iteration in map(int, options.iterations.split(",")) for seed in seeds]
    groups = [[(*job, gpu) for job in raw[index::len(gpus)]]
              for index, gpu in enumerate(gpus)]
    with ThreadPoolExecutor(max_workers=len(gpus)) as executor:
        nested = executor.map(lambda jobs: [run_job(job, options, options.output) for job in jobs], groups)
        records = [record for group in nested for record in group]
    (options.output / "renders.json").write_text(json.dumps(records, indent=2))
    summaries = summarize(records, options.width, options.height, references)
    (options.output / "convergence.json").write_text(json.dumps(summaries, indent=2))
    fields = list(dict.fromkeys(key for item in summaries for key in item))
    with (options.output / "convergence.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(summaries)
    plot(summaries, options.output / "convergence.png")
    print(f"wrote {len(records)} renders to {options.output}", flush=True)


if __name__ == "__main__":
    main()
