import re
from pathlib import Path

def check(report, volume=None):
    (void_vol := volume)
    text = Path(report).read_text()
    m = re.search(r"LARA1 status/mount/gui/amb/frames/sw/fault/glerr/px ([0-9a-f ]+)", text)
    if not m:
        raise AssertionError("missing LARA1 summary line")
    vals = [int(x, 16) for x in m.group(1).split()]
    status, mount, gui_ok, amb_ok, frames, sw, fault, glerr, px = vals
    ok = (
        status == 0
        and mount == 0
        and gui_ok == 1
        and amb_ok == 1
        and frames == 30
        and sw == 0
        and fault == 0
        and glerr == 0
        and (px & 0xffffff) != 0
        and "LARA PASS" in text
    )
    return {
        "pass": ok,
        "status": status,
        "mount": mount,
        "gui_ok": gui_ok,
        "amb_ok": amb_ok,
        "frames": frames,
        "software": sw,
        "fault": fault,
        "glerr": glerr,
        "px": hex(px),
    }
