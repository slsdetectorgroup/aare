"""Does any text on a slide physically overlap any other text?

The overflow checker only sees text crossing the footer line. It cannot see a
bullet running into the code panel underneath it, which is exactly the failure a
type-size change produces. This reads the RENDERED pdf and reports every pair of
text lines whose bounding boxes actually intersect.

    python docs/deck/audit_layout.py docs/cf_cuda_performance.pdf

No column heuristics: two lines are a collision only if their boxes overlap in
BOTH axes, so side-by-side columns and consecutive baselines never register.
"""
import re
import subprocess
import sys
from collections import defaultdict

PDF = sys.argv[1] if len(sys.argv) > 1 else "docs/cf_cuda_performance.pdf"
FOOTER = 525.5          # the progress track; nothing may sit below it
LINE_TOL = 2.5          # words whose baselines are this close share a line
X_FRAC = 0.10           # overlap must cover this much of the narrower line
                        # (0.25 let a bullet run a whole word under a code
                        #  panel title without registering)
Y_MIN = 2.0             # ... and this many points vertically
GUTTER = 24.0           # white space this wide separates two columns

ATTR = re.compile(r'(\w+)="([\d.eE+-]+)"')

xml = subprocess.run(["pdftotext", "-bbox", PDF, "-"],
                     capture_output=True, text=True).stdout

pages, cur = [], None
for raw in xml.split("\n"):
    if "<page " in raw:
        cur = []
        pages.append(cur)
    elif "<word " in raw and cur is not None:
        # A hand-rolled split on quotes silently yields an empty dict here; the
        # audit then reports a clean deck because it parsed nothing at all.
        g = dict(ATTR.findall(raw.split("<word ")[1].split(">")[0]))
        if {"xMin", "yMin", "xMax", "yMax"} <= g.keys():
            cur.append((float(g["xMin"]), float(g["yMin"]),
                        float(g["xMax"]), float(g["yMax"]),
                        raw.split(">", 1)[1].split("</word>")[0]))


def lines_of(words):
    """Group words into lines, and SPLIT a line at a column gutter.

    Grouping by baseline alone merges a rail row and a body bullet that happen
    to sit at the same height into one full-width "line". Two such lines then
    overlap in x by construction, and the audit reports a collision on every
    two-column slide in the deck -- which is most of them. A run of words
    separated by more than a third of an inch of white space is a different
    column, not the same line.
    """
    rows = defaultdict(list)
    for w in words:
        rows[round(w[3] / LINE_TOL)].append(w)
    out = []
    for k in sorted(rows):
        run = []
        for w in sorted(rows[k]):
            if run and w[0] - run[-1][2] > GUTTER:
                out.append(_span(run))
                run = []
            run.append(w)
        if run:
            out.append(_span(run))
    return out


def _span(ws):
    return (min(w[0] for w in ws), min(w[1] for w in ws),
            max(w[2] for w in ws), max(w[3] for w in ws),
            " ".join(w[4] for w in ws))


# Page 1 is the PSI template's own title slide. Its layout is not ours to fix --
# the deck imports it whole -- and it puts the date below our footer line.
TEMPLATE_PAGE = 1

overlaps, footer = [], []
for pno, words in enumerate(pages, 1):
    ls = lines_of(words)
    for i, a in enumerate(ls):
        if a[3] > FOOTER and pno != TEMPLATE_PAGE:
            footer.append((pno, a[3], a[4]))
        for b in ls[i + 1:]:
            # A one-character "line" beside a real one is a superscript marker
            # sharing the baseline it annotates. That is what a footnote mark IS.
            if len(a[4]) == 1 or len(b[4]) == 1:
                continue
            ox = min(a[2], b[2]) - max(a[0], b[0])
            oy = min(a[3], b[3]) - max(a[1], b[1])
            if oy < Y_MIN or ox <= 0:
                continue
            if ox >= X_FRAC * min(a[2] - a[0], b[2] - b[0]):
                overlaps.append((pno, ox, oy, a[4][:44], b[4][:44]))

nw = sum(len(p_) for p_ in pages)
assert nw > 500, f"parsed only {nw} words -- the bbox parser is broken, not the deck"
print(f"pages: {len(pages)}   words parsed: {nw}\n")
print(f"COLLIDING text: {len(overlaps)}")
seen = set()
for pno, ox, oy, a, b in overlaps:
    if pno in seen and len([o for o in overlaps if o[0] == pno]) > 3:
        continue
    seen.add(pno)
    print(f"  p{pno:>3}  x{ox:5.1f} y{oy:5.1f}   {a!r}\n              {b!r}")
n_pages = len({o[0] for o in overlaps})
print(f"  ({n_pages} pages affected)")

print(f"\nBELOW THE FOOTER: {len(footer)}")
for pno, y, txt in footer:
    print(f"  p{pno:>3}  y {y:6.1f}   {txt[:70]!r}")

print("\nclean" if not (overlaps or footer) else "\nFIX THE ABOVE")
sys.exit(1 if (overlaps or footer) else 0)
