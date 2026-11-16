import re
from pathlib import Path

def check(report, volume=None):
    (void_vol := volume)
    text = Path(report).read_text()
    m = re.search(r"LARA1 status/mount/frames/sw/fault/glerr/nonblack/px ([0-9a-f ]+)", text)
    if not m:
        raise AssertionError("missing LARA1 summary line")
    vals = [int(x, 16) for x in m.group(1).split()]
    status, mount, frames, sw, fault, glerr, nonblack, px = vals
    ok = (
        status == 0
        and mount == 0
        and frames > 0
        and fault == 0
        and nonblack > 0
        and "LARA PASS" in text
    )
    return {
        "pass": ok,
        "status": status,
        "mount": mount,
        "frames": frames,
        "software": sw,
        "fault": fault,
        "glerr": glerr,
        "nonblack": nonblack,
        "px": hex(px),
    }
