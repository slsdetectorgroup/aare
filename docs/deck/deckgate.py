"""The projection floor, and the one thing both figure generators must share.

A figure's on-screen type size is `raw_pt x (placement_width / figure_width)`,
and neither factor is visible where the font size is written. This closes that
loop by reading the placement width out of the deck script itself, so the gate
cannot drift from the layout it checks.

It lives in its own module because there are TWO generators -- make_figs.py and
make_figs_kernel.py -- and for a long time only the first was checked. The
kernel figures were consequently set at 7-8 pt and nobody found out until they
were projected.
"""
import re
from pathlib import Path

import matplotlib

# The floor, in points, on the projected slide. Set for a room where the back
# row is 6-7 m away: on a 13.33 x 7.5 in slide, 10 pt is two thirds of the 15 pt
# body size, which is the usual lower bound for supporting type. Below that a
# label inside a plot reads as a footnote rather than part of the argument.
MIN_EFF_PT = 10.0

# A string may opt out by carrying gid="texture". That is for marks nobody is
# asked to READ -- a value printed into every cell of a pixel map, where the
# pattern is the message and the digits are shading. It is set per Text object,
# never per figure, so an exemption stays a decision about one label.
EXEMPT_GID = "texture"

DECK = Path(__file__).resolve().parent / "build_performance_deck.py"
VIOLATIONS = []


def placements():
    """{figure name: the narrowest width the deck places it at}.

    The token values must track build_performance_deck.py. They did not when the
    margins were narrowed, and the gate then silently checked widths no figure
    was ever placed at.
    """
    if not DECK.exists():
        return {}
    env = {"M": 0.50, "COL": 8.16, "RAIL_W": 3.62, "RAIL_X": 9.28,
           "W": 13.333, "H": 7.5}
    out = {}
    for m in re.finditer(r'\b(?:card_)?figure\(s,\s*"([a-z0-9_]+)",([^)]*)\)',
                         DECK.read_text()):
        args = [a.strip() for a in m.group(2).split(",")]
        if len(args) < 3:
            continue
        try:
            w = float(eval(args[2], {}, env))
        except Exception:
            continue
        out[m.group(1)] = min(out.get(m.group(1), 99.0), w)
    return out


PLACE_W = placements()


def check(fig, path, name, dpi, place_w=None):
    """Report every string in `fig` that will project below the floor.

    Call it BEFORE closing the figure and AFTER saving it: the saved width is
    what sets the scale, and `bbox_inches="tight"` means that width is not the
    figsize.
    """
    from PIL import Image

    texts = [(t.get_text(), t.get_fontsize())
             for t in fig.findobj(matplotlib.text.Text)
             if t.get_text().strip() and t.get_visible()
             and t.get_gid() != EXEMPT_GID]
    if place_w is None:
        place_w = PLACE_W.get(name)
    if place_w is None:
        print("wrote", name)
        return
    pw = Image.open(path).size[0] / dpi
    scale = place_w / pw
    bad = sorted({(round(sz * scale, 2), round(sz, 1), txt[:44].replace("\n", " "))
                  for txt, sz in texts if sz * scale < MIN_EFF_PT - 0.05})
    print(f"wrote {name:22s} {pw:5.2f} in -> {place_w:5.2f} in  "
          f"(x{scale:.3f})  min eff "
          f"{min((sz * scale for _, sz in texts), default=99):.1f} pt")
    for eff, raw, txt in bad:
        VIOLATIONS.append((name, eff, raw, txt))
        print(f"    ILLEGIBLE  {eff:5.2f} pt (set {raw:4.1f})  {txt!r}")


def report():
    if not VIOLATIONS:
        print(f"\nlegibility: every string in every figure renders at "
              f">= {MIN_EFF_PT} pt on the slide.")
        return
    print(f"\nlegibility: {len(VIOLATIONS)} strings below {MIN_EFF_PT} pt "
          f"in {len({v[0] for v in VIOLATIONS})} figures")
    for name, eff, raw, txt in sorted(VIOLATIONS):
        print(f"  {name:22s} {eff:5.2f} pt   {txt!r}")
