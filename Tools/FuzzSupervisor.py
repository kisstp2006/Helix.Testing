"""Runs a fuzz executable of Helix.Testing for a long time: after a crash or a hang it finds the exact round, keeps the
report, and goes on from the next round until the time is up.

    python -I FuzzSupervisor.py --exe <fuzz.exe> --out <folder> --hours 24 [--first N] [--path <dir>]...

Writes into the folder: run.log (the runner's output), faults/round_<n>.txt (each fault's report and the command that
replays it) and summary.json (rounds, seconds, targets, faults), updated after every run.
--path adds folders to PATH for the executable (the AddressSanitizer runtime of a windows-fuzz build).
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time


def run(exe, args, env, timeout=None):
    """Runs the executable; returns (exit code, stdout, stderr)."""
    try:
        p = subprocess.run([exe] + args, env=env, capture_output=True, text=True, errors="replace", timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired as e:
        return -999, e.stdout or "", e.stderr or ""


def fails(exe, env, first, rounds):
    code, _, _ = run(exe, ["--first=%d" % first, "--rounds=%d" % rounds], env, timeout=600)
    return code != 0


def locate(exe, env, start, guess):
    """The first failing round at or after `start` (the progress file can be up to a second old)."""
    span = max(guess, 16)
    while not fails(exe, env, start, span):
        span *= 4
        if span > 1 << 24:
            return None
    lo, hi = start, start + span
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if fails(exe, env, lo, mid - lo):
            hi = mid
        else:
            lo = mid
    return lo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--hours", type=float, default=24)
    parser.add_argument("--first", type=int, default=0)
    parser.add_argument("--path", action="append", default=[])
    args = parser.parse_args()
    exe = os.path.abspath(args.exe)
    os.makedirs(os.path.join(args.out, "faults"), exist_ok=True)
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(args.path + [env.get("PATH", "")])
    progress = os.path.abspath(os.path.join(args.out, "progress.txt"))
    summary_path = os.path.join(args.out, "summary.json")
    summary = {"exe": exe, "first": args.first, "hours": args.hours, "started": time.strftime("%Y-%m-%d %H:%M:%S"), "seconds": 0.0, "rounds": 0,
               "targets": None, "faults": [], "runs": 0}
    goal = args.hours * 3600
    start = time.time()
    first = args.first
    log = open(os.path.join(args.out, "run.log"), "a")
    while True:
        left = goal - (time.time() - start)
        if left <= 1:
            break
        began = time.time()
        code, out, err = run(exe, ["--seconds=%d" % int(left), "--first=%d" % first, "--progress=%s" % progress], env)
        log.write(out)
        log.flush()
        summary["runs"] += 1
        done = re.search(r"done: (\d+) rounds of (\d+) target", out)
        if done:
            summary["rounds"] += int(done.group(1))
            summary["targets"] = int(done.group(2))
            first += int(done.group(1))
        if code != 0:
            # The round of the fault: from the runner's own report, or found from the progress file.
            m = re.search(r"fuzz round (\d+) of (\S+)", err)
            if m:
                round_ = int(m.group(1))
            else:
                last = open(progress).read().split() if os.path.exists(progress) else [str(first)]
                rate = max(1, int(summary["rounds"] / max(1.0, summary["seconds"] + time.time() - began)))
                round_ = locate(exe, env, int(last[0]), rate * 3)
            if round_ is None:
                round_ = first
            ran = round_ - first
            summary["rounds"] += max(0, ran)
            report = os.path.join(args.out, "faults", "round_%d.txt" % round_)
            _, _, again = run(exe, ["--first=%d" % round_, "--rounds=1"], env, timeout=600)
            with open(report, "w") as f:
                f.write("exit code %d\nagain: %s --first=%d --rounds=1\n\n" % (code, exe, round_))
                f.write(again or err)
            summary["faults"].append({"round": round_, "exit": code, "report": os.path.relpath(report, args.out),
                                      "at_seconds": round(time.time() - start)})
            first = round_ + 1
        summary["seconds"] = round(time.time() - start, 1)
        summary["next_round"] = first
        summary["clean"] = not summary["faults"]
        with open(summary_path, "w") as f:
            json.dump(summary, f, indent=1)
        if code == 0 and done:
            break
    summary["finished"] = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(summary_path, "w") as f:
        json.dump(summary, f, indent=1)
    print("%d rounds in %.0f s, %d faults" % (summary["rounds"], summary["seconds"], len(summary["faults"])))


if __name__ == "__main__":
    sys.exit(main())
