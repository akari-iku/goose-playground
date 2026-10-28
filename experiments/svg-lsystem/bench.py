"""Run the three lsystem binaries on the same inputs and report wall time and
peak working set. Windows only: the peak is read through GetProcessMemoryInfo
while the process handle is still open, which works after the process exits.

usage: python bench.py [preset depth] [runs]
"""

import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).parent
BINARIES = {
    "goose": HERE / "goose" / "lsystem.exe",
    "rust": HERE / "rust" / "target" / "release" / "lsystem.exe",
    "cpp": HERE / "cpp" / "build" / "lsystem.exe",
}


class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
    _fields_ = [
        ("cb", wt.DWORD),
        ("PageFaultCount", wt.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
    ]


def run_once(exe: Path, args: list[str]) -> tuple[float, int]:
    t0 = time.perf_counter()
    p = subprocess.Popen([str(exe), *args], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    p.wait()
    elapsed = time.perf_counter() - t0
    counters = PROCESS_MEMORY_COUNTERS()
    counters.cb = ctypes.sizeof(counters)
    ok = ctypes.windll.psapi.GetProcessMemoryInfo(int(p._handle), ctypes.byref(counters), counters.cb)
    if not ok:
        raise OSError(ctypes.get_last_error())
    return elapsed, counters.PeakWorkingSetSize


def main() -> None:
    argv = sys.argv[1:]
    preset, depth = (argv[0], argv[1]) if len(argv) >= 2 else ("plant", "8")
    runs = int(argv[2]) if len(argv) >= 3 else 10
    out = HERE / "out"
    out.mkdir(exist_ok=True)
    print(f"{preset} depth {depth}, {runs} runs each, best time / median time / peak working set")
    for name, exe in BINARIES.items():
        if not exe.exists():
            print(f"{name:6s} (not built: {exe.relative_to(HERE)})")
            continue
        times, peaks = [], []
        try:
            for _ in range(runs):
                t, peak = run_once(exe, [preset, depth, str(out / f"{preset}.{name}.svg")])
                times.append(t)
                peaks.append(peak)
        except OSError as e:
            print(f"{name:6s} (could not run: {e})")
            continue
        times.sort()
        print(f"{name:6s} {times[0] * 1000:7.1f} ms  {times[len(times) // 2] * 1000:7.1f} ms  {max(peaks) / 1024:9.0f} KB")


if __name__ == "__main__":
    main()
