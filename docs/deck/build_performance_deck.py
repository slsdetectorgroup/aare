"""Build docs/cf_cuda_performance.pptx — the algorithm + kernel + hardware half of
docs/cf_cuda_kernel.pptx fused with the opt1→opt7 optimization story.

The ladder is told in three acts, ordered by which bar is tallest:

    ACT I   [f64]  feed the GPU        opt1 opt2 opt3 opt4   (+ route A, rejected)
    ACT II  [f64]  get results back    opt5 opt6             (+ routes B', B'', rejected)
    ACT III [f32]  the kernel          opt7

Act III comes last because it cannot be justified earlier: measured through
collect() the f32 kernel is worth 1.5 % end-to-end, and only once the host is off
the critical path is the same change worth 21 %.

cf_cuda_kernel.pptx is kept solely as the base presentation — it donates the PSI
theme and title slide, and every other slide of it is deleted below.

All hardware numbers are re-measured against the CURRENT kernel, not taken
from the kernel deck (whose implementation details and timings are stale):

    nvcc -arch=sm_89 --ptxas-options=-v          -> registers, spills
    cudaOccupancyMaxActiveBlocksPerMultiprocessor -> blocks/SM, occupancy

    3x3 : 34 regs/thread, 0 spill, 1296 B smem, 6 blocks/SM, 100.0% occupancy
    9x9 : 128 regs/thread, 0 spill, 2304 B smem, 2 blocks/SM,  33.3% occupancy
    32x32 blocks @ 9x9: 0 blocks/SM -- 1024 x 128 regs > 65536 regs/SM

Performance numbers come from docs/ClusterFinderCUDA_benchmark_results.md (quotable
rows only).
"""
from pptx import Presentation
from pptx.util import Inches as In, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.enum.shapes import MSO_SHAPE
from lxml import etree
from pathlib import Path
from PIL import Image

DOCS = Path(__file__).resolve().parent.parent
FIGS = DOCS / "figures"
BASE = DOCS / "cf_cuda_kernel.pptx"          # PSI theme + title slide
OUT = DOCS / "cf_cuda_performance.pptx"

# ---------------------------------------------------------------- design tokens
BG     = RGBColor(0x0B, 0x10, 0x18)
PANEL  = RGBColor(0x12, 0x1A, 0x28)
# Code panels are framed, not filled, to set them apart. The fill can only ever
# be a hair lighter than the slide (1.16:1 at #17202E) before the accent-coloured
# tokens inside start losing contrast against it -- and a projector's black level
# crushes every dark tone together anyway. The 1 pt edge reaches 2.18:1 against
# the background without touching the ink's ground, so the boundary is drawn by
# the line and the fill only has to say "a different surface".
CODEBG = RGBColor(0x17, 0x20, 0x2E)
CODEEDGE = RGBColor(0x3A, 0x4C, 0x66)
# MUTED was tuned against the slide background; on the lighter code fill it drops
# to 3.75:1, and it carries the comments and the panel title. This puts them back
# above MUTED's original 4.22 -- inside code panels only.
CODEDIM = RGBColor(0x7C, 0x8A, 0x9E)
RULE   = RGBColor(0x1E, 0x28, 0x36)
ACCENT = RGBColor(0x1E, 0x90, 0xC2)
AMBER  = RGBColor(0xE8, 0xB2, 0x5C)
PALE   = RGBColor(0xE7, 0xED, 0xF4)
TEXT2  = RGBColor(0xA5, 0xB2, 0xC4)
MUTED  = RGBColor(0x6B, 0x7A, 0x90)
CARD   = RGBColor(0xF4, 0xF6, 0xF9)          # light card for white figures
RED    = RGBColor(0xE2, 0x54, 0x54)          # pointing only, never a data colour

UI, MONO = "Segoe UI", "Consolas"
W, H = 13.333, 7.5
M = 0.7                       # left margin
COL = 7.9                     # left column width
RAIL_X, RAIL_W = 9.2, 3.5     # right rail

A = "{http://schemas.openxmlformats.org/drawingml/2006/main}"
P = "{http://schemas.openxmlformats.org/presentationml/2006/main}"
R = "{http://schemas.openxmlformats.org/officeDocument/2006/relationships}"

prs = Presentation(str(BASE))
prs.slide_width, prs.slide_height = In(W), In(H)
BLANK = prs.slide_layouts[0]                  # 'Blank Slide' — zero shapes
N_SLIDES = 35


# ------------------------------------------------------------------- helpers
def keep_only_slide(prs, keep=0):
    """Drop every slide but one from the base presentation."""
    lst = prs.slides._sldIdLst
    for i, sldId in enumerate(list(lst)):
        if i != keep:
            prs.part.drop_rel(sldId.get(f"{R}id"))
            lst.remove(sldId)


def set_para_texts(shape, texts):
    """Replace paragraph texts in-place, keeping each paragraph's formatting."""
    for p, txt in zip(shape.text_frame.paragraphs, texts):
        if not p.runs:
            continue
        p.runs[0].text = txt
        for r in p.runs[1:]:
            r.text = ""


def new_slide():
    """A dark slide on the PSI master.

    Two independent guards, because the base template's master carries a PSI
    background picture and logo that must not bleed through:
      1. showMasterSp="0" + a slide-level <p:bg> (correct schema position:
         first child of <p:cSld>), which is what PowerPoint honours;
      2. a full-bleed rectangle as the first shape, which every renderer
         honours regardless of how it treats (1).
    """
    s = prs.slides.add_slide(BLANK)
    s._element.set("showMasterSp", "0")

    cSld = s._element.find(f"{P}cSld")
    bg = etree.Element(f"{P}bg")
    pr = etree.SubElement(bg, f"{P}bgPr")
    fill = etree.SubElement(pr, f"{A}solidFill")
    clr = etree.SubElement(fill, f"{A}srgbClr")
    clr.set("val", "0B1018")
    etree.SubElement(pr, f"{A}effectLst")
    cSld.insert(0, bg)

    rect(s, 0, 0, W, H, BG)
    return s


def rect(s, x, y, w, h, color, shape=MSO_SHAPE.RECTANGLE):
    sh = s.shapes.add_shape(shape, In(x), In(y), In(w), In(h))
    sh.fill.solid(); sh.fill.fore_color.rgb = color
    sh.line.fill.background(); sh.shadow.inherit = False
    return sh


def tb(s, x, y, w, h, anchor=MSO_ANCHOR.TOP):
    box = s.shapes.add_textbox(In(x), In(y), In(w), In(h))
    tf = box.text_frame
    tf.word_wrap = True
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = anchor
    return tf


def para(tf, first=False, space_after=0, space_before=0, line=None, align=None):
    p = tf.paragraphs[0] if first else tf.add_paragraph()
    p.space_after = Pt(space_after); p.space_before = Pt(space_before)
    if line: p.line_spacing = line
    if align: p.alignment = align
    return p


# The projection floor, in points, for text set directly in PowerPoint.
#
# The deck is shown in a room where the back row is 6-7 m from the screen. On a
# 13.33 x 7.5 in slide, 9 pt is about 1/60 of the slide height, which is the
# conventional lower bound for supporting detail. It is deliberately the SAME
# number that make_figs.py checks every string inside every PNG against, so the
# deck has one floor rather than two, and nothing is legible on the slide but
# not inside the picture next to it.
#
# It is enforced here, in run(), rather than at the ~400 call sites, because a
# floor applied by hand is a floor that one new caption silently drops through.
MIN_PT = 9.0


def run(p, text, size=11, color=TEXT2, font=UI, bold=False, italic=False, spc=None):
    r = p.add_run(); r.text = text
    f = r.font
    f.name, f.size, f.bold, f.italic = font, Pt(max(size, MIN_PT)), bold, italic
    f.color.rgb = color
    if spc is not None:
        r.font._rPr.set("spc", str(int(spc * 100)))
    return r


# ------------------------------------------------------------------ chrome
def chrome(s, idx, eyebrow, title, title_size=27):
    rect(s, M, 0.60, 0.35, 0.035, ACCENT)
    tf = tb(s, 1.17, 0.50, 10.33, 0.32)
    run(para(tf, True), eyebrow.upper(), 9, MUTED, bold=True, spc=1.6)

    tf = tb(s, M, 0.86, 11.9, 1.0)
    run(para(tf, True, line=1.05), title, title_size, PALE, bold=True)

    span, n = 11.0, N_SLIDES
    pitch = span / n; wseg = pitch * 0.90
    for i in range(n):
        rect(s, M + i * pitch, 7.28, wseg, 0.045, ACCENT if i <= idx - 1 else RULE)
    tf = tb(s, 12.0, 7.14, 0.9, 0.3)
    run(para(tf, True, align=PP_ALIGN.RIGHT), f"{idx} / {n}", 8.5, MUTED)


N_ANNEX = 7          # annex GROUPS, not slides


def annex_chrome(s, grp, eyebrow, title, part=None, nparts=None, title_size=27):
    """Same chrome, amber, on its own progress track.

    The annex is GROUPED, not enumerated: slides that belong together carry the
    same tag (A2 is both CUDA-Graphs slides, A5 all three artefacts), because
    what a reader needs at a glance is which slides form one argument. The badge
    is rendered in the header, not only in the corner.
    """
    tf = tb(s, M, 0.44, 1.0, 0.34)
    run(para(tf, True), f"A{grp}", 15, AMBER, bold=True)
    tf = tb(s, 1.42, 0.50, 10.1, 0.32)
    tag = f"ANNEX · {eyebrow}" + (f" · {part} of {nparts}" if nparts else "")
    run(para(tf, True), tag.upper(), 9, MUTED, bold=True, spc=1.6)
    tf = tb(s, M, 0.86, 11.9, 1.0)
    run(para(tf, True, line=1.05), title, title_size, PALE, bold=True)
    pitch = 11.0 / N_ANNEX
    for i in range(N_ANNEX):
        rect(s, M + i * pitch, 7.28, pitch * 0.90, 0.045,
             AMBER if i <= grp - 1 else RULE)
    tf = tb(s, 11.7, 7.14, 1.2, 0.3)
    foot = f"A{grp}" + (f" · {part}/{nparts}" if nparts else "")
    run(para(tf, True, align=PP_ALIGN.RIGHT), foot, 8.5, MUTED)


def table(s, x, y, w, header, rows, colw, size=9.5, rowh=0.62):
    """Minimal header + zebra table. colw are fractions of w."""
    xs, acc = [], 0.0
    for c in colw:
        xs.append(x + acc * w)
        acc += c
    rect(s, x, y, w, 0.34, PANEL)
    for cx, h in zip(xs, header):
        tf = tb(s, cx + 0.16, y + 0.08, w, 0.24)
        run(para(tf, True), _up(h), 8, MUTED, bold=True, spc=1.2)
    yy = y + 0.38
    for i, row in enumerate(rows):
        if i % 2 == 0:
            rect(s, x, yy, w, rowh, PANEL)
        for j, (cx, cell) in enumerate(zip(xs, row)):
            col = PALE if j == 0 else TEXT2
            tf = tb(s, cx + 0.16, yy + 0.10, colw[j] * w - 0.24, rowh)
            p = para(tf, True, line=1.2)
            for k, part in enumerate(str(cell).split("**")):
                if part:
                    run(p, part, size, AMBER if k % 2 else col, bold=bool(k % 2))
        yy += rowh + 0.04
    return yy


def bullets(s, x, y, w, items, size=11, gap=7):
    tf = tb(s, x, y, w, 0.3)
    for i, it in enumerate(items):
        color, txt = (it if isinstance(it, tuple) else (TEXT2, it))
        p = para(tf, i == 0, space_after=gap, line=1.25)
        run(p, "•  ", size, MUTED)
        for j, part in enumerate(txt.split("**")):
            if part:
                run(p, part, size, PALE if j % 2 else color, bold=bool(j % 2))
    return tf


def code(s, x, y, w, lines, size=9, title=None):
    # Line height has to follow the font size: it was a constant 0.148 in tuned
    # for 8.5 pt, so raising the type to the projection floor pushed the last
    # line out of the panel. 0.0174 in/pt reproduces the old value at 8.5.
    size = max(size, MIN_PT)
    lh = 0.0174 * size
    h = 0.24 + len(lines) * lh + (0.24 if title else 0)
    box = rect(s, x, y, w, h, CODEBG, MSO_SHAPE.ROUNDED_RECTANGLE)
    box.line.fill.solid()             # rect() cleared the line; put one back
    box.line.fill.fore_color.rgb = CODEEDGE
    box.line.width = Pt(1.0)
    box.adjustments[0] = 0.055        # a small radius: a panel, not a pill
    ty = y + 0.12
    if title:
        tf = tb(s, x + 0.18, ty, w - 0.36, 0.2)
        run(para(tf, True), title, 9, CODEDIM, bold=True, spc=1.2)
        ty += 0.24
    tf = tb(s, x + 0.18, ty, w - 0.36, h - 0.24)
    for i, ln in enumerate(lines):
        p = para(tf, i == 0, line=1.12)
        if ln.strip().startswith(("//", "#")):
            run(p, ln, size, CODEDIM, MONO)
            continue
        for j, part in enumerate(ln.split("«")):
            for k, seg in enumerate(part.split("»")):
                if not seg: continue
                hi = (j > 0 and k == 0)
                run(p, seg, size, ACCENT if hi else TEXT2, MONO, bold=hi)
    return h


def callout(s, x, y, w, text, h=0.78, color=ACCENT, size=10.5):
    rect(s, x + 0.045, y, w - 0.045, h, PANEL)
    rect(s, x, y, 0.045, h, color)
    tf = tb(s, x + 0.28, y + 0.10, w - 0.5, h - 0.2, MSO_ANCHOR.MIDDLE)
    p = para(tf, True, line=1.2)
    for j, part in enumerate(text.split("**")):
        if part:
            run(p, part, size, PALE if j % 2 else TEXT2, bold=bool(j % 2))


def _up(txt):
    """upper() for labels, but 'µ'.upper() is Greek capital Mu — which renders as
    an 'M' and turns 'µs' into 'MS', i.e. microseconds into milliseconds."""
    return txt.upper().replace("\u039c", "µ")


def rail(s, items, y0=2.0, divider=True):
    if divider:
        rect(s, 8.95, 2.0, 0.012, 4.55, RULE)
    y = y0
    for it in items:
        kind = it[0]
        if kind == "label":
            tf = tb(s, RAIL_X, y, RAIL_W, 0.26)
            run(para(tf, True), _up(it[1]), 8.5, MUTED, bold=True, spc=1.4)
            y += 0.28
        elif kind == "stat":
            _, lab, val, col = it
            tf = tb(s, RAIL_X, y, RAIL_W, 0.24)
            run(para(tf, True), _up(lab), 8.5, MUTED, spc=1.2)
            tf = tb(s, RAIL_X, y + 0.24, RAIL_W, 0.6)
            run(para(tf, True), val, 26, col, bold=True)
            y += 0.98
        elif kind == "row":
            _, lab, val, col = it
            tf = tb(s, RAIL_X, y, RAIL_W, 0.24)
            run(para(tf, True), _up(lab), 8.5, MUTED, spc=1.2)
            tf = tb(s, RAIL_X, y + 0.22, RAIL_W, 0.3)
            run(para(tf, True), val, 13, col, bold=True)
            y += 0.66
        elif kind == "note":
            tf = tb(s, RAIL_X, y, RAIL_W, 0.9)
            run(para(tf, True, line=1.25), it[1], 9, TEXT2)
            y += 0.30 + 0.17 * (len(it[1]) // 42 + 1)
        elif kind == "gap":
            y += it[1]
    return y


def figure(s, name, x, y, w):
    p = FIGS / f"{name}.png"
    iw, ih = Image.open(p).size
    h = w * ih / iw
    s.shapes.add_picture(str(p), In(x), In(y), In(w), In(h))
    return h


def card_figure(s, name, x, y, w, pad=0.10):
    """A light-background figure (imported, not re-rendered) on a light card."""
    p = FIGS / f"{name}.png"
    iw, ih = Image.open(p).size
    h = w * ih / iw
    rect(s, x - pad, y - pad, w + 2 * pad, h + 2 * pad, CARD,
         MSO_SHAPE.ROUNDED_RECTANGLE)
    s.shapes.add_picture(str(p), In(x), In(y), In(w), In(h))
    return h + 2 * pad


def frame_rect(s, x, y, w, h, color=RED, wpt=1.75):
    """An outline, not a fill: used to point at one row without recolouring it."""
    sh = s.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, In(x), In(y), In(w), In(h))
    sh.fill.background()
    sh.line.color.rgb = color
    sh.line.width = Pt(wpt)
    sh.shadow.inherit = False
    sh.adjustments[0] = 0.06
    return sh


def notes(s, text):
    """Speaker notes. Detail that belongs in the talk, not on the slide.

    python-pptx creates the notes slide on first access, so this is safe to call
    on any slide. Used to relieve slides that carry a figure: the mechanism goes
    on the screen, the API detail goes here.
    """
    s.notes_slide.notes_text_frame.text = text


def caption(s, x, y, w, text, size=9):
    tf = tb(s, x, y, w, 0.3)
    run(para(tf, True, line=1.25), text, size, MUTED)


def flow(s, x, y, w, steps, h=0.62):
    """Numbered left-to-right step strip."""
    n = len(steps); gap = 0.30
    bw = (w - gap * (n - 1)) / n
    for i, t in enumerate(steps):
        bx = x + i * (bw + gap)
        rect(s, bx, y, bw, h, PANEL)
        rect(s, bx, y, 0.03, h, ACCENT)
        tf = tb(s, bx + 0.20, y + 0.05, bw - 0.32, h - 0.10, MSO_ANCHOR.MIDDLE)
        p = para(tf, True, line=1.1)
        run(p, f"{i + 1}  ", 9, ACCENT, bold=True, font=MONO)
        run(p, t, 9.5, PALE)
        if i < n - 1:
            tf = tb(s, bx + bw, y + 0.05, gap, h - 0.10, MSO_ANCHOR.MIDDLE)
            run(para(tf, True, align=PP_ALIGN.CENTER), "›", 15, MUTED, bold=True)


def statstrip(s, x, y, w, items, h=0.80):
    n = len(items); gap = 0.22
    bw = (w - gap * (n - 1)) / n
    for i, (lab, val) in enumerate(items):
        bx = x + i * (bw + gap)
        rect(s, bx, y, bw, h, PANEL)
        tf = tb(s, bx + 0.20, y + 0.12, bw - 0.4, 0.22)
        run(para(tf, True), lab.upper(), 8, MUTED, bold=True, spc=1.2)
        tf = tb(s, bx + 0.20, y + 0.37, bw - 0.4, 0.34)
        run(para(tf, True), val, 15, PALE, bold=True)


# ------------------------------------------------------------ interstitial
_NARROW, _WIDE = set("ijlt.,;:'!|()[]"), set("mwMW")


def _em(txt):
    """Width of `txt` in ems of Segoe UI Bold, near enough to wrap by.

    A character count is not good enough: 'now the tallest bar' and
    'measured, and in what' are the same length and differ by 8 % in width, which
    is exactly the margin that decides whether a title takes two lines or three.
    Weights are calibrated against a LibreOffice render of two title lines and
    reproduce both to within 1 %.
    """
    return sum(1.08 if c in _WIDE else
               0.37 if c in _NARROW else
               0.34 if c == " " else 0.68 for c in txt)


def _fit(title, w, sizes=(36, 32, 28, 24), lines=2):
    """Largest size at which `title` wraps into at most `lines` lines across `w`.

    python-pptx cannot measure text and PowerPoint's autofit does not apply until
    a render, so a title that grows one line silently walks over the rule beneath
    it. No tolerance is granted — a title that only just fits is one font
    substitution away from not fitting on someone else's machine.
    """
    for size in sizes:
        cap = w / (size / 72)
        n, cur = 1, 0.0
        for word in title.split():
            need = _em(word) + (0.34 if cur else 0)
            if cur + need > cap and cur:
                n, cur = n + 1, _em(word)
            else:
                cur += need
        if n <= lines:
            return size
    return sizes[-1]


def section(kicker, title, thesis, items, rng, col=ACCENT, carry=None,
            annex=False):
    """An unnumbered beat between sections: where we are, and what is coming.

    Deliberately sparse — it exists to buy 10–15 s of stage setting, so it has
    to be readable at a glance and finished before the audience starts reading
    ahead. It carries no slide number and takes no tick of its own on the
    progress track: slides 3–35 keep the numbers they have, so the annex's
    cross-references ("expands slide 27") stay true. What it lights up instead
    is the *range* the section covers, which is the thing the audience wants.
    """
    s = new_slide()
    rect(s, 0, 0, 0.16, H, col)

    tf = tb(s, M + 0.3, 1.52, 5.8, 0.3)
    run(para(tf, True), kicker.upper(), 9.5, MUTED, bold=True, spc=1.8)
    tf = tb(s, M + 0.3, 1.90, 6.0, 1.35)
    run(para(tf, True, line=1.03), title, _fit(title, 6.0), PALE, bold=True)
    rect(s, M + 0.3, 3.42, 1.5, 0.03, col)
    tf = tb(s, M + 0.3, 3.70, 5.7, 1.6)
    run(para(tf, True, line=1.4), thesis, 13.5, TEXT2)

    if carry:
        lab, val, sub = carry
        rect(s, M + 0.3, 5.55, 5.7, 1.12, PANEL)
        rect(s, M + 0.3, 5.55, 0.035, 1.12, col)
        tf = tb(s, M + 0.60, 5.72, 5.2, 0.24)
        run(para(tf, True), _up(lab), 8.5, MUTED, bold=True, spc=1.4)
        tf = tb(s, M + 0.60, 5.94, 5.2, 0.4)
        run(para(tf, True), val, 24, col, bold=True)
        tf = tb(s, M + 0.60, 6.40, 5.2, 0.24)
        run(para(tf, True), sub, 9.5, MUTED)

    rect(s, 7.15, 1.95, 0.012, 4.4, RULE)
    step = 0.42 if len(items) > 7 else 0.46 if len(items) > 5 else 0.54
    y = 1.95 + (4.4 - len(items) * step) / 2
    tf = tb(s, 7.45, y - 0.42, 5.0, 0.26)
    run(para(tf, True), "COMING UP", 8.5, MUTED, bold=True, spc=1.6)
    for num, txt in items:
        tf = tb(s, 7.45, y, 0.7, 0.3)
        run(para(tf, True), str(num), 12, col, bold=True, font=MONO)
        tf = tb(s, 8.15, y, 4.5, 0.3)
        p = para(tf, True)
        for j, part in enumerate(txt.split("**")):
            if part:
                run(p, part, 12, col if j % 2 else PALE, bold=bool(j % 2))
        y += step

    # The annex divider sits on the annex's own track: the main arc is finished
    # behind it, so lighting main-track segments would misreport where we are.
    n_track = N_ANNEX if annex else N_SLIDES
    pitch = 11.0 / n_track
    for i in range(n_track):
        n = i + 1
        # the section ahead in its own colour, what is already behind us dimmed,
        # the rest dark — so the divider agrees with the chrome on either side.
        c = col if annex or rng[0] <= n <= rng[1] else (
            MUTED if n < rng[0] else RULE)
        rect(s, M + i * pitch, 7.28, pitch * 0.90, 0.045, c)
    return s


# =========================================================== 1 · PSI TITLE
keep_only_slide(prs, 0)
title_slide = prs.slides[0]
by_name = {sh.name: sh for sh in title_slide.shapes}
set_para_texts(by_name["TextShape 1"], ["The CUDA ClusterFinder"])
set_para_texts(by_name["CustomShape 4"],
               ["Kernel design · hardware limits · seven optimization steps"])
set_para_texts(by_name["CustomShape 2"], ["Khalil Daniel Ferjaoui"])
set_para_texts(by_name["CustomShape 3"],
               ["Paul Scherrer Institut · aare", "August 2026"])

# =========================================================== 2 · HERO
s = new_slide()
rect(s, 0, 0, 0.16, H, ACCENT)
tf = tb(s, M + 0.3, 0.85, 11, 0.3)
run(para(tf, True), "AARE · HYBRID PIXEL DETECTORS · CUDA CLUSTERFINDER",
    9.5, MUTED, bold=True, spc=1.8)

tf = tb(s, M + 0.3, 1.28, 11.4, 1.45)
run(para(tf, True, line=1.02),
    "The kernel was never the bottleneck — feeding it was", 42, PALE, bold=True)
tf = tb(s, M + 0.3, 2.80, 11.4, 0.45)
run(para(tf, True, line=1.05),
    "One kernel, one thread per pixel, and seven steps to keep it fed", 22, ACCENT)

tf = tb(s, M + 0.3, 3.40, 11.4, 0.9)
run(para(tf, True, line=1.3),
    "The stencil was fast almost immediately: at 3×3 the kernel needs 5.5 µs per "
    "frame while getting that frame across PCIe costs 16.6 — 13.2 even "
    "uncontended. The frame is gated by the wire, not the arithmetic. Six of the "
    "seven steps get data in, get results back, and measure honestly.",
    12.5, TEXT2)

tf = tb(s, M + 0.3, 4.40, 11.4, 0.26)
run(para(tf, True),
    "WHERE THIS ENDS UP — THE SHIPPED f32 BUILD AFTER ALL SEVEN STEPS · "
    "ENGINE TIMES [f32 · s4] · RECONCILED IN A1",
    8.5, AMBER, bold=True, spc=1.4)

stats = [("×9.1", "VS 24-THREAD CPU", ACCENT), ("61,312", "FRAMES / SECOND", PALE),
         ("16.3 µs", "PER FRAME, END TO END", PALE), ("6 / 23 M", "CLUSTER MISMATCH VS CPU TWIN", AMBER)]
for i, (v, l, c) in enumerate(stats):
    x = M + 0.3 + i * 2.85
    rect(s, x, 4.78, 0.035, 0.95, c)
    tf = tb(s, x + 0.22, 4.78, 2.5, 0.55)
    run(para(tf, True), v, 30, c, bold=True)
    tf = tb(s, x + 0.22, 5.41, 2.5, 0.3)
    run(para(tf, True), l, 8.5, MUTED, spc=1.2)

rect(s, M + 0.3, 6.05, 11.0, 0.012, RULE)
tf = tb(s, M + 0.3, 6.25, 11.4, 0.6)
run(para(tf, True, line=1.35),
    "RTX 4090 (Ada, sm_89) · PCIe 4.0 ×16 · Mönch 400×400 uint16 · 3×3 clusters · "
    "100 000 frames · Cu fluorescence, MAX IV", 10, MUTED)

# ------------------------------------------------------- divider · context
section("Context · what the code does",
        "What the kernel does, and what limits it",
        "No optimizations yet, only what the hardware has to do.",
        [("3–4", "The algorithm"),
         ("5–6", "The two machines"),
         ("7–8", "The CUDA kernel"),
         ("9–10", "What limits it"),
         ("11", "The roadmap")],
        rng=(3, 11))

# =========================================================== 3 · THE PHYSICS
s = new_slide()
chrome(s, 3, "The algorithm · what it is for",
       "A photon is not a pixel — it is a cluster")
bullets(s, M, 1.90, 7.9, [
    "Charge from one absorbed photon **spreads over neighbouring pixels**. "
    "Summing that 3×3 patch recovers the photon energy; a single pixel does not.",
    "The histogram of those cluster energies **is** the measurement: peak position "
    "and width give the detector's gain and **energy resolution**.",
], size=10.5)
figure(s, "fig_frame", M, 2.98, 7.45)
rail(s, [("label", "Why it matters")], y0=1.95)
h = card_figure(s, "img_spectra", RAIL_X, 2.32, RAIL_W)
caption(s, RAIL_X, 2.32 + h + 0.10, RAIL_W,
        "Cluster-energy spectra from an energy scan, against allpix² simulation. "
        "Each peak is one beam energy; its width is the resolution being measured.",
        size=8.5)
rail(s, [
    ("label", "MÖNCH03 · the detector this feeds"),
    ("row", "Array · pitch · active area", "400 × 400 · 25 µm · 10 × 10 mm²", TEXT2),
    ("row", "Frames per second", "1.3 k standard, 3–6 k optimised", AMBER),
    ("row", "Peak pixels = photons / frame", "~2 330   ·   1.5 %", ACCENT),
], y0=4.92, divider=False)
caption(s, M, 6.68, 7.9,
        "Real MOENCH data, Cu fluorescence, MAX IV beamtime. One cluster is emitted "
        "per local maximum, so 2 330 counts photons, not lit pixels; the recorded "
        "3×3 windows cover 12.7 % of the frame. The detector's frame rate is the "
        "number every throughput figure in this deck should be read against.")
notes(s, """The detector, and why its frame rate is the yardstick.

MOENCH03 is a hybrid silicon pixel detector: charge integration with analog
readout, 25 x 25 um^2 pitch, 400 x 400 pixels over a 10 x 10 mm^2 active area.
Its standard frame rate is 1.3 kHz; with optimised readout boards the design
reaches 3-6 kHz depending on configuration.

That is the number to hold on to. 400 x 400 = 160 000 pixels is 312.5 kB per
frame at 16 bit, so 1.3 kHz is ~0.4 GB/s off the detector and 6 kHz is ~1.9
GB/s. The 24-thread CPU finder does 6 762 FPS at 3x3, so it already keeps up
with the standard mode and roughly matches the optimised ceiling -- with nothing
left over for anything else on the machine, and nothing left at 9x9, where it
manages 1 503 FPS. Slide 33 closes this loop.

Pixels above 5 sigma are ~5 700, about 3.6 % of the frame and 2.4 per photon;
that is the number that sets how much of the frame the 3x3 windows cover.""")

# =========================================================== 4 · PER FRAME
s = new_slide()
chrome(s, 4, "The algorithm · per frame", "Per pixel: subtract, threshold, update the pedestal")
bullets(s, M, 1.95, COL, [
    "Per pixel: subtract a **running pedestal** (mean ± rms), keep pixels above "
    "**nσ · rms**, cut a 3×3 cluster around each local maximum.",
    "400×400 = 160 k pixels, **312.5 kB per frame**; Cu data yields ~2 330 clusters "
    "per frame at 3×3.",
    "The pedestal is **updated by every pixel that sees no photon**, about 80 % of "
    "them, every frame, so the arithmetic and the data movement are coupled.",
])
code(s, M, 3.55, COL, [
    "// the whole algorithm, per pixel",
    "v   = frame[i] - pedestal_mean[i]",
    "rms = pedestal rms at i",
    "m   = max(v) over the 3x3 window at i",
    "if (m > «nSigma» * rms)                  // a photon is within reach",
    "    if (v == m) -> emit cluster        //   ... and I am its peak",
    "    else        -> «nothing»             //   ... I am in its shadow",
    "else            -> update pedestal     // I saw nothing",
], title="THE WHOLE ALGORITHM · THREE OUTCOMES, NOT TWO")
callout(s, M, 5.55, COL,
        "**Thesis of this talk:** the compute was fast almost immediately. "
        "Six of the seven steps are about feeding it.")
rail(s, [
    ("label", "The shape of the work"),
    ("gap", 0.15),
    ("stat", "Work items per frame", "160 000", PALE),
    ("row", "Operations on each", "~5, identical", TEXT2),
    ("gap", 0.12),
    ("row", "Communication between them", "none", ACCENT),
    ("row", "Order they may run in", "any", ACCENT),
    ("gap", 0.22),
    ("note", "What a pixel does never depends on what its neighbours decided, only "
             "on what they measured. That one property is what the next two slides "
             "point two very different machines at."),
])

# ==================================================== 5 · THE CPU
# The two machine slides. They exist because the audience is asked, from slide 7
# on, to accept "one thread per pixel" and "occupancy" without ever having been
# shown what a thread costs on either machine. Both diagrams are the CS149 ones
# (credited in the captions): redrawing them would lose the shared visual
# grammar — orange fetch/decode, yellow ALU, blue execution context — which is
# the entire reason the pair reads at a glance.
s = new_slide()
chrome(s, 5, "The machine we are starting from",
       "CPU: latency-oriented, built to finish one thread fast")
figure(s, "img_cpu_core", M, 1.84, 6.5)
# The die photo mirrors slide 6's: same grammar, compute units boxed, so the two
# machines are compared as objects and not only as block diagrams. It is kept
# WHOLE rather than cropped to the ten cores, because the L3 slab on the right
# and the I/O block on the left are half the die area -- which is the callout's
# point standing next to it in silicon.
figure(s, "img_cpu_die", M, 5.04, 4.55)
callout(s, 5.42, 5.04, 3.18,
        "Count the boxes: **6 fetch/decode**, out-of-order instruction selection, two "
        "levels of private cache, all of it to keep **two** instruction streams fed. "
        "The ALUs are the small part.", h=1.34, size=10)
caption(s, 5.42, 6.50, 3.18,
        "Comet Lake · 10 cores boxed; nearly half the die is cache and I/O.", size=8)
caption(s, 5.42, 6.88, 3.18,
        "Both diagrams after Stanford CS149, Fall 2025.", size=8)
rail(s, [
    ("label", "pc-moench-04 · AMD Ryzen 9 7950X"),
    ("gap", 0.10),
    ("stat", "Cores × SMT", "16 × 2", PALE),
    ("gap", 0.04),
    ("stat", "CPU, 1 thread", "1.75 ms", MUTED),
    ("stat", "CPU MT, 24 threads", "148 µs", PALE),
    ("gap", 0.06),
    ("row", "That is the bar", "6 762 frames / s", ACCENT),
    ("gap", 0.16),
    ("note", "Thread count is swept per cluster size, not assumed: "
             "24 at 3×3, 32 at 9×9."),
])
notes(s, """The CPU slide. The point is not that CPUs are bad -- it is what the
silicon is SPENT on.

On the diagram: Intel Skylake is shown, one core, schematically. The Zen 4 core
in this machine differs in detail -- 4 FP pipes rather than 3, AVX-512
double-pumped on 256-bit datapaths -- but not in kind: ~6-wide front end, 4
scalar ALUs, 2 SMT contexts, private L1 + L2. Load/store units are not drawn.

On the die photo: Comet Lake, a 10-core Core i9, at the same scale of argument
as the AD102 die on the next slide. Ten cores, and they do not fill the die --
the orange slab on the right is L3 and the block on the left is I/O and the
memory controller. If someone asks why a 16-core Ryzen is not simply 16x a
1-core Ryzen, that picture is the answer.

Six fetch/decode units, an out-of-order instruction selector, branch prediction
(not even drawn), L1 + L2 private cache: all of that machinery exists to find
independent work INSIDE one instruction stream, and to hide memory latency
behind a cache. It is the right design when you have a few threads that must
each finish fast.

Our problem has the opposite shape: 160 000 work items that are already
independent. We do not need a machine to FIND the parallelism -- it is handed to
us. Every transistor spent looking for it is a transistor not doing arithmetic.

On the thread sweep: 6 762 FPS is the best of a measured sweep (perf/cpu_threads.py),
not a default. 48 threads on 16 cores is 24 % SLOWER than 24 -- worth saying out
loud, because an oversubscribed baseline is the easiest way to inflate a GPU
speedup without lying about anything.""")

# ==================================================== 6 · THE GPU
s = new_slide()
chrome(s, 6, "The machine we are moving to",
       "GPU: throughput-oriented, the whole frame at once")
figure(s, "img_gpu_die", M, 2.10, 2.75)
caption(s, M, 4.92, 2.75,
        "AD102 · 144 blocks, 128 enabled on this card. One SM boxed.", size=7.5)
figure(s, "img_gpu_sm", M + 3.05, 1.92, 4.75)
# The colour key is a separate crop: in the source it spans the full slide width
# while the diagram spans 60 % of it, so one rectangle cannot hold both.
figure(s, "img_gpu_legend", M + 3.05, 5.26, 4.75)
callout(s, M, 6.02, COL,
        "Same grammar, inverted proportions: **4 fetch/decode** for **64 warp "
        "contexts** and a wall of lanes. Nothing reorders instructions: when a warp "
        "stalls on memory, the selector just **runs a different one**.", h=0.86,
        size=10)
caption(s, M, 6.82, COL,
        "One SM: a V100 is shown; this card's is the same idea — 128 FP32 lanes, "
        "48 warp slots, 100 kB shared memory. After Stanford CS149, Fall 2025.", size=8)
rail(s, [
    ("label", "NVIDIA GeForce RTX 4090"),
    ("gap", 0.12),
    ("stat", "FP32 lanes", "16 384", PALE),
    ("row", "128 SMs × 128 lanes", "vs 32 CPU streams", TEXT2),
    ("gap", 0.14),
    ("stat", "Resident thread slots", "196 608", ACCENT),
    ("row", "The frame needs", "160 000  →  all at once", AMBER),
    ("gap", 0.20),
    ("note", "Device memory runs at 1 008 GB/s (384-bit, 21 Gbps); PCIe delivers 25. "
             "Two copy engines, so H2D and D2H run at the same time."),
])
notes(s, """The GPU slide, and the number to land.

128 SMs x 1 536 threads = 196 608 thread slots that can be RESIDENT at the same
time. Our frame is 160 000 pixels. The entire frame fits in the machine at once,
one thread per pixel, with room left over -- which is why slide 7's "one thread
per pixel" is not a figure of speech, and why slide 10 can talk about occupancy
as a real quantity (3x3 achieves 100 %: 1 536 threads resident per SM).

Latency hiding, in one sentence: the CPU hides memory latency with a cache and
out-of-order execution; the GPU hides it by having 48 other warps ready to run.
That is why there is no reorder buffer on this diagram and no branch predictor.

Count the pink cells in the legend before moving on: FP64 units are a small
minority of the SM, 8 MUL/ADD per clock against 16 for fp32 and 16 for int. On
a GeForce part the ratio is far worse than this V100 diagram suggests -- 1/64.
That is half of why opt7 pays, and it is worth planting here so opt7 lands as a
consequence rather than a surprise.

The trap to pre-empt: 16 384 lanes vs 32 streams is a factor of 512, and we
measure x9.1. Say so before someone else does. We are not lane-limited; we are
BANDWIDTH-limited -- 1 TB/s of device memory, and 25 GB/s of PCIe to reach it.
That gap between 512 and 9 IS the talk.""")

# =========================================================== 7 · THE KERNEL
s = new_slide()
chrome(s, 7, "The CUDA kernel · execution model", "One thread per pixel")
flow(s, M, 1.90, 11.9, ["load tile + halo", "__syncthreads", "stencil reduction",
                        "classify", "append or update pedestal"])
bullets(s, M, 2.85, 7.5, [
    "A **16×16 block = 256 threads** covers 256 pixels; the grid tiles the "
    "whole 400×400 frame. Cluster geometry is a **template parameter**, so the "
    "stencil is fully unrolled at compile time.",
    "The output is **sparse**: only detections touch global memory, through one "
    "atomic bump of a per-frame counter. The **decision work is dense**: every "
    "pixel is tested, independently and identically.",
    "That is exactly the shape a GPU wants: regular, independent, repeated "
    "160 000 times per frame.",
], size=10.5)
code(s, M, 4.35, 7.5, [
    "block = dim3(BLOCK_X, BLOCK_Y);            // 16 x 16",
    "grid  = dim3((ncols + BLOCK_X - 1)/BLOCK_X,",
    "             (nrows + BLOCK_Y - 1)/BLOCK_Y);",
    "device::find_clusters_in_single_frame<<<grid, block, «shmem», stream>>>(",
    "    d_frame, d_pd_mean, d_pd_sum, d_pd_sum2, d_pd_off, n_pd_samples,",
    "    nSigma, nrows, ncols, d_clusters, d_cluster_count, max_clusters);",
], size=8, title="LAUNCH CONFIGURATION · ClusterFinderCUDA.hpp")
caption(s, M, 5.94, 7.5,
        "400×400 pixels → a 25×25 grid of 16×16 blocks = 625 blocks per frame, "
        "handed to 128 SMs. Nothing about the launch depends on how many clusters "
        "the frame happens to contain, which is what makes the work uniform.")
code(s, 8.5, 2.85, 4.1, [
    "// ClusterFinder.hpp — the serial CPU",
    "if (max > nSigma * rms) {",
    "    if (value < max)",
    "        continue;  // Not max go to the",
    "                   // next pixel — but",
    "                   // also no pedestal",
    "                   // update",
    "} else {",
    "    pedestal.«push_fast»(iy, ix, ...);",
    "}",
], size=8, title="THREE OUTCOMES, NOT TWO")
callout(s, 8.5, 5.04, 4.1,
        "That comment is **verbatim from the CPU source**. A pixel in a photon's "
        "shadow is **neither recorded nor fed back**, and the CUDA kernel "
        "reproduces it exactly.",
        h=1.15, size=10)
callout(s, 8.5, 6.10, 4.1,
        "**~80 % of threads update the pedestal**: every pixel with no photon in "
        "its window. ~1.5 % are peaks, ~18 % are shadow. The update, not the "
        "cluster write, dominates.",
        h=1.1, size=10, color=AMBER)

# =========================================================== 6 · TILING
s = new_slide()
chrome(s, 8, "The CUDA kernel · shared memory", "Load the tile once, reuse it nine times")
bullets(s, M, 1.92, 12.0, [
    "Neighbouring threads need **overlapping** 3×3 windows. Without shared memory "
    "each pixel would be fetched from global memory up to nine times.",
    "Each block stages a tile of (16 + 2r) × (16 + 2r) **pedestal-subtracted** "
    "values, the halo is the price of the stencil, and it is loaded cooperatively "
    "by the threads on the block edges.",
], size=10.5)
figure(s, "fig_tile", M, 3.05, 7.5)
code(s, 8.4, 3.05, 4.3, [
    "extern __shared__ unsigned char smem[];",
    "auto *sh = (COMPUTE_TYPE*)smem;",
    "auto stride = blockDim.x + 2*col_radius;",
    "auto tid = (threadIdx.y + row_radius)*stride",
    "         + (threadIdx.x + col_radius);",
    "// pedestal subtraction fused into the load",
    "sh[tid] = d_frame[gid] - «d_pd_mean»[gid];",
], size=8, title="clusterfinder_kernel.cuh")
callout(s, 8.4, 4.80, 4.3,
        "Only **odd** cluster sizes are supported (3×3, 5×5, 7×7, 9×9), so that "
        "the centre pixel is unique and local-maximum suppression is well defined.",
        h=1.15, size=10)
caption(s, 8.4, 6.06, 4.3,
        "The tile is stored in COMPUTE_TYPE (float), not in the pedestal type: "
        "1.3 KB for 3×3, 2.3 KB for 9×9, against 100 KB of shared memory per SM. "
        "Even the old double-precision tile only reached 4.5 KB.")
caption(s, M, 6.70, 7.5,
        "Halo cost falls with block size: 56 % of the tile at 8×8, 27 % at 16×16, "
        "13 % at 32×32, which is the first half of the block-size argument. "
        "The second half is registers, next slide.")

# ================================================= 7 · REGISTERS (the input)
# Registers come BEFORE occupancy, not after. Occupancy is an output of the
# register budget, so presenting the percentage first and the cause second asked
# the audience to learn a metric and then be told it was not the point.
s = new_slide()
chrome(s, 9, "Hardware · what runs out first",
       "38 registers per thread at 3×3, 128 at 9×9")
bullets(s, M, 1.90, COL, [
    "An SM has a fixed budget: **65 536 registers** and **1 536 thread slots**. "
    "A 16×16 block claims 256 slots and 256 × (registers per thread), and whichever "
    "budget runs out first decides how many blocks fit on the SM at once.",
    "Every thread keeps a private **clusterData[CSX × CSY]** staging array, so "
    "register demand grows with the **square** of the cluster size. Neither build "
    "spills: ptxas would rather hold fewer blocks than go to local memory.",
], size=10.5)
figure(s, "fig_regpressure", M, 3.34, 7.9)
callout(s, M, 5.30, COL,
        "**At 9×9 the register file is exactly full at two blocks** (2 × 128 × 256 "
        "= 65 536), so two thirds of the thread slots are stranded. At 3×3 the "
        "slots run out first and the registers still have room.", h=0.80, size=10.5)
code(s, M, 6.22, COL, [
    "«cuobjdump -res-usage» build/aare/_aare_cuda*.so | c++filt",
    "  3x3: REG:«38»  STACK:0 LOCAL:0     # STACK/LOCAL 0 = no spills",
    "  9x9: REG:«128» STACK:0 LOCAL:0",
], size=8.5, title="MEASURED, NOT ESTIMATED · READ FROM THE BUILT .SO")
rail(s, [
    ("label", "Per SM · sm_89 · f32 build"),
    ("gap", 0.15),
    ("stat", "3×3 · blocks resident", "6", ACCENT),
    ("stat", "9×9 · blocks resident", "2", AMBER),
    ("gap", 0.05),
    ("row", "Spills, either case", "0 bytes", TEXT2),
    ("row", "3×3 on the f64 build", "47 regs → 5 blocks", AMBER),
    ("gap", 0.20),
    ("note", "How many blocks fit is the whole hardware story. The next slide is "
             "what that buys."),
])
notes(s, """The arithmetic, and the one build-dependent caveat.

One SM holds 65 536 registers and 1 536 thread slots. A 16x16 block is 256
threads, so a block costs regs_per_thread x 256 registers. At 3x3 that is 9 728,
six blocks fit, and the THREAD SLOTS run out first. At 9x9 it is 32 768, so two
blocks exactly fill the REGISTER FILE and strand two thirds of the slots. Same
kernel, same block size, opposite binding resource.

Build dependence of the register count itself: the f64 pedestal costs 3x3 nine
extra registers (47), which loses a block per SM. 9x9 is unmoved at 128 -- there
the limiter is the clusterData[9][9] staging array, not the pedestal, which is
why opt7 helps 3x3's occupancy and not 9x9's.

Reproduce blocks/SM at runtime with python/tests/perf/kernel_resources.py, which
calls cudaOccupancyMaxActiveBlocksPerMultiprocessor on the built kernel.""")

# ================================================ 8 · OCCUPANCY (the output)
s = new_slide()
chrome(s, 10, "Hardware · occupancy",
       "Occupancy is a latency-hiding budget")
bullets(s, M, 1.86, 11.9, [
    "When a warp stalls on memory the SM switches to another warp that is already "
    "resident. **Occupancy = resident warps / the maximum the SM can hold**: how "
    "many alternatives it has to switch to. More resident warps, more stalls hidden.",
], size=11)
statstrip(s, M, 2.44, 11.9, [
    ("block size", "16 × 16"),
    ("threads / block", "256"),
    ("thread slots / SM", "1 536"),
    ("max warps / SM", "48"),
    ("occupancy 3×3 · 9×9", "100 % · 33 %"),
], h=0.74)
figure(s, "fig_occupancy", 1.37, 3.34, 10.6)
callout(s, M, 6.44, 11.9,
        "**16×16 is the balance point**: enough threads to amortise the halo, few "
        "enough that 6 blocks still fit. But 33 % is not a failure to fix — it is "
        "what the register budget allows, and at 9×9 one kernel nearly fills the "
        "machine on its own.", h=0.62, size=10.5)
notes(s, """Why "33 % occupancy" is not the alarm it looks like.

Occupancy only buys latency hiding, and latency hiding only matters if there is
latency left unhidden. At 9x9 the kernel is long and each thread does a lot of
work per byte loaded, so 16 resident warps are enough to keep the SM fed.

It is also build-dependent, and that is the honest caveat. On the shipped f32
build the 9x9 kernel overlaps across four streams by only 1.02x -- extra streams
buy nothing in kernel concurrency. On the f64 build the kernel is 3.5x longer
and DOES overlap, 1.32x, and there four streams genuinely lower the floor. So
quote the overlap factor, never "occupancy is fine" on its own.

Shared memory never binds here: 1.3 KB per block at 3x3, 2.3 KB at 9x9, against
100 kB per SM.

Provenance for both hardware slides: shipped f32 build, measured not estimated.
cuobjdump -res-usage for registers and spills;
cudaOccupancyMaxActiveBlocksPerMultiprocessor for blocks per SM; RTX 4090
(sm_89). Reproduce with python/tests/perf/kernel_resources.py.""".rstrip())
# =========================================================== 10 · THE LADDER
s = new_slide()
chrome(s, 11, "Roadmap",
       "Three acts, ordered by which bar is tallest")
rows = [
    ("act", "ACT I  ·  feed the GPU", "the host cannot submit work fast enough", "at 3×3  [f64]", ACCENT),
    ("opt1", "First CUDA port", "1 stream, one launch per frame", "×2.34", ACCENT),
    ("opt2", "Streams + batching", "4 streams, 2 000-frame batches", "×3.66", ACCENT),
    ("opt3", "Pipeline rework", "sync barriers removed", "×4.32", ACCENT),
    ("opt4", "Pinned memory", "DMA-speed host transfers", "×5.69", ACCENT),
    ("act", "ACT II  ·  get the results back", "the host copy is now the tallest bar", "at 3×3  [f64]", PALE),
    ("opt5", "Host↔GPU overlap", "chunked submit / collect", "×7.45", PALE),
    ("opt6", "Zero-copy collection", "read in place, never copy", "×8.65", PALE),
    ("act", "ACT III  ·  the kernel", "the kernel is the tallest bar, at 9×9", "at 9×9  [f32]", AMBER),
    ("opt7", "FP32 pedestal + variance rewrite", "the only kernel change in the deck",
     "kernel −41%", AMBER),
]
y = 1.70
for i, (tag, name, sub, gain, col) in enumerate(rows):
    if tag == "act":
        rect(s, M, y + 0.30, 11.9, 0.016, col)
        tf = tb(s, M + 0.02, y, 8.0, 0.28)
        run(para(tf, True), name.upper(), 11, col, bold=True, spc=1.4)
        tf = tb(s, M + 5.2, y + 0.03, 6.7, 0.26)
        run(para(tf, True, align=PP_ALIGN.RIGHT), f"{sub}   {gain}", 9, MUTED)
        y += 0.40
        continue
    rect(s, M, y, 11.9, 0.48, PANEL)
    rect(s, M, y, 0.035, 0.48, col)
    tf = tb(s, M + 0.28, y + 0.09, 1.0, 0.32)
    run(para(tf, True), tag, 12.5, col, bold=True, font=MONO)
    tf = tb(s, M + 1.45, y + 0.04, 5.0, 0.28)
    run(para(tf, True), name, 11.5, PALE, bold=True)
    tf = tb(s, M + 1.45, y + 0.26, 5.6, 0.26)
    run(para(tf, True), sub, 9, MUTED)
    tf = tb(s, 9.4, y + 0.09, 3.1, 0.35)
    run(para(tf, True, align=PP_ALIGN.RIGHT), gain, 12.5, col, bold=True)
    y += 0.52
caption(s, M, 6.62, 11.9,
        "Speedups are 3×3 vs the best CPU configuration, 24 threads. Three routes were "
        "measured and rejected: CUDA Graphs, and two faster ways of copying the results. "
        "All three are in annex A2, because the rule that predicts the wins predicts "
        "the failures too.")

# --------------------------------------------------------- divider · ACT I
section("Act I of III · feed the GPU",
        "The host cannot submit work fast enough",
        "The kernel was fast almost immediately. This act is about the host, and it "
        "is told at 3×3, where the wire is the floor.",
        [(12, "opt1 · first port"),
         (13, "opt2 · streams + batching"),
         (14, "opt3 · no barriers"),
         (15, "opt3 · one D2H, not two"),
         (16, "opt4 · pinned memory")],
        rng=(12, 16),
        carry=("Starting from", "6 762 FPS",
               "24-thread CPU · 14.8 s for 100 000 frames"))

# =========================================================== 11 · OPT1
s = new_slide()
chrome(s, 12, "Act I · opt1 · the first CUDA port",
       "The first port runs at 26 % of the GPU's floor")
bullets(s, M, 1.95, COL, [
    "Shared-memory tiling with **halo loading** for any cluster size; pedestal "
    "subtraction fused into the tile load.",
    "Cluster geometry is a **compile-time template parameter** → the 3×3 stencil "
    "is fully unrolled.",
    "One cudaMemcpy in, one kernel, one cudaMemcpy out; **the host blocks "
    "on every frame**.",
])
code(s, M, 3.18, COL, [
    "// one frame at a time, the host waits at every step",
    "cudaMemcpy(d_frame, h_frame, bytes, cudaMemcpyHostToDevice);",
    "find_clusters_in_single_frame<ClusterType, FRAME_TYPE>",
    "    <<<grid, block, shmem>>>(d_frame, d_pd_mean, ...);",
    "cudaMemcpy(h_out, d_out, out_bytes, cudaMemcpyDeviceToHost);",
], title="ClusterFinderCUDAOpt2.hpp · find_clusters()")
callout(s, M, 4.62, COL,
        "**PCIe is full-duplex**: H2D, kernel and D2H run on independent engines and "
        "overlap, so the **floor** — the fastest a frame can go if the host cost "
        "nothing — is **max(H2D, kernel, D2H)**, never the sum. At 3×3 that is "
        "max(**16.17**, 15.17, 7.69) = **16.2 µs → 61 859 FPS**. Exactly how each of "
        "those three is measured is slide 20; it does not change this one.",
        h=1.00, size=10.5)
figure(s, "fig_opt1_timeline", M, 5.80, COL)
rail(s, [
    ("label", "opt1 · 3×3 · 100 k frames · f64"),
    ("gap", 0.10),
    ("stat", "Throughput", "15 807 FPS", PALE),
    ("stat", "vs 24-thread CPU", "×2.34", ACCENT),
    ("gap", 0.05),
    ("row", "Per frame", "63.3 µs", TEXT2),
    ("row", "The GPU floor", "61 859 FPS", ACCENT),
    ("row", "% of floor", "26 %", AMBER),
])

# =========================================================== 12 · OPT2
s = new_slide()
chrome(s, 13, "Act I · opt2 · streams and batching",
       "Four streams and 2 000-frame batches: ×1.56")
bullets(s, M, 1.95, COL, [
    "A **stream** is an ordered queue of GPU work. Work in **different** streams may "
    "overlap, so a copy can run while another stream computes.",
    "Each stream gets its own **StreamContext**: device frame buffer, output buffer "
    "and pedestal. Frames are handed out **round-robin**.",
    "The host now submits **2 000 frames per call** instead of one.",
])
code(s, M, 3.30, COL, [
    "struct StreamContext {",
    "    cudaStream_t   stream;",
    "    FRAME_TYPE    *d_frame;      ClusterType *d_clusters;",
    "    PEDESTAL_TYPE *d_pd_mean, *d_pd_sum, *d_pd_sum2;",
    "};",
    "auto &sc = v_sc[frame_idx % «n_streams»];   // round-robin",
], title="ClusterFinderCUDA.hpp · per-stream state")
figure(s, "fig_opt2_timeline", M, 4.77, COL)
callout(s, M, 6.48, COL,
        "**Scaffolding, not yet the payoff.** The streams exist, but the host still "
        "synchronises after every round: see opt3.", h=0.60)
rail(s, [
    ("label", "opt2 · 3×3 · 4 streams · batch 2 000"),
    ("gap", 0.10),
    ("stat", "Throughput", "24 726 FPS", PALE),
    ("stat", "vs 24-thread CPU", "×3.66", ACCENT),
    ("gap", 0.05),
    ("row", "Per frame", "40.4 µs", TEXT2),
    ("row", "Step gain over opt1", "×1.56", ACCENT),
    ("row", "% of floor · 61 859 FPS", "40 %  (was 26)", AMBER),
])

# =========================================================== 13 · OPT3
s = new_slide()
chrome(s, 14, "Act I · opt3 · remove the sync barriers",
       "One sync per batch, not one per round: ×1.18")
bullets(s, M, 1.95, 7.4, [
    "opt2 synchronised **all streams after every round** of n_streams frames. "
    "The GPU drained to empty each time.",
    "opt3 submits every frame's H2D → kernel → D2H **asynchronously**, then "
    "synchronises **once at the end of the batch**.",
], size=10.5)
figure(s, "fig_streams", M, 3.05, 6.55)
code(s, 8.35, 1.95, 4.25, [
    "// opt2:  barrier after every round",
    "for (round) {",
    "   submit(n_streams frames);",
    "   «cudaDeviceSynchronize»();",
    "}",
    "",
    "// opt3:  submit everything, sync once",
    "for (frame : batch) {",
    "   cudaMemcpyAsync(..., sc.stream);",
    "   kernel<<<..., sc.stream>>>(...);",
    "   cudaMemcpyAsync(..., sc.stream);",
    "}",
    "for (sc : streams)",
    "   «cudaStreamSynchronize»(sc.stream);",
], size=8, title="THE ONE-LINE IDEA")
callout(s, 8.35, 5.05, 4.25,
        "**29 188 FPS · ×4.32**\n34.3 µs/frame · 47 % of floor (was 40)", h=0.86, size=11)
caption(s, 8.35, 6.15, 4.25,
        "Each lane is one stream. Removing the barrier lets a stream start its next "
        "frame while its neighbours are still copying. The three panels are scheduled, "
        "not sketched: H2D and D2H are one FIFO engine each, so a stream waits for the "
        "copy engine, never for another stream's copy to finish overlapping it.")

# ===================================================== 15 · OPT3b · ONE D2H
# opt3's title has always said "barriers", plural, but the deck only ever told
# one of them: the per-round cudaDeviceSynchronize. The count-then-fetch round
# trip went at the same step and was never shown, which made opt2 -> opt3 look
# like a refactor instead of the change of contract it was.
s = new_slide()
chrome(s, 15, "Act I · opt3 · the other barrier",
       "One D2H per frame, not two")
bullets(s, M, 1.92, 11.9, [
    "opt2 asked the device **how many clusters**, blocked until the answer came "
    "back, then asked for **that many**. The size of the second copy was a "
    "function of data that had not arrived yet.",
    "opt3 gives every frame a **fixed envelope** — count, then room for **cap** "
    "clusters — so the copy's size is known at construction and can be queued "
    "with the kernel. The count is still read, but **afterwards**, on the host.",
], size=11)

flow(s, M, 3.42, 11.9,
     ["kernel", "copy 4 B", "BLOCK", "read count", "copy N B", "BLOCK"], h=0.62)
caption(s, M, 4.12, 11.9,
        "opt2 · two transfers and two stalls per frame, because the second one "
        "cannot be issued until the first has landed.", size=9.5)

flow(s, M, 4.62, 11.9,
     ["kernel", "copy the whole envelope", "→ next frame, host not involved"], h=0.62)
caption(s, M, 5.32, 11.9,
        "opt3 · one transfer, no stall. Nothing in the loop waits on a value.", size=9.5)

callout(s, M, 5.86, 11.9,
        "**You cannot stream a transfer whose length depends on the transfer "
        "before it.** opt3 pays bytes to delete that dependency: the envelope is "
        "sized by the **cap**, not by how many clusters were found, so an empty "
        "frame costs the same D2H as a full one — 120 kB at 3×3 against 93 kB of "
        "real clusters.", h=0.94, size=10.5)
caption(s, M, 6.94, 11.9,
        "Everything downstream needs that fixed layout: opt6 could not hand out a "
        "view into a buffer whose shape was not known in advance.", size=9)
notes(s, """The point to say out loud: this is the one step in the ladder that is
not a setting. It changed the kernel signature, the buffer ownership and the
collection loop, and it is why the opt1/opt2 class is frozen in a separate header
(ClusterFinderCUDAOpt2.hpp) rather than being a flag on the current one.

The dependency edge is the whole argument. In opt2 the second memcpy's SIZE
argument is *sc.h_cluster_count -- host memory that only becomes valid after a
cudaStreamSynchronize (Opt2.hpp:277-294, then :442-446). So the sequence is
forced: kernel, copy 4 bytes, BLOCK, read, copy N bytes, BLOCK. Two of those six
steps are the host doing nothing, every frame.

opt3 fixes the size once in the constructor (m_output_bytes_per_frame =
m_clusters_offset + cap * sizeof(ClusterType), ClusterFinderCUDA.hpp:440-445), so
the copy is enqueued in the same loop iteration as the kernel launch (:725-728).
The kernel gets two pointers into ONE allocation (:701 and :709).

What it costs: 120 kB instead of 93 kB per frame at 3x3, 558 instead of 467 at
9x9. Roughly 20 % more bytes on an engine that had spare time, to buy back a
barrier that was stalling everything. That is also why the cap becomes a
throughput knob only from opt3 onward: under opt2 it bounded an allocation, under
opt3 it sets the D2H bar directly (report section 4.2).

If asked why opt2 did not simply copy a cap-sized buffer and skip the sync: that
IS opt3. It could not be done in opt2 because the clusters lived in their own
device allocation with no count field and no fixed per-frame stride -- there was
no single object to copy. Merging the two buffers is what created one.""")


# =========================================================== 14 · OPT4
s = new_slide()
chrome(s, 16, "Act I · opt4 · pinned (page-locked) memory",
       "Pinning the input buys DMA-speed H2D: ×1.32")
bullets(s, M, 1.95, 12.0, [
    "Normal host memory is **pageable**: the OS may move or swap it. A DMA engine "
    "cannot safely read that, so the driver first copies your data into a **hidden "
    "pinned staging buffer**. Every transfer is copied twice.",
    "**Pinning** locks the pages in physical RAM. The GPU's DMA engine then reads "
    "host memory **directly**, no staging copy, and the transfer can be truly asynchronous.",
], size=10.5)
figure(s, "fig_pinning", M, 3.15, 7.6)
code(s, 8.5, 3.15, 4.1, [
    "// pin the whole dataset once",
    "«cudaHostRegister»(ptr, bytes,",
    "                 cudaHostRegisterDefault);",
    "",
    "// ... run the whole campaign ...",
    "",
    "«cudaHostUnregister»(ptr);",
], size=8, title="ClusterFinderCUDA.hpp")
callout(s, 8.5, 4.90, 4.1,
        "**38 486 FPS · ×5.69**\n26.0 µs/frame · 62 % of floor, the largest step in Act I",
        h=0.86, size=11)
caption(s, 8.5, 5.94, 4.1,
        "Measured H2D [s1, uncontended]: one 400×400 uint16 frame (312.5 KiB = "
        "320 000 B) in 13.2 µs = 24.2 GB/s, 77 % of PCIe 4.0 ×16 theoretical, i.e. "
        "true DMA speed. In the shipped pipeline it reads 16.6 [s4], +26 % of "
        "H2D↔D2H contention (A1). Pageable staging runs ~15 GB/s.")
callout(s, M, 6.45, 7.6,
        "**The rule, first sighting: ×1.32 at 3×3 but only ×1.03 at 9×9.** Pinning "
        "attacks H2D, the tallest bar at 3×3, the shortest at 9×9.",
        h=0.72, size=10, color=AMBER)

# -------------------------------------------------------- divider · ACT II
section("Act II of III · get the results back",
        "The host copy is now the tallest bar",
        "Frames go in at DMA speed. The results still come back slowly. Still 3×3, "
        "but 9×9 is where this act pays most.",
        [(17, "opt5 · host↔GPU overlap"),
         (18, "9×9 · why overlap runs out"),
         (19, "opt6 · zero-copy")],
        rng=(17, 19), col=PALE,
        carry=("Arriving at", "38 486 FPS",
               "opt4 · 26.0 µs per frame · 62 % of the GPU floor"))

# =========================================================== 15 · OPT5
s = new_slide()
chrome(s, 17, "Act II · opt5 · host↔GPU overlap",
       "Overlapping host and GPU hides min(host, GPU): ×1.31")
bullets(s, M, 1.92, COL, [
    "opt3 overlapped H2D ∥ kernel ∥ D2H **across streams, inside one batch**, but "
    "never the **host** with the GPU: find_clusters_batched synchronised, then built "
    "thousands of ClusterVectors with the GPU idle.",
    "opt5 keeps **one batch in flight while materialising the previous one**: chunk "
    "i+1 is submitted before chunk i is collected.",
])
figure(s, "fig_overlap", M - 0.15, 3.26, COL + 0.30)
code(s, M, 5.88, COL, [
    "tok = cf.«submit_batch»(data[a0:b0], first_frame=a0)",
    "for a, b in bounds[1:]:",
    "    nxt = cf.«submit_batch»(data[a:b], first_frame=a)   // GPU starts i+1",
    "    results.extend(cf.«collect»(tok))                   // host unpacks i",
    "    tok = nxt",
], size=9, title="you never write these: find_clusters_batched() wraps them")
notes(s, """opt5 — host<->GPU overlap. Code and chunk sizing are on annex A3.

    tok = cf.submit_batch(data[a0:b0], first_frame=a0)
    for a, b in bounds[1:]:
        nxt = cf.submit_batch(data[a:b], first_frame=a)   # GPU starts N+1 ...
        results.extend(cf.collect(tok))                   # ... host unpacks N
        tok = nxt
    results.extend(cf.collect(tok))                       # drain

1. This is now INTERNAL to find_clusters_batched(), so every caller gets it for
   free. submit_batch/collect stay public for anyone who wants the token by hand.

2. The chunk size is rounded to a multiple of n_streams, because the device
   pedestal is per-stream: an uneven chunk would advance the four stream
   pedestals by different amounts and the finders would stop being comparable.

Why the gain differs by cluster size: the saving is min(GPU, host) per chunk, so
it is largest when the two terms are comparable. At 3x3 they nearly are (GPU
16.2 us, host ~9.8) and opt5 is worth x1.31. At 9x9 the host term is roughly
twice the GPU term, so overlap hides only the smaller one and opt5 is worth
x1.20 - which is exactly the diagnosis that motivates opt6: you cannot overlap
your way out of a host term that is simply larger. Report SS8.2.""")
rail(s, [
    ("label", "opt5 · 3×3 · no CUDA work at all"),
    ("gap", 0.10),
    ("stat", "3×3 throughput", "50 410 FPS", PALE),
    ("row", "per frame · step · of floor", "19.8 µs · ×1.31 · 81 %", ACCENT),
    ("gap", 0.16),
    ("label", "the whole change"),
    ("gap", 0.06),
    ("row", "CUDA API calls added", "none", TEXT2),
    ("row", "what moved", "the host loop", PALE),
    ("gap", 0.16),
    ("note", "Told at 3×3, where the host copy is SHORTER than the GPU floor and "
             "tucks underneath it. 9×9 is the other case, and it is the next "
             "slide. For clusters that must outlive the finder, 3×3 opt5 is the "
             "endpoint: opt6 lends, it does not give."),
])


# ============================================ 18 · WHY OVERLAP RUNS OUT AT 9x9
# The bridge from opt5 to opt6. Slide 17 is told at 3x3, where the host copy fits
# underneath the GPU floor and two slots are plainly enough. At 9x9 the host is
# the taller bar and the room reliably guesses "add more slots" -- so the picture
# answers that guess directly, by drawing three slots and landing on the same
# finish line. Once buffering is ruled out, opt6 is the only move left.
s = new_slide()
chrome(s, 18, "Act II · opt5 at 9×9 · why overlap runs out",
       "Overlap runs out: the host is the taller bar")
bullets(s, M, 1.84, 11.9, [
    "At 3×3 the host copy is **shorter than the GPU floor** and hides underneath "
    "it. At 9×9 it is **roughly twice the floor** — ~62 µs of malloc-and-copy "
    "against 30.01 µs of GPU — so overlap still works, it just has less to hide. "
    "That is why opt5 is worth **×1.20** here and ×1.31 at 3×3.",
], size=10.5)
figure(s, "fig_overlap_9x9", M + 0.15, 2.44, 11.3)
callout(s, M, 6.30, 11.9,
        "A deeper buffer **relocates the GPU's idle, it does not close it** — the "
        "host lane is already back-to-back in both strips, so it alone sets the "
        "pace. **The only way down is to make the host term smaller.**",
        h=0.66, size=10.5)
caption(s, M, 7.06, 11.9,
        "Measured proportions: GPU 30.01 µs/frame, host ~62 µs steady-state. "
        "Fault correction and the raw 66.4 µs are in the notes and annex A4.",
        size=9)
notes(s, """This slide exists because "add more slots" is the reliable guess here,
and it is worth letting the room say it out loud before the second strip goes up.

The queueing argument, if it is asked for: with producer period G and consumer
period H, an N-buffer pipeline has steady-state period max(G, H) for every N >= 2.
Buffers DECOUPLE two stages; they do not speed up either. Depth beyond 2 only
helps when the periods vary -- it absorbs jitter, at the cost of latency and
pinned memory. Here the work per chunk is near constant, so there is no jitter to
absorb.

The concrete version lands better: a third slot needs somebody to fill it, and
the only host thread is inside collect(). Adding slots without adding a producer
thread is adding storage to a queue that is not storage-bound.

"So would a producer thread help?" It would let submit and collect overlap, but
the host term is dominated by malloc + first touch, which is allocator-serialised
anyway. That was measured and reverted (ClusterFinderCUDA.hpp:130-136): 2.27 M
faults at 8 threads against 9.7 k at 1, a 6 % gain at best and a 33 % LOSS when
results are freed promptly. The comment there ends with the right conclusion --
stop allocating per frame, do not copy faster. That is opt6.

ON THE 62 US, if challenged. It is not measured directly; no one timed the host
loop. Two independent routes agree on it. (1) Fault-correct the five f64 reps at
0.68 us/fault -- the same rate fitted at 3x3 and applied out of sample -- and a
22 % raw spread collapses to 4.6 %, at 61-64 us. (2) f32 rep 3 happened to run
with only 10 128 faults and measured 61.85 us with no correction at all. The
"~40 us" that used to be on the opt6 slide was the memcpy alone, computed at
bandwidth; the loop is allocation-bound, not bandwidth-bound, so it under-counted
the host term by about half.""")

# =========================================================== 19 · OPT6
s = new_slide()
chrome(s, 19, "Act II · opt6 · zero-copy collection",
       "Read the results in place: ×2.21 at 9×9")
bullets(s, M, 1.92, COL, [
    "The D2H lands in a **pinned host buffer**. collect() then allocates one "
    "ClusterVector per frame and memcpys into it; at 9×9 that is **467 kB per frame, "
    "~9.3 GB per run**, single-threaded.",
    "collect_view() returns a **BatchView**: strided numpy views straight onto the "
    "pinned buffer. It withholds ownership past the chunk, **not access**, every "
    "cluster's payload and coordinates are readable.",
], size=10.5)
figure(s, "fig_resultpath", M - 0.15, 3.05, COL + 0.30)
callout(s, M, 5.86, COL,
        "The win is **max(0, host copy − GPU floor)**: at 3×3 the 8 µs copy hides under "
        "a 16.2 µs floor and opt5 had already absorbed most of it; at 9×9 the ~62 µs "
        "host term is **twice the floor** and cannot hide at any overlap.", h=0.80, size=10)
caption(s, M, 6.78, COL,
        "The two bars are the competing costs, not the two steps; the step times are "
        "on the right.", size=8.5)
notes(s, """Where 93 kB and 467 kB come from: clusters/frame x sizeof(Cluster).

sizeof — Cluster.hpp:28 is two CoordType coords then std::array<T, X*Y> data.
CoordType=uint16, T=int32, alignof 4, so the coords pack with no padding:
    3x3:  2x2 B +  9x4 B =  40 B
    9x9:  2x2 B + 81x4 B = 328 B

clusters/frame — report section 13, the same counts used for correctness:
    3x3:  233 094 390 / 100 000 fr = 2 330.9 /fr
    9x9:   28 447 962 /  20 000 fr = 1 422.4 /fr

    2 330.9 x  40 B =  93.2 kB/frame
    1 422.4 x 328 B = 466.5 kB/frame  ->  x 20 000 fr = 9.33 GB/run

NOT the D2H transfer. D2H is fixed at cap x sizeof regardless of how many clusters
were found: at the 9x9 campaign cap of 1700 that is 1700 x 328 B = 558 kB every
frame, i.e. the 467 kB of real clusters is an 84 % fill (section 4.2). The 467 kB
is the HOST memcpy inside collect(), pinned buffer -> freshly allocated
ClusterVector. That is what collect_view() removes; the wire traffic is unchanged.

Report: section 12 (payload table), section 9.4 (materialize_slot), section 13.""")

rail(s, [
    ("label", "opt6 · 3×3 and 9×9 · collect_view()"),
    ("gap", 0.10),
    ("stat", "3×3 throughput", "58 495 FPS", PALE),
    ("row", "per frame · step · of floor", "17.1 µs · ×1.16 · 95 %", ACCENT),
    ("gap", 0.14),
    ("stat", "9×9 throughput", "33 323 FPS", PALE),
    ("row", "per frame · step · of floor", "30.0 µs · ×2.21 · 100 %", AMBER),
    ("gap", 0.12),
    ("note", "opt5 → opt6: 19.8 → 17.1 and 66.4 → 30.0 µs, bit-identical, "
             "0.2 % spread, zero warm faults. FLOOR = lower of the s4 engine max "
             "and the best sustained rate: at 3×3 the 16.17 µs max sets it; at 9×9 "
             "f64 the run BEATS the 32.66 µs max, so the 30.0 sustained sets it [A1]."),
])

# ------------------------------------------------------- divider · ACT III
section("Act III of III · the kernel",
        "Only now is the kernel the tallest bar",
        "The story moves to 9×9, where the kernel is finally the tallest bar. First, "
        "how the engine times in this act are measured.",
        [(20, "how the engine times are measured"),
         (21, "opt7 · FP32 pedestal"),
         (22, "catastrophic cancellation"),
         (23, "why it comes last")],
        rng=(20, 23), col=AMBER,
        carry=("Arriving at", "58 495 FPS",
               "opt6 · 3×3, and 33 323 FPS at 9×9, where this act pays"))

# ============================================ 10b · THE MEASUREMENT CONVENTION
# Promoted out of the annex. Everything from here on is quoted as "[build · s1]"
# or "[build · s4]" and compared against a "floor", and none of those three words
# had been defined anywhere the audience would see them. The annex keeps the full
# grid of numbers; this slide keeps only the three definitions and the one
# picture that makes the middle one make sense.
s = new_slide()
chrome(s, 20, "How the engine times are measured",
       "Two configurations, one floor")
cards = [
    ("s1", ACCENT, "One stream, nothing else running",
     "How long an operation actually takes. The right number for a capability "
     "claim, and for the headroom that is left."),
    ("s4", PALE, "The shipped pipeline, four streams",
     "How BUSY each engine is per frame: the union of its intervals, which is "
     "not the sum of the durations. The only number that can set a floor."),
    ("floor", AMBER, "Set by the busiest engine",
     "The fastest a frame could go if the host cost nothing. In µs per frame, or "
     "its reciprocal in FPS. The LOWER of the profiled estimate and the best "
     "rate sustained."),
]
x = M
for tag, col, title, body in cards:
    rect(s, x, 1.82, 3.83, 1.70, PANEL)
    rect(s, x, 1.82, 3.83, 0.035, col)
    tf = tb(s, x + 0.26, 2.00, 3.3, 0.30)
    run(para(tf, True), tag, 14, col, bold=True, font=MONO)
    tf = tb(s, x + 0.26, 2.34, 3.35, 0.30)
    run(para(tf, True, line=1.1), title, 11, PALE, bold=True)
    tf = tb(s, x + 0.26, 2.68, 3.35, 0.80)
    run(para(tf, True, line=1.22), body, 9.5, TEXT2)
    x += 4.03
figure(s, "fig_measure", 1.52, 3.62, 10.3)
callout(s, M, 6.48, 11.9,
        "**Nothing that measures a duration falls under load** — yet the 9×9 kernel "
        "row falls 39.9 → 32.7 µs from s1 to s4. That is the tell: s4 is occupancy, "
        "not duration. **The floor is quoted both ways in this deck** — 30.01 µs per "
        "frame is 33 323 FPS. Full engine grid: **annex A1**.", h=0.62, size=10.5)
notes(s, """Say the floor out loud, in this order.

1. There are three engines and they are independent: the H2D copy engine, the
   SMs, and the D2H copy engine. PCIe is full duplex, so a frame's cost is
   never the sum of the three -- it is the slowest of them.

2. There is exactly ONE copy engine per direction. So two H2D copies can never
   run at the same time; they queue. That is not a modelling assumption, it is
   measured: H2D_overlap and D2H_overlap are 1.000 in every row of probes.csv.
   Kernels are the only row that ever overlaps.

3. That is why s4 is a UNION, not a sum. Four kernels, each 43.2 us long, but
   the SMs are busy only 32.66 us per frame: overlap factor 1.32x.

4. On units, because the card says "1 / the busiest engine" and the axis says
   microseconds: they are the same number. The busiest engine's busy time per
   frame is in us/frame; one divided by it is frames per second. 30.01 us/frame
   IS 33 323 FPS. The deck quotes whichever reads better in context -- us/frame
   when comparing engines, FPS when comparing against the CPU or the detector --
   and "% of floor" is the same ratio either way.

5. The floor is max(H2D, kernel, D2H) at s4. At 9x9 f64 that max is 32.66 us
   = 30 618 FPS. But the unprofiled pipeline actually sustained 30.01 us/frame,
   which is FASTER than the probe says is possible -- because under nsys
   submission is sparser, kernels overlap less, and a less-overlapped interval
   set has a LARGER union. A sustained rate is an existence proof; a probe is an
   estimate. So the floor is the lower of the two, and the 1.32x is a lower
   bound on the real overlap.

If time is short, say only: "s1 is how long it takes, s4 is how busy it is, and
the floor is the busiest engine." The rest is in A1.""")


# =========================================================== 18 · OPT7 why
s = new_slide()
chrome(s, 21, "Act III · opt7 · FP32 device pedestal",
       "FP32 halves pedestal traffic: −41 % kernel time")
bullets(s, M, 1.95, 7.5, [
    "**~80 % of pixels** take the **pedestal-update** branch: it reads off, sum and "
    "sum², and writes back sum, sum² and mean. All four pedestal arrays are "
    "DEVICE_PED_TYPE, so one typedef halves all six accesses: **48 bytes per "
    "updating pixel in FP64, 24 in FP32**.",
    "The kernel is **bandwidth-bound**, so halving that traffic nearly halves the "
    "time. On a GeForce part there is a second effect: **FP64 arithmetic runs at "
    "1/64 of FP32**, and the pedestal update was paying that tax on every pixel.",
    "Quietly, a third: the narrower accumulators free **9 registers at 3×3**, 47 → "
    "38, which buys back a block per SM.",
], size=10.5)
figure(s, "fig_f32_kernel", M, 3.90, 7.5)
code(s, 8.5, 1.95, 4.1, [
    "// clusterfinder_kernel.cuh",
    "using COMPUTE_TYPE    = float;",
    "using DEVICE_PED_TYPE = «float»;",
    "//            was: double",
], size=9, title="ONE TYPEDEF")
callout(s, 8.5, 3.24, 4.1,
        "Kernel, 9×9 **[s1 · cap 1700]**\n**39.86 → 23.70 µs  (−40.5 %)**\n"
        "Shipped **[s4]**: **30.0 → 25.1 µs** end to end.",
        h=1.32, size=10.5)
callout(s, 8.5, 4.68, 4.1,
        "Naive FP32 is **wrong** (next slide), and even when correct it only pays "
        "**because Act II came first**.", h=0.86, size=10.5, color=AMBER)
callout(s, 8.5, 5.76, 4.1,
        "At 3×3 the same typedef is worth **−70.6 %** (14.72 → 4.32 µs [s1]) yet buys "
        "only **4.6 %** end to end: there the kernel was never the tallest bar.",
        h=1.00, size=10.5)
caption(s, 8.5, 6.94, 4.1,
        "Both builds, same git rev, 20 000 frames. Full engine grid: A1.", size=9)
notes(s, """Reading the two panels, and the number to quote.

Left is s1: one stream, nothing else running, so those are true durations. The
kernel binds in BOTH arms there -- 39.86 and 23.70 against a D2H of ~21.95 -- so
s1 alone would say the kernel is the thing to optimise and stop there.

Right is s4, the shipped four-stream pipeline, and it says something different:
the f64 kernel's busy time falls to 32.66 (self-overlap, 1.32x) while every
transfer RISES under contention, and once opt7 puts the kernel at 23.94 the D2H
bar at 25.24 is above it. That is the handover slide 23 is about.

So: -40.5 % is the s1 kernel claim and is the honest headline for what the
typedef does to the arithmetic. -26.7 % is the same change measured at s4, where
self-overlap had already hidden part of the win. Both are true, they measure
different things, and the previous slide is the rule for which to quote.

The 3x3 occupancy effect is real but not worth stage time: 47 registers -> 38
takes 3x3 from five blocks per SM to six, 83 % -> 100 % occupancy. It changes
nothing end to end, because at 3x3 H2D is the floor.""")


# ================================================ 19 · THE TRAP AND THE FIX
# Was two slides. The split spent one whole slide on the ULP arithmetic of the
# error floor, which is the least transferable part of the story: what the
# audience needs is that a tiny answer computed as the difference of two huge
# numbers is not computable in f32, what that did to the physics, and the
# two-line change that removes it. The error-floor curve and the full rewrite
# are annex A5.
s = new_slide()
chrome(s, 22, "Act III · opt7 · catastrophic cancellation",
       "Accumulate what is small, not what is large")
bullets(s, M, 1.90, COL, [
    (TEXT2, "**The trap.** The variance was computed as **var = E[X²] − mean²**. "
     "With a pedestal at ~4 655 ADU both operands are ≈ 2.17 × 10⁷ while the answer "
     "is ≈ 2 000. FP32 carries ~7 digits, so the answer inherits an **absolute** "
     "error of **±3 ADU², which does not shrink as the answer does**."),
    (TEXT2, "**What it cost.** For a quiet pixel whose true variance is 9, ±3 is a "
     "third of it; below rms ≈ 2 the variance goes negative, the rms **clamps to "
     "zero**, and its 5σ gate becomes a 0σ gate. **~1–2 % of the sensor** then fires "
     "every frame: **+28.06 % clusters** and a population of clusters below the "
     "physical threshold."),
    (TEXT2, "**The fix.** Freeze **X₀ = round(mean)** once at the end of pedestal "
     "training and accumulate the **centred** value **Y = X − X₀**. Both operands "
     "become O(rms) and the cancellation is gone."),
], size=10.5)
figure(s, "fig_cancellation", M, 3.92, COL)
caption(s, M, 6.42, COL,
        "Left: the two operands and the answer, log scale, against the ±3 ADU² error. "
        "Right: the f64 curve is measured (23.2 M clusters); the f32 curve is "
        "reconstructed — measured area, modelled shape, method in the notes. "
        "The two-line patch itself is in annex A5.", size=9)
rail(s, [
    ("label", "naive f32 · what it did"),
    ("gap", 0.12),
    ("stat", "Extra clusters", "+28.06 %", AMBER),
    ("row", "Pixels affected", "~1–2 % of the sensor", AMBER),
    ("gap", 0.28),
    ("label", "after the rewrite"),
    ("gap", 0.12),
    ("stat", "f32 vs f64 counts", "3 × 10⁻⁷", ACCENT),
    ("row", "vs the CPU baseline", "0.0039 %", ACCENT),
    ("gap", 0.20),
    ("note", "X₀ must never be updated: the accumulators are defined relative to it."),
])
notes(s, """The one sentence to leave the room with.

"Precision is relative." A float resolves small numbers finely and large numbers
coarsely, so a small answer must never be computed as the difference of two
large numbers. That is the transferable lesson; everything else on this slide is
this detector's instance of it.

Why the tail matters more than the count. +28 % is a number you might argue
about. A spectrum with a population sitting below the 5 sigma cut is not
arguable: those clusters cannot physically be there, and any gain or resolution
fit done on that spectrum is wrong.

Honesty about the right-hand panel: nobody kept the broken build around to
re-run, so the f32 curve is reconstructed. What is measured is the f64 curve and
the +28.06 % excess (CPU 116 010 113 vs CUDA 148 559 598, SS1 of the write-up).
The placement follows SS9: a correct 5 sigma gate cuts at ~225 ADU, a collapsed
gate admits the whole positive side of the pixel's distribution, so the excess
is smeared upward from zero. The write-up's TL;DR calls it a high-energy tail;
SS9 derives it from ~0 upward. Both describe the same corrupted pixels.

Welford's online variance is the other correct answer and is mentioned in the
write-up. The frozen-offset form was chosen because it is two lines and does not
change the update's arithmetic cost.""")


# =========================================================== 21 · OPT6 when
s = new_slide()
chrome(s, 23, "Act III · why this act comes last",
       "The saving never grew — the frame around it shrank")
figure(s, "fig_f32_absolute", M, 1.95, 11.9)
callout(s, M, 4.86, 5.85,
        "**The same typedef saves −4.63 µs at opt4 and −4.87 µs at opt6**: the two "
        "readings whose fault counts match. What changes is the denominator: the frame "
        "falls 79.8 → 30.0 µs, so an identical saving reads **−5.8 %, then −16.2 %**. "
        "Act II did not make the kernel win bigger; it made it **separable**.",
        h=1.28, size=10.5)
callout(s, 6.75, 4.86, 5.85,
        "**And the act ends by handing the floor away.** At **s4**, the config that "
        "ships, the f64 arm is kernel-bound: **32.66** against a **25.25 µs** D2H. The "
        "typedef puts the kernel at **23.94**, **below a D2H that never moved**. "
        "The constraint is now the result path, not the arithmetic. [engines: 19/33]",
        h=1.28, size=10.5, color=AMBER)
caption(s, M, 6.30, 11.9,
        "9×9 · 20 000 frames · 4 streams · cap 1 700 · warm · both arms at the same git "
        "rev. opt3 is excluded: its two arms sat in different allocator states, so that "
        "comparison does not report the typedef at all. † opt5's arms differ the same "
        "way; its reading agrees with the other two but is not independently "
        "attributable. Both are worked through in annex A4. The f32 bar at opt6 is "
        "opt7, the shipped build; the f32 bars at opt4 and opt5 are the same typedef "
        "applied at earlier steps, configurations that exist only to make this "
        "comparison controlled.")

# ------------------------------------------------------- divider · results
section("Results · what came out of it",
        "The whole ladder, and how to use it",
        "Both cluster sizes end to end, and the audit behind the numbers.",
        [("24–25", "Results, both cluster sizes"),
         ("26", "Where the time went"),
         ("27–28", "What the numbers survived")],
        rng=(24, 28), col=PALE,
        carry=("Arriving at", "58 495 FPS",
               "opt6 · everything after this is the ladder seen whole"))

# =========================================================== 22 · RESULTS
s = new_slide()
chrome(s, 24, "Results · 3×3", "×9.1 at 3×3, sitting on the H2D floor")
figure(s, "fig_arc", 1.95, 1.88, 9.4)
callout(s, M, 5.80, 5.85,
        "**×9.1 over 24 CPU threads**, 14.8 s → 1.63 s for 100 000 frames, "
        "and **at the H2D floor**.", h=0.8)
callout(s, 6.75, 5.80, 5.85,
        "Every step is **monotonic**, and correctness is held constant **throughout**: "
        "0.004 % against the CPU baseline; against the CPU twin that isolates the port, "
        "**exact on the f64 pedestal** and **6 clusters in 23 M** on the shipped f32 "
        "(slides 29–32).", h=0.8, color=AMBER)
caption(s, M, 6.62, 11.9,
        "3×3 clusters · nσ = 5 · 100 000 frames · batch 2 000 · 4 streams · 5 reps · "
        "warm = best of reps 1–4 (collect() does not converge, it oscillates between "
        "allocator states) · each step in its own process · CPU baseline = "
        "ClusterFinderMT at its best thread count, 24 here, first pass only.")

# =========================================================== 23 · RESULTS 9x9
s = new_slide()
chrome(s, 25, "Results · 9×9", "×26.5 at 9×9, and opt7 hands the floor to D2H")
figure(s, "fig_arc_9x9", 1.95, 1.88, 9.4)
callout(s, M, 5.80, 5.85,
        "**×26.5 over 32 CPU threads**; the kernel is the tallest bar for the whole "
        "f64 arm, and opt7's −40 % drops it **below D2H**.", h=0.8)
callout(s, 6.75, 5.80, 5.85,
        "opt4 buys **×1.03** here and **×1.32** at 3×3. Same code, opposite regimes, "
        "the rule, twice.", h=0.8, color=AMBER)
caption(s, M, 6.62, 11.9,
        "9×9 · cap 1 700 (lossless; 1 500 truncated 0.0095 % of clusters) · 20 000 frames · "
        "opt1/opt2 are 3×3-only · CPU baseline ClusterFinderMT at 32 threads. The floor is the "
        "lower of the nsys estimate and the best sustained rate; both arms are sustained-bound, "
        "corroborated to 8.8 % (f64) and 0.4 % (f32). Why the arms differ: 544.5 kB of D2H "
        "costs 25.2 µs on both [s4], hidden under the 32.7 µs f64 kernel, not under "
     "the 23.9 µs f32 one.")

# =========================================================== 24 · WHERE TIME GOES
s = new_slide()
chrome(s, 26, "Where the time actually went",
       "The host bar dies first, then the floor itself drops")
figure(s, "fig_overhead", 1.95, 1.95, 9.4)
callout(s, M, 5.30, 5.85,
        "**Acts I and II never touch the arithmetic.** The GPU floor is a flat "
        "16.2 µs at 3×3 / 30.0 µs at 9×9; what collapses is everything stacked on it.",
        h=0.86, size=10.5)
callout(s, 6.75, 5.30, 5.85,
        "**Act III is the only step that lowers the floor itself**, and it could not "
        "have been seen until the stack above it was gone.", h=0.86, size=10.5,
        color=AMBER)
caption(s, M, 6.45, 11.9,
        "Blue/white/amber = the GPU floor for that act's build: the LOWER of the s4 "
        "engine max (max of H2D, kernel, D2H; PCIe is full duplex, so never the sum) "
        "and the best sustained rate. At 3×3 the engine max sets it, 16.17 µs; at 9×9 "
        "f64 the sustained rate does, 30.01 against a 32.66 µs max. Grey = everything "
        "the host adds on top. At 9×9 the host contributes +50 µs at opt3 and nothing "
        "at opt6.")

# ======================================================= 24 · MEASUREMENT AUDIT
# Was a three-card slide that gave equal weight to page faults, CUDA-event
# timing and profiler overhead. Only the first of the three changes a number the
# audience is about to be shown, and only the first is something they will hit
# themselves. The other two are instrument caveats: named here, worked through
# in A6. The full three-card version is A6·1.
s = new_slide()
chrome(s, 27, "Behind the numbers · the artefact that dominates",
       "A GPU benchmark mostly measures the operating system")
bullets(s, M, 1.90, COL, [
    "Every run that keeps its results materialises **~10 GB of clusters**. The "
    "first pass **faults in ~2.6 M pages**, and each fault costs the kernel a page "
    "it must find and **zero** before the write can proceed.",
    "At **0.7 µs a fault**, that is up to **4 seconds of pure OS work inside the "
    "timer** — on a run whose GPU work is under 2 seconds. Nothing about the GPU "
    "changed; kernel time is constant across every one of these runs. Nor is it a "
    "warm-up you can wave away: it is what a **user's first run** looks like, "
    "which is the next slide.",
], size=10.5)
code(s, M, 3.76, COL, [
    "# every timed cell in the benchmark notebook is bracketed with:",
    "mf0 = resource.getrusage(resource.RUSAGE_SELF).ru_minflt",
    "...   t = time.perf_counter() - t0   ...",
    "print(f'minor faults: {mf1-mf0:,}')   # quote the run where this plateaus",
], size=9, title="THE PROTOCOL · python/tests/ClusterFinderCUDA_perf.ipynb")
callout(s, M, 5.10, COL,
        "**The protocol: re-run until getrusage() minor faults plateau** (< 200 k), "
        "and quote that run. Validated: **wall = steady-state + faults × 0.68 µs** "
        "reproduced a 6.110 s run to within **1 ms**.", h=0.86, size=10.5)
callout(s, M, 6.06, COL,
        "**Two instrument caveats, named and then set aside.** Under nsys, host-side "
        "CUDA API calls read ~4× high and wall time inflates with them, so every wall "
        "time in this deck comes from an **unprofiled** run and only GPU-side "
        "timestamps come from nsys. CUDA events measure a stream, not a kernel. "
        "Both worked through in **A6**.", h=0.86, size=10.5, color=AMBER)
rail(s, [
    ("label", "one 100 000-frame pass · 9×9"),
    ("gap", 0.12),
    ("stat", "Pages faulted in", "~2.6 M", AMBER),
    ("stat", "Cost inside the timer", "up to 4 s", AMBER),
    ("gap", 0.08),
    ("row", "Cost per fault, fitted", "0.68 µs", TEXT2),
    ("row", "Major faults, all campaign", "0", ACCENT),
    ("gap", 0.24),
    ("note", "No disk is involved. A minor fault is the kernel finding a physical "
             "page and zeroing it, which is mandatory and cannot be avoided by "
             "copying faster. Only by not allocating, which is opt6."),
])


# =========================================================== 25 · FIRST RUN
s = new_slide()
chrome(s, 28, "Behind the numbers · what a user actually gets",
       "A first run loses a third of its throughput to page faults")
figure(s, "fig_first_run", 1.37, 1.70, 10.6)
callout(s, M, 5.98, 5.85,
        "**Everything that materialises clusters loses a third of its throughput "
        "on the first run**, +7 to +20 µs per frame, depending on how much of it "
        "the GPU can hide behind its own work.", h=0.68, size=9.5)
callout(s, 6.75, 5.98, 5.85,
        "**Only the two ends escape, for opposite reasons.** opt1 never grows the "
        "heap: it discards each frame. opt6 never needs one, and reaches **98 % "
        "of its peak on a cold process**.", h=0.68, size=9.5, color=AMBER)
caption(s, M, 6.76, 11.9,
        "Single pass, one process, every ClusterVector retained, "
        "python/tests/ClusterFinderCUDA_perf.ipynb, f32, the same 100 000 frames. Not the "
        "campaign's \"cold\" rep, which discards results and so never grows the heap. Repeat "
        "the run and the amber bars climb onto the blue ones; opt1 and opt6 never move, "
        "because neither ever paid.", size=8.5)

# --------------------------------------------------- divider · VALIDATION
section("Validation · does it find the same photons",
        "Every number so far assumed the answers are identical",
        "Whether the CUDA finder returns the same clusters as the CPU.",
        [("29–30", "The fair comparison"),
         ("31–32", "The residual, dissected"),
         ("33–34", "For users"),
         ("35", "What is next")],
        rng=(29, 35), col=PALE,
        carry=("Established", "×9.1 and ×26.5",
               "on the hardware floor at both cluster sizes, if the physics holds"))

# ============================================ 26 · PEDESTAL UPDATE TIMING
s = new_slide()
chrome(s, 29, "Validation · why a CPU twin was needed",
       "CPU and CUDA update the pedestal at different moments")
figure(s, "fig_pedtiming", M - 0.15, 1.90, 12.2)
callout(s, M, 5.30, 5.85,
        "The serial CPU finder updates the pedestal **as the raster scan reaches each "
        "pixel**. A CUDA thread cannot: 160 000 of them read the pedestal at once, so "
        "the update is **applied at the frame boundary**.", h=1.02, size=10.5)
callout(s, 6.75, 5.30, 5.85,
        "So a straight CPU↔CUDA comparison moves **two** things at once. "
        "**ClusterFinderFrozen** is the serial finder with only the update moved to "
        "the frame end: same arithmetic, same gates, same scan.", h=1.02, size=10.5,
        color=AMBER)
caption(s, M, 6.52, 11.9,
        "Frozen is a diagnostic twin, not a product: it exists so the next two slides "
        "can attribute each disagreement to exactly one cause. cpu vs frozen = update "
        "timing; frozen vs cuda = the port. Every finder on these slides is trained on "
        "the same 1 000 pedestal frames and run over the same 10 000 data frames.")
notes(s, """Why this slide is here.

The obvious experiment - run the CPU finder and the CUDA finder over the same
frames and count the differences - cannot answer the question anyone actually
asks, which is "is the port correct?" It moves two variables at once:

  1. WHEN the pedestal is updated. The serial CPU finder updates per pixel,
     during the raster scan, so a pixel late in the frame is judged against a
     pedestal that already contains this frame's earlier pixels. That is
     scan-order dependent by construction.
  2. WHAT arithmetic runs. float vs double, a different local-max expression,
     a different rounding point.

ClusterFinderFrozen holds (2) fixed and changes only (1): it is the serial
finder, same decision logic line for line, with the update deferred to the frame
boundary. That is the CUDA update model, on the CPU.

So the three-way comparison factorises:
    cpu    vs frozen  -> update timing alone      (19 of 23.2 M)
    frozen vs cuda    -> the port alone           (6 of 23.2 M)

and the second is the number that answers the question. It is also why the
headline mismatch figure in this deck is 6 / 23 M and not 25 / 23 M: the larger
number is dominated by an effect that has nothing to do with CUDA.

Frozen ships in the library as a diagnostic, not as the recommended finder.""")

# =========================================================== 26 · CORRECTNESS
s = new_slide()
chrome(s, 30, "Validation · isolating one variable at a time",
       "CUDA and its CPU twin agree exactly: 0 in 23 million")
bullets(s, M, 1.86, 12.0, [
    "**ClusterFinderFrozen** makes byte-for-byte the same decisions as ClusterFinder "
    "and differs in exactly one thing: **when** the pedestal is updated. Frozen per "
    "frame, pushed at frame end. That is the CUDA model, so comparing against it "
    "isolates everything else the port changes.",
], size=10.5)
table(s, M, 2.52, 12.0,
      ["comparison", "the one thing that differs", "A-only / B-only",
       "% of clusters"],
      [["serial CPU  vs  frozen CPU", "update timing alone: **a CPU-only effect**",
        "8 / 11", "0.000082 %"],
       ["serial CPU  vs  CUDA  [f64 ped]", "the same thing, **and nothing else**",
        "8 / 11", "0.000082 %"],
       ["frozen CPU  vs  CUDA  [f64 ped]", "**nothing**", "**0 / 0**", "**0 %**"],
       ["frozen CPU  vs  CUDA  [f32 ped]", "the float32 pedestal EMA drifting: "
        "see next slide", "0 / 6", "0.000026 %"]],
      colw=[0.28, 0.40, 0.17, 0.15], size=9.5, rowh=0.60)
callout(s, M, 5.76, 5.85,
        "**Identical, frame by frame.** 23 244 605 clusters, not one disagreement, "
        "and **cpu vs cuda** equals **cpu vs frozen** exactly, so the port adds nothing "
        "of its own. Payloads: **99.9941 % bit-identical**, worst case 1 ADU on one "
        "pixel.", h=0.74, size=9.5)
callout(s, 6.75, 5.76, 5.85,
        "**The 0.004 % headline is the baseline disagreeing with itself.** "
        "ClusterFinderMT builds **48 ClusterFinders, each with its own Pedestal**, "
        "after 100 k frames each has ~2 083 updates, not 100 000. It measures the "
        "baseline, not the port.", h=0.74, size=9.5, color=AMBER)
caption(s, M, 6.58, 11.9,
        "3×3 · 10 000 frames · 23.2 M clusters · same pedestal, same frames · exact "
        "centre-set difference at tol = 0 · python/tests/validation_tiers.py. [f64 ped] "
        "and [f32 ped] are the same source built with DEVICE_PED_TYPE double / float; "
        "COMPUTE_TYPE is float in both, so the stencil arithmetic is identical across "
        "the two rows. Either row is reproduced by rebuilding with that typedef and "
        "re-running python/tests/ClusterFinderFrozen_vs_CUDA.ipynb, which counts and "
        "localises every disagreement rather than only totalling them.")

notes(s, """Row 1 and row 2 are the same 8/11, which is the point: everything
CUDA changes is worth zero, and the whole residual is a CPU-vs-CPU effect.

WHICH TEST the timing moves is measured in annex A7, and the answer is specific:

    frozen-only 11 clusters  ->  ALL from Test3 (total > c3*nSigma*rms)
    cpu-only     8 clusters  ->  ALL from the local-max gate (value == max)
    Test1 threshold          ->  contributes ZERO clusters

Test1 and the local-max gate are protected by construction: any pixel above
nSigma*rms is never updated inside the frame (it is a centre or in shadow, and
neither branch pushes), so the stencil argmax is always pristine. Test3 sums all
nine values, and the four already-scanned neighbours -- three above, one left --
are exactly the pixels eligible for update.

Confirmed two ways: instrumented (branch codes diffed per frame) and ablated
(Test3 compiled out, at which point the 11 go to zero exactly). A7 has both.

Test1 flips MORE often than Test3 -- 619 of 974 divergent pixels -- and costs
nothing, because it only moves a pixel between QUIET_UPDATE and SHADOW and
neither of those stores. It is not merely downstream of Test3 either: with Test3
ablated the first divergence is still a Test1 flip, on a frame where the two
pedestals were provably identical. Test1 initiates on its own, just ~7x slower
(frame 30 vs frame 4), and can never create a cluster by itself.

Row 4 is a different mechanism entirely -- f32 EMA drift, not timing, since both
finders freeze the pedestal there. That is the next slide.""")

# ================================================ 28 · THE MISMATCH, SEEN
s = new_slide()
chrome(s, 31, "Validation · the disagreement, seen",
       "The whole disagreement is one duplicate centre")
figure(s, "fig_mismatch147", 1.37, 1.72, 10.6)
callout(s, M, 6.30, 5.85,
        "Only the **3×3 footprints of each finder's own centres** are drawn, so the "
        "panels differ **exactly** where the finders do. Cell values are "
        "pedestal-subtracted ADU.", h=0.78, size=10.5)
callout(s, 6.75, 6.30, 5.85,
        "cuda's patch is **one row taller**: a second centre directly below the one "
        "both found. **The charge is already counted** — a duplicate, not a new "
        "photon.", h=0.78, size=10.5, color=AMBER)
notes(s, """Frame 147, the strongest of the six residuals, shipping f32 build.
Every other centre in the patch agrees, including the ordinary photon at bottom
right.

Each panel masks every pixel that is not inside the 3x3 footprint of one of THAT
finder's own cluster centres. So a pixel is visible on the left only if frozen
claimed it, and visible on the right only if cuda did. Where the panels look
identical, the finders agreed pixel for pixel.

The red dots are cluster centres. The amber ring is the one centre cuda keeps
that frozen does not: (202, 8), directly below (202, 7) which both finders keep.
Because the two 3x3 windows overlap, cuda's patch is four rows tall where
frozen's is three - that visible extra row IS the disagreement.

Note what is NOT happening: there is no cluster in one panel that is absent from
the other's neighbourhood entirely. Nothing was missed, and nothing was invented
out of noise. The two finders are arguing about which of two adjacent pixels owns
a photon they both found.

Same construction as helper.plot_masked_mismatch() in the notebook, so this is
reproducible from ClusterFinderFrozen_vs_CUDA.ipynb directly.""")

# ========================================================== 27 · RESIDUALS
s = new_slide()
chrome(s, 32, "Validation · the six residuals, dissected",
       "float32 cannot tell these two pixels apart")
code(s, M, 1.88, 6.35, [
    "frame 147    centre (x=202, y=8)      3×3 window",
    "raw window (ADU)     pedestal-subtracted, 1 decimal",
    "[[4646 5282 4703]    [[ 45.3 «638.4» -12.1]   frozen and cuda",
    " [4857 5318 4950]     [ 43.7 «638.4»  -7.5]   print the SAME",
    " [4763 4640 4858]]    [  1.1  70.7  136.4]]   window",
    "",
    "the two contenders, at full precision:",
    "                          rival (dy=-1)        centre",
    "frozen  [f64 ped]      638.383019956   638.382773664",
    "cuda    [f32 ped]      638.382812500   «638.382812500»",
    "",
    "gate:  accept if  centre >= max(window)",
    "   frozen   638.382773664 >= 638.383019956  ->  reject",
    "   cuda     638.382812500 >= 638.382812500  ->  «ACCEPT»",
], size=8, title="THE ONLY TEST THAT FLIPS, AND WHY")
callout(s, M, 4.72, 6.35,
        "Separation under the f64 pedestal: **0.000246 ADU**. One float32 ULP at "
        "4 679.6 ADU is **0.000488**. The two pixels are **half a ULP apart**; in "
        "float32 they are *the same number*, and the gate accepts on a tie.",
        h=1.00, size=10)
figure(s, "fig_spectra_valid", 7.30, 1.92, 5.40)
callout(s, M, 5.84, 11.9,
        "**With the f64 pedestal there are none at all.** In the shipping f32 build "
        "each of the six sits **one pixel from a cluster both finders found**, a "
        "duplicate neighbour, never a spurious photon and never a missed one.",
        h=0.76, size=10.5, color=AMBER)
caption(s, M, 6.70, 11.9,
        "The same frame 147 as the previous slide, recomputed under each finder's "
        "decision-time pedestal. Both finders run the same gate (CPU value == max, CUDA "
        "!(val < max)), so the tie separates them, not the expression.")
notes(s, """Frame 147, centre (x=202, y=8). The full numbers.

                              centre               rival      centre - rival
  frozen (f64 host ped)  638.382773664       638.383019956        -0.000246291
  cuda   (f32 dev  ped)  638.382812500       638.382812500         0.000000000

  pedestal mean, centre:  f64 4679.617226336   f32 4679.617187500
  pedestal mean, rival :  f64 4643.616980044   f32 4643.617187500

  1 float32 ULP at 4679.6 ADU = 0.000488 ADU
  the f64 separation between the two pixels = 0.50 ULP

Read that last line slowly: the two pixels differ by HALF a float32 ULP. There is
no float32 number between them, so once the pedestal is stored as float32 they
round to the identical bit pattern. The local-max gate is `value >= max`, which
accepts on equality - so CUDA keeps the centre and frozen, which can still see
the rival is 2.5e-4 ADU higher, rejects it.

This is not drift in the usual sense. It is not that the f32 EMA wandered away
from the f64 one over thousands of updates - look at the pedestal columns, they
agree to 4e-5 and 2e-4 ADU. It is that the QUESTION being asked ("which of these
two pixels is larger?") has an answer that float32 cannot represent.

Consequences worth stating out loud if asked:
  - It is one-directional. cuda-only 6, frozen-only 0. A tie can only ever ADD a
    centre, never remove one, because >= accepts.
  - All six are at Chebyshev distance 1 from a cluster both finders found: a
    duplicate neighbour, never a spurious photon.
  - With DEVICE_PED_TYPE = double the residual is 0 / 0 at both 3x3 and 9x9.
  - Fixing it in f32 would mean a strict > in the gate, which changes the CPU's
    documented behaviour on genuine ties. Not worth 6 clusters in 23 million.""")

# =========================================================== 28 · API 1
s = new_slide()
chrome(s, 33, "For users · Python API", "The fast path in eight lines")
code(s, M, 1.95, 7.6, [
    "from aare import File, ClusterFinderCUDA",
    "",
    "cf = ClusterFinderCUDA(image_size=(400, 400), cluster_size=(3, 3),",
    "                       n_sigma=5, «n_streams»=4,",
    "                       «max_clusters_per_frame»=3000)",
    "",
    "for _ in range(1000):                    # 1. train the pedestal",
    "    cf.push_pedestal_frame(pd.read_frame())",
    "",
    "data = f.read_n(100_000)                 # 2. one contiguous array",
    "cf.«register_input_buffer»(data)           # 3. pin it once",
    "",
    "for s in range(0, N, 2000):              # 4. batch through it",
    "    clusters = cf.«find_clusters_batched»(data[s:s+2000], first_frame=s)",
    "",
    "cf.unregister_input_buffer()             # 5. release the pages",
], size=9, title="THE RECOMMENDED PATTERN")
bullets(s, 8.6, 2.0, 4.1, [
    "find_clusters_batched returns **one ClusterVector per frame**, in order, and "
    "does opt5's chunked overlap internally, so you get it for free.",
    "register_input_buffer is what turns opt3 into opt4: **one call**.",
], size=10)
code(s, 8.6, 3.55, 4.1, [
    "# opt6: never copy",
    "for v in cf.«find_cluster_views_",
    "          batched_iter»(data, 2000):",
    "    hist.fill(v.sums())",
    "    # views die with the chunk",
], size=8, title="IF YOU REDUCE AS YOU GO")
callout(s, 8.6, 5.25, 4.1,
        "**×1.16 at 3×3, ×2.21 at 9×9.** The views expose every cluster; they only "
        "withhold ownership past the chunk.", h=0.95, size=10)
callout(s, 8.6, 6.35, 4.1,
        "Pin **once**, outside the loop. Slices of a registered array inherit the "
        "pinning.", h=0.72, size=10, color=AMBER)

# =========================================================== 27 · API 2
s = new_slide()
chrome(s, 34, "For users · choosing the knobs",
       "Five knobs, and the one that silently truncates")
hdr = [("Parameter", 1.05), ("What it does", 3.6), ("Guidance", 5.2)]
y = 2.0
rect(s, M, y, 11.9, 0.4, PANEL)
for lab, dx in hdr:
    tf = tb(s, M + dx - 0.85 if dx > 1.05 else M + 0.28, y + 0.09, 5.0, 0.3)
    run(para(tf, True), lab.upper(), 9, MUTED, bold=True, spc=1.3)
y += 0.44
params = [
    ("n_streams", "How many frames may be in flight at once: an upper bound, not a "
     "count: the copy engines cap the real number below it.",
     "4 at both sizes. 8 buys no kernel concurrency at 9×9 (+1% instance time) and "
     "inflates the CUDA-event timer 3.5×."),
    ("max_clusters_per_frame", "Fixed size of the per-frame D2H transfer.",
     "Must exceed the real maximum or clusters are silently dropped, and it sets the "
     "D2H bar directly. Measured at 9×9: the maximum is 1 633, and the lossless "
     "cap of 1 700 already makes D2H the bottleneck on the f32 build."),
    ("batch size", "Frames per find_clusters_batched call.",
     "2 000 amortises launch overhead without a large pinned footprint."),
    ("cluster_size", "Compile-time stencil geometry.",
     "3×3 and 9×9 are registered; 9×9 moves the bottleneck off H2D and onto the "
     "kernel on f64, and onto D2H once opt7 shortens it."),
    ("register_input_buffer", "Page-locks the host array for DMA.",
     "Always, if the data is already in RAM. Check the pinning budget first."),
]
flagged = None
for i, (p_, what, guide) in enumerate(params):
    if i % 2 == 0:
        rect(s, M, y, 11.9, 0.82, PANEL)
    tf = tb(s, M + 0.28, y + 0.14, 2.6, 0.5)
    run(para(tf, True, line=1.1), p_, 9.5, ACCENT, font=MONO, bold=True)
    tf = tb(s, M + 3.0, y + 0.14, 2.9, 0.6)
    run(para(tf, True, line=1.2), what, 9.5, PALE)
    tf = tb(s, M + 6.15, y + 0.14, 5.4, 0.6)
    run(para(tf, True, line=1.2), guide, 9.5, TEXT2)
    if p_ == "max_clusters_per_frame":
        flagged = y
    y += 0.80
# The one row that loses data if you get it wrong, ringed so it is found without
# reading the table. Drawn last so the outline sits over the zebra fill.
frame_rect(s, M - 0.04, flagged - 0.05, 11.98, 0.92)
# The ring and the warning are the same point, so they carry the same colour.
callout(s, M, 6.55, 11.2,
        "The single most common mistake: leaving **max_clusters_per_frame** too low. "
        "It does not error; it truncates, and every frame quietly returns the same count.",
        h=0.66, size=10, color=RED)

# =========================================================== 28 · NEXT
s = new_slide()
chrome(s, 35, "Where this leaves us",
       "The bottleneck has walked from the host, to the GPU, to the wire")
cards = [
    ("DONE", ACCENT, "×9.1 at 3×3, ×26.5 at 9×9",
     "16.3 and 25.1 µs/frame end to end, both sitting on their hardware floor. "
     "At 3×3 that is 58 495 FPS against MÖNCH03's 1.3 kHz standard frame rate: "
     "45× the detector, and ~10× its optimised 3–6 kHz ceiling."),
    ("DONE", ACCENT, "FP32 pedestal, safely",
     "−40.5 % kernel, and correct, because the variance is accumulated on a frozen "
     "per-pixel offset instead of a raw second moment. Against the CPU twin that "
     "isolates the port the decisions are identical; the shipped f32 pedestal adds "
     "6 duplicates in 23 million."),
    ("NEXT", AMBER, "3×3: transfer granularity",
     "The 16.31 µs sustained sits 3.16 µs above the uncontended 13.15 µs H2D rate "
     "[f32: s4 vs s1]: "
     "2 000 separate 320 kB descriptors, plus 26% of H2D↔D2H contention."),
    ("NEXT", AMBER, "9×9: the result path, not the kernel",
     "At a lossless cap D2H already binds [f32 · s4]: 25.24 µs against a 23.94 µs "
     "kernel. More "
     "kernel work buys nothing until the D2H slot stops being cap-sized."),
]
for i, (tag, col, title, body) in enumerate(cards):
    cx = M + (i % 2) * 6.05
    cy = 2.05 + (i // 2) * 2.35
    rect(s, cx, cy, 5.85, 2.05, PANEL)
    rect(s, cx, cy, 5.85, 0.035, col)
    tf = tb(s, cx + 0.3, cy + 0.26, 1.4, 0.26)
    run(para(tf, True), tag, 8.5, col, bold=True, spc=1.5)
    tf = tb(s, cx + 0.3, cy + 0.60, 5.2, 0.4)
    run(para(tf, True, line=1.1), title, 14, PALE, bold=True)
    tf = tb(s, cx + 0.3, cy + 1.12, 5.2, 0.85)
    run(para(tf, True, line=1.3), body, 10, TEXT2)
callout(s, M, 6.58, 11.2,
        "Full numbers, methodology and reproduction steps: **docs/ClusterFinderCUDA_benchmark_results.md** · "
        "notebook **python/tests/ClusterFinderCUDA_perf.ipynb** · measurement campaign **python/tests/perf/**",
        h=0.60, size=10)
notes(s, """The headroom claim, stated carefully.

MOENCH03 runs at 1.3 kHz as standard and 3-6 kHz with optimised readout boards.
The finder does 58 495 FPS at 3x3 and 39 775 at 9x9, so it is 45x and 31x the
standard rate, and about 10x and 7x the optimised ceiling. The point is not the
multiple: it is that cluster finding has stopped being the thing that decides
how fast you can take data, on one GPU, at either cluster size.

Two honest qualifications. First, this is throughput on frames already in host
RAM -- getting them there from the detector is a separate problem and is not
measured here. Second, at 9x9 the D2H slot is cap-sized, so the margin shrinks
if max_clusters_per_frame has to grow for a busier beam.

What the 24-thread CPU finder does, for contrast: 6 762 FPS at 3x3, which
matches the standard mode with nothing to spare, and 1 503 FPS at 9x9, which
does not.""")

# --------------------------------------------------------- divider · ANNEX
section("",
        "Annexes",
        "",
        [("A1", "Every engine number, reconciled"),
         ("A2", "The three rejected routes"),
         ("A3", "opt5 · the overlap code"),
         ("A4", "The fault model, tested"),
         ("A5", "The variance rewrite in full"),
         ("A6", "The three benchmark artefacts"),
         ("A7", "Which test causes the CPU/CPU gap")],
        rng=(1, N_ANNEX), col=AMBER, annex=True,
        carry=("Everything so far", "35 slides",
               "the arc is finished; what follows answers questions"))

# ===========================================================================
# ANNEX — the measurement detail behind slides 26–27, and the rejected routes
# ===========================================================================

# ---- A1 · THE CONVENTION -------------------------------------------------
s = new_slide()
annex_chrome(s, 1, "measurement convention · expands slide 20",
             "Uncontended, or as the pipeline runs it")
bullets(s, M, 1.90, 12.0, [
    "Every engine time in this deck is tagged **[build · s1|s4]**. **s1** is one "
    "stream with nothing else running: what an engine does **on its own**, which is "
    "the right number for a capability claim and for the headroom that remains. "
    "**s4** is the shipped four-stream pipeline: each engine's **busy time per "
    "frame**, the union of its intervals, the only number that can set a floor.",
], size=10.5)
table(s, M, 2.78, 5.8,
      ["3×3 · µs/frame", "s1 f64", "s1 f32", "s4 f64", "s4 f32"],
      [["H2D", "13.14", "13.15", "16.17", "16.63"],
       ["kernel", "14.72", "4.32", "15.17", "5.53"],
       ["D2H", "5.31", "5.27", "7.69", "7.57"],
       ["engine max [s4]", "—", "—", "16.17", "16.63"],
       ["FLOOR = lower of max, sustained", "—", "—", "**16.17**", "**16.31**"]],
      colw=[0.28, 0.18, 0.18, 0.18, 0.18], size=9, rowh=0.44)
table(s, 7.0, 2.78, 5.8,
      ["9×9 · cap 1700", "s1 f64", "s1 f32", "s4 f64", "s4 f32"],
      [["H2D", "13.20", "13.22", "20.77", "20.54"],
       ["kernel", "39.86", "23.70", "32.66", "23.94"],
       ["D2H", "21.97", "21.95", "25.25", "25.24"],
       ["engine max [s4]", "—", "—", "32.66", "25.24"],
       ["FLOOR = lower of max, sustained", "—", "—", "**30.01**", "**25.14**"]],
      colw=[0.28, 0.18, 0.18, 0.18, 0.18], size=9, rowh=0.44)
callout(s, M, 5.66, 12.0,
        "**Two traps this table closes.** (1) s4 is engine *occupancy*, not duration: "
        "the 9×9 kernel row **falls** 39.86 → 32.66 while every transfer rises, and "
        "nothing that measures a duration falls under load. (2) **the FLOOR is not the "
        "engine max**: the max is profiled and runs 2–8 % high, so the floor is whichever is "
        "lower, it or the best unprofiled sustained rate, which is why opt6 reports "
        "**30.01 µs against a 32.66 max**.", h=0.92, size=10.5)
caption(s, M, 6.62, 12.0,
        "THE D2H SHIFT IS AN s4 PHENOMENON: at s1 the kernel binds in both arms "
        "(39.86 and 23.70 against a 21.97 / 21.95 D2H); only under four-stream "
        "contention does D2H climb to 25.24 and overtake the 23.94 f32 kernel, so any "
        "claim about which engine binds must be read from the s4 columns. Source: "
        "probes.csv in perf/results/2026-08-18_{f64,f32}/ (3×3) and "
        "2026-08-20_{f64,f32}_capAB/ (9×9). Bandwidth arithmetic in the notes.")
notes(s, """Neither direction is faster than the other, and the bar heights say so.

At s1, H2D moves 320 000 B in 13.15 us = 24.3 GB/s; D2H moves 120 004 B in
5.27 us = 22.8 GB/s; at 9x9, D2H moves 557 604 B in 21.95 us = 25.4 GB/s. All
three sit at 72-81 % of PCIe 4.0 x16's 31.5 GB/s, the smallest transfer paying
the most fixed cost per byte. So a taller bar in this deck always means MORE
BYTES, never a slower wire -- which is exactly why raising the cap to 1 700
hands the 9x9 floor to D2H: it is 544.5 kB per frame either way the arm is
built, and it does not care about the typedef.

On the last row: the floor is the LOWER of the s4 estimate and the best
unprofiled sustained rate. 16.31 vs 16.63 at 3x3 f32; 25.14 vs 25.24 at 9x9 f32;
30.01 vs 32.66 at 9x9 f64, which is the widest gap and the reason the rule
exists. A probe roofline is an estimate, never a hard denominator.""")

# ---- A2 · ROUTE A · IDEA -------------------------------------------------
s = new_slide()
annex_chrome(s, 2, "rejected route · CUDA graphs · expands slide 11",
             "CUDA Graphs, a sound idea that the next act overtook", part=1, nparts=3)
bullets(s, M, 1.95, 12.0, [
    "Every cudaMemcpyAsync / kernel launch costs the **CPU** a few microseconds of "
    "driver work, per frame and per operation. After opt4 that looked like the budget.",
    "A **CUDA Graph** captures the whole dependency DAG once. Replaying it is a "
    "**single** cudaGraphLaunch: the driver already knows every node and edge.",
], size=10.5)
figure(s, "fig_graphs", M, 3.15, 7.6)
code(s, 8.5, 3.15, 4.1, [
    "// record once, at setup",
    "cudaStreamBeginCapture(sc.stream, ...);",
    "   submit_h2d_kernel_d2h(sc);",
    "cudaStreamEndCapture(sc.stream, &sc.graph);",
    "«cudaGraphInstantiate»(&sc.graphExec, ...);",
    "",
    "// per batch: one call",
    "«cudaGraphLaunch»(sc.graphExec, sc.stream);",
], size=8, title="ClusterFinderCUDA_graph.hpp")
callout(s, 8.5, 5.06, 4.1,
        "**REJECTED**\n3×3: 39 752 FPS, inside noise of opt4.\n9×9: **11 072 FPS, 12 % slower**.",
        h=1.10, size=10.5, color=AMBER)
caption(s, M, 6.62, 12.0,
        "Its 3×3 edge was never established: the graph finder recorded no CUDA events "
        "while the stream finder did, and that instrumentation tax (2.8 µs) is larger "
        "than the gap (0.8 µs). More decisively, it never received the chunked pipeline "
        "of opt5, so it is competing on ~2 µs of launch cost against a 24 µs floor. "
        "Launch overhead stops binding one step later; the technique aimed at it can no "
        "longer pay.")

# ---- A3 · ROUTE A · BUDGET -----------------------------------------------
s = new_slide()
annex_chrome(s, 2, "rejected route · CUDA graphs",
             "What a CUDA Graph actually saves, in microseconds", part=2, nparts=3)
bullets(s, M, 1.90, COL, [
    "The stream path issues **four runtime calls per frame**, a memset to clear the "
    "cluster counter, H2D, the launch, D2H. A graph replaces all four with **one** "
    "cudaGraphLaunch, so the ceiling on what it can save is **¾ of the submission cost**.",
], size=10.5)
table(s, M, 2.72, COL,
      ["call", "per frame", "host cost", "µs/frame"],
      [["cudaMemcpyAsync", "2", "1.98 µs", "3.97"],
       ["cudaLaunchKernel", "1", "2.13 µs", "2.13"],
       ["cudaMemsetAsync", "1", "1.57 µs", "1.57"],
       ["**submission total**", "**4**", "", "**7.67**"]],
      colw=[0.40, 0.18, 0.22, 0.20], size=9, rowh=0.44)
code(s, M, 5.42, COL, [
    "7.67 us/frame x 3/4  = 5.75 us/frame   eliminated, AS MEASURED (under nsys)",
    "5.75 / 4 (see A5)    ~ 1.4 us/frame    eliminated, unprofiled estimate",
], size=8, title="THE ARITHMETIC")
callout(s, M, 6.38, COL,
        "**~1.4 µs against a 16.17 µs floor = 8.7 %**, real while the host is the "
        "critical path, and **worth nothing after opt5**, which hides host work under "
        "the GPU entirely.", h=0.80, size=10, color=AMBER)
rail(s, [
    ("label", "route A · measured verdict"),
    ("gap", 0.10),
    ("stat", "3×3 vs opt4", "×1.03", TEXT2),
    ("stat", "9×9 vs opt4", "×0.88", AMBER),
    ("gap", 0.05),
    ("row", "vs opt5 at 9×9", "36 % behind", AMBER),
    ("gap", 0.20),
    ("note", "Read from CUPTI_..._RUNTIME, the host table, so this is the one "
             "order-of-magnitude number in the deck, ±50 %. It is enough to show "
             "graphs cannot pay against a 16 µs floor, and not enough to quote to "
             "two figures."),
])


# ---- A2·3 · ACT II REJECTED ROUTES ---------------------------------------
# Was a main-arc slide. It answers "did you try just making the copy faster?",
# which is a question, not a step in the argument, so it belongs here.
s = new_slide()
annex_chrome(s, 2, "rejected routes · the result copy · expands slide 20",
             "The copy is allocation-bound, not bandwidth-bound", part=3, nparts=3)
rows = [
    ("B\u2032", "One allocation per chunk", "collect_packed()",
     "Removes the per-frame malloc but keeps the copy, and the copy is ~80% of the "
     "cost. Worse, the replacement allocation is 1.17 GB, far above glibc's mmap "
     "threshold, so it is mmap'd and munmap'd every chunk: 606 566 faults, ~21 \u00b5s/frame.",
     "69.3 \u00b5s  \u00b7  deleted from the API"),
    ("B\u2033", "Parallel materialisation", "8-thread copy pool",
     "Each worker gets its own glibc arena, which destroys the cross-run heap reuse "
     "that makes the single-threaded path cheap. Faults went 9 700 \u2192 2 270 000; "
     "MALLOC_ARENA_MAX=1 collapsed them back to 138 k, which is the proof.",
     "+6% at best, \u221233% when results are freed promptly"),
]
y = 2.00
for tag, name, how, body, verdict in rows:
    rect(s, M, y, 11.9, 2.05, PANEL)
    rect(s, M, y, 0.035, 2.05, AMBER)
    tf = tb(s, M + 0.30, y + 0.22, 1.0, 0.4)
    run(para(tf, True), tag, 20, AMBER, bold=True, font=MONO)
    tf = tb(s, M + 1.30, y + 0.20, 6.0, 0.3)
    run(para(tf, True), name, 14, PALE, bold=True)
    tf = tb(s, M + 1.30, y + 0.52, 6.0, 0.3)
    run(para(tf, True), how, 10, MUTED, font=MONO)
    tf = tb(s, M + 1.30, y + 0.90, 10.2, 1.0)
    run(para(tf, True, line=1.3), body, 10, TEXT2)
    tf = tb(s, M + 1.30, y + 1.68, 10.2, 0.3)
    run(para(tf, True), verdict, 10.5, AMBER, bold=True)
    y += 2.25
callout(s, M, 6.38, 11.9,
        "**Copying faster does not help when the cost is the OS populating pages.** "
        "The only winning move is not to allocate — which is exactly what opt6 does. "
        "materialize_slot() is deliberately single-threaded and carries a comment "
        "saying so, to stop the experiment being repeated.", h=0.80, size=10.5)


# ---- A4 · THE OPT5 CODE --------------------------------------------------
s = new_slide()
annex_chrome(s, 3, "opt5 · the overlap code · expands slide 19",
             "The overlap, in six lines, and why you never write them")
code(s, M, 1.86, 7.15, [
    "tok = cf.«submit_batch»(data[a0:b0], first_frame=a0)",
    "for a, b in bounds[1:]:",
    "    nxt = cf.«submit_batch»(data[a:b], first_frame=a)   # GPU starts N+1 …",
    "    results.extend(cf.«collect»(tok))                   # … host unpacks N",
    "    tok = nxt",
    "results.extend(cf.«collect»(tok))                       # drain the last one",
], size=8.5, title="THE WHOLE OF OPT5")
bullets(s, M, 3.66, 7.15, [
    "**You do not write this.** It is inside find_clusters_batched(), which chunks "
    "the batch and runs the loop for you, opt5 arrived as a **speedup, not an API "
    "change**, and every existing caller got it without editing a line.",
    "submit_batch() and collect() stay public for anyone who wants the token by "
    "hand, streaming from a detector, interleaving other work between chunks.",
], size=10)
rail(s, [
    ("label", "chunk sizing · the two constraints"),
    ("gap", 0.12),
    ("row", "multiple of", "n_streams", ACCENT),
    ("row", "capped at", "MAX_SLOT_BYTES", ACCENT),
    ("gap", 0.16),
    ("note", "The chunk MUST be a multiple of n_streams. The device pedestal is "
             "per-stream and advances once per frame the stream sees, so an uneven "
             "chunk leaves the four pedestals at different ages, the finder would "
             "stop being reproducible, and two runs of the same data would not agree."),
    ("gap", 0.14),
    ("note", "It is also capped so the two pinned output slots stay bounded: the "
             "slot is chunk × (4 + cap × sizeof(Cluster)), which at 9×9 and cap 1 700 "
             "is 544.5 kB per frame. chunk_size_for(n) applies both rules; pass the "
             "result to reserve_output_slots() to pre-pay the page-locking outside "
             "any timed region."),
])
callout(s, M, 5.92, 7.15,
        "Two chunks in flight is enough. A third adds pinned memory and no overlap: "
        "the host is already busy for the whole time the GPU is.", h=0.72, size=10)
caption(s, M, 6.80, 12.0,
        "The saving is min(GPU, host) per chunk, so opt5 pays most when the two terms "
        "are comparable, ×1.31 at 3×3, and least when one dominates, ×1.20 at 9×9, "
        "where the host term is roughly twice the GPU term. That gap is the diagnosis "
        "that motivates opt6 (report §8.2).")

# ---- A5 · THE FAULT MODEL ------------------------------------------------
s = new_slide()
annex_chrome(s, 4, "the fault model · expands slide 23",
             "The fault model, tested against every step")
bullets(s, M, 1.90, 12.0, [
    "Slide 26 fits **0.68 µs per first-touch fault** on the **3×3 f32** ladder, where "
    "wall = steady-state + faults × 0.68 µs reproduced a 6.110 s run to **1 ms**. "
    "Every row below is **9×9, and an f64-vs-f32 gap**: a different cluster size and a "
    "different comparison, so the rate is applied **out of sample**, never refitted.",
], size=10.5)
table(s, M, 2.78, 12.0,
      ["step", "f64 warm (faults)", "f32 warm (faults)", "Δ wall",
       "Δ faults", "predicted", "verdict"],
      [["opt3", "82.44 µs  (128 k)", "95.66 µs  (521 k)", "**+13.22**", "+393 k",
        "**+13.37**", "**the allocator**"],
       ["opt4", "79.83 µs  (128 k)", "75.20 µs  (127 k)", "−4.63", "−0.5 k",
        "−0.02", "clean"],
       ["opt5", "66.39 µs  (152 k)", "61.85 µs  (10 k)", "−4.54", "−141 k",
        "**−4.81**", "**not separable**"],
       ["opt6", "30.01 µs  (0)", "25.14 µs  (0)", "−4.87", "0", "0.00", "clean"]],
      colw=[0.08, 0.18, 0.18, 0.10, 0.10, 0.13, 0.23], size=8.5, rowh=0.50)
callout(s, M, 5.42, 12.0,
        "**opt3 is the whole argument in one row.** A 393 k fault gap predicts +13.37 µs; "
        "+13.22 was observed, agreement to **1 %**. The −40 % kernel is in there "
        "somewhere, invisible under 13 µs of the OS zeroing pages. Only opt4 and opt6, "
        "where the fault term is ~0, report the typedef at all.", h=0.90, size=10.5)
caption(s, M, 6.52, 12.0,
        "ladder_9x9.csv in results/2026-08-20_{f64,f32}_cap1700/, warm = best of reps "
        "1–4, faults are that rep's own getrusage minor-fault count. Predicted = Δfaults "
        "× 0.68 µs ÷ 20 000 frames, with 0.68 carried in unchanged from the 3×3 fit "
        "(slide 27): nothing on this slide is tuned to make the columns agree. Observed "
        "vs predicted: +13.22 / +13.37, −4.63 / −0.02, −4.54 / −4.81, −4.87 / 0.00. "
        "This table replaces an earlier figure that quoted opt3's +16 % as a measurement "
        "of the result path; it is a measurement of two allocator states.")

# ---- A5 · THE VARIANCE REWRITE IN FULL -----------------------------------
# Was main-arc slide 23, plus the error-floor panel that used to share
# fig_cancellation. Both are the quantitative backing for slide 22's third
# bullet, and neither is needed to follow the argument.
s = new_slide()
annex_chrome(s, 5, "the variance rewrite · expands slide 22",
             "The rewrite in full, and which pixels the error reached")
bullets(s, M, 1.90, 7.4, [
    "Freeze a per-pixel baseline **X₀ = round(mean)** once, at the end of pedestal "
    "training, and never move it again.",
    "Accumulate the **centred** value Y = X − X₀ instead of X, and report the mean "
    "as **X₀ + sum/n**, so nothing downstream changes.",
    "Both operands of the subtraction are now **O(rms)-sized**: the huge common "
    "term is gone before the subtraction rather than after it.",
], size=10.5)
code(s, M, 3.62, 7.4, [
    "// before: both terms ~2.17e7, answer ~2000",
    "var = sum2/n - mean*mean;",
    "",
    "// after: centred on a frozen per-pixel offset X0",
    "DEVICE_PED_TYPE resid  = mean - «d_pd_off»[i];      // ~O(1)",
    "DEVICE_PED_TYPE var_px = sum2[i]/n - resid*resid;  // no cancellation",
], size=9, title="clusterfinder_kernel.cuh")
callout(s, M, 5.42, 7.4,
        "Result: the 100 % FP32 build matches the FP64 build to **3 × 10⁻⁷**, "
        "70 clusters out of 233 million.", h=0.72, size=10.5)
callout(s, M, 6.30, 7.4,
        "**X₀ must never be updated.** The accumulators are defined relative to it, "
        "so moving it invalidates every sum already collected. Welford's online "
        "variance is the other correct answer; this one was chosen because it is "
        "two lines and does not change the update's arithmetic cost.",
        h=0.86, size=10.5, color=AMBER)
h = figure(s, "fig_varfloor", 8.30, 2.00, 4.32)
caption(s, 8.30, 2.00 + h + 0.18, 4.32,
        "Which pixels the ±3 ADU² floor actually reaches. Below rms ≈ 2 the variance "
        "is lost outright; from 2 to 5 the threshold is corrupted but not clamped. "
        "Source: §5–§6 of docs/pedestal_precision_f32_cancellation.md. The naive f32 "
        "build's +28.06 % excess is this shaded band, integrated over the sensor.",
        size=9)

# ---- A6·1 · THE THREE ARTEFACTS ------------------------------------------
# Main-arc slide 27 keeps only the first of these three, because it is the only
# one that moves a number the audience is shown. This is that slide as it stood.
s = new_slide()
annex_chrome(s, 6, "benchmark artefacts · expands slide 27",
             "Three ways a GPU benchmark lies", part=1, nparts=4)
items = [
    ("First-touch page faults", AMBER,
     "Each run materialises ~10 GB of clusters. The first pass faults in ~2.6 M "
     "pages at 0.7 µs each, up to 4 s of pure OS work inside the timer.",
     "Fix: re-run until getrusage() minor faults plateau (< 200 k)."),
    ("CUDA-event kernel timing", AMBER,
     "avg_kernel_time_ms() measures elapsed time on a stream, including waiting "
     "for other streams. Under 8-stream load it over-reads by up to 3.5×.",
     "Fix: Nsight Systems per-instance times; 1 stream for exclusive numbers."),
    ("The profiler itself", AMBER,
     "Under nsys, wall time per frame inflates ~4× from API tracing.",
     "Fix: GPU op times from nsys, wall times from unprofiled runs."),
]
x = M
for title, col, body, fix in items:
    rect(s, x, 2.0, 3.83, 3.15, PANEL)
    rect(s, x, 2.0, 3.83, 0.035, col)
    tf = tb(s, x + 0.26, 2.28, 3.3, 0.6)
    run(para(tf, True, line=1.15), title, 13, PALE, bold=True)
    tf = tb(s, x + 0.26, 3.02, 3.3, 1.5)
    run(para(tf, True, line=1.3), body, 10, TEXT2)
    tf = tb(s, x + 0.26, 4.42, 3.3, 0.65)
    run(para(tf, True, line=1.25), fix, 10, ACCENT)
    x += 4.03
code(s, M, 5.4, 11.9, [
    "# every timed cell in the benchmark notebook is bracketed with:",
    "mf0 = resource.getrusage(resource.RUSAGE_SELF).ru_minflt",
    "...   t = time.perf_counter() - t0   ...",
    "print(f'minor faults: {mf1-mf0:,}')   # quote the run where this plateaus",
], size=9, title="THE FAULT PROTOCOL · python/tests/ClusterFinderCUDA_perf.ipynb")
callout(s, M, 6.68, 11.9,
        "Validated: **wall = steady-state + faults × 0.68 µs** reproduced a 6.110 s "
        "run to within **1 ms**. Kernel time stayed constant throughout; the GPU was "
        "never the variable.", h=0.62, size=10)

# ---- A6 · FAULTS ---------------------------------------------------------
s = new_slide()
annex_chrome(s, 6, "benchmark artefacts · expands slide 27",
             "First-touch page faults: two sources, one counter", part=2, nparts=4)
bullets(s, M, 1.90, 12.0, [
    "A page exists in the process's address space but has no physical frame yet. "
    "On first touch the kernel finds one, **zeroes it** (mandatory), and maps it. "
    "No disk I/O: ru_majflt stays 0 all campaign. At 4 kB/page, **1 GB = 262 144 faults**.",
], size=10.5)
table(s, M, 2.62, 12.0,
      ["", "(a) result heap", "(b) pinned D2H slots"],
      [["allocator", "malloc → mmap, one ClusterVector per frame",
        "cudaMallocHost, in submit_batch"],
       ["cost / page", "**0.7 µs**", "**1.0 µs**: same fault + pin + DMA map"],
       ["recurs?", "**yes**, every alloc/free cycle above the mmap threshold",
        "**no**: once per buffer, for its lifetime"],
       ["removed by", "**collect_view()**: it allocates nothing",
        "nothing; reserve_output_slots() only moves it out of the timer"]],
      colw=[0.14, 0.43, 0.43], size=9)
callout(s, M, 5.62, 5.85,
        "**The two are exactly additive.** Reserving subtracts precisely the pre-pin "
        "count from run 0 and changes nothing else.", h=0.72, size=10)
code(s, 6.75, 5.42, 5.85, [
    "3x3:   572 292 - 455 129 = 117 163   vs 117 192 pre-pin",
    "9x9: 2 759 037 - 2 278 567 = 480 470   vs 480 474",
    "closed form: 2 slots x 2000 x 120 004 B / 4 kB = 117 191",
], size=7.5, title="NOT A CORRELATION, AN IDENTITY")
callout(s, M, 6.50, 11.9,
        "**At 9×9 the heap never plateaus.** ~9.3 GB per pass is above glibc's mmap "
        "threshold, so it is munmap'd and re-faulted every pass: ~292 k faults ≈ "
        "**10 µs/frame, permanently**. No number of re-runs removes it, which is an "
        "independent argument for opt6.", h=0.72, size=10, color=AMBER)

# ---- A7 · EVENTS ---------------------------------------------------------
s = new_slide()
annex_chrome(s, 6, "benchmark artefacts",
             "CUDA events measure the stream, not the kernel", part=3, nparts=4)
bullets(s, M, 1.90, COL, [
    "avg_kernel_time_ms() brackets the launch with **cudaEventRecord on the "
    "kernel's own stream**. What it returns is elapsed time on **that stream's "
    "timeline**, which includes time spent **queued behind other streams**.",
    "So it is honest at 1 stream and inflates under saturation: up to **3.5×** at "
    "8 streams. The tell is that the derived *PCIe + overhead* = wall/N − kernel_ms "
    "**goes negative**: kernels overlap, so wall/frame < kernel/frame.",
    "It is also not free: **~3.6 µs/frame**, i.e. 10–15 % of end-to-end throughput "
    "at 3×3, to produce a number that is unusable exactly when it matters.",
], size=10.5)
code(s, M, 4.75, COL, [
    "if (m_time_kernels)                       // OFF by default, all 3 finders",
    "    cudaEventRecord(start[slot][i], sc.stream);",
    "find_clusters_in_single_frame<<<grid, block, shmem, «sc.stream»>>>(...);",
    "if (m_time_kernels)",
    "    cudaEventRecord(stop[slot][i], sc.stream);   // <- queue-wait lands here",
], size=8, title="ClusterFinderCUDA.hpp")
callout(s, M, 6.18, COL,
        "The flag exists for **comparability**, not preference: with events on for one "
        "finder and off for another, the step between them absorbs the tax.",
        h=0.72, size=10)
rail(s, [
    ("label", "9×9 kernel · f64 · nsys"),
    ("gap", 0.10),
    ("stat", "s1 · duration", "39.86 µs", ACCENT),
    ("stat", "s4 · occupancy", "32.66 µs", AMBER),
    ("gap", 0.05),
    ("row", "overlap factor", "1.32×", TEXT2),
    ("gap", 0.20),
    ("note", "Same kernel. The s4 column is the union of kernel intervals per frame, "
             "not how long one kernel takes. Quote s1 for duration; s4 feeds the floor, "
             "subject to the sustained-rate rule on slide 20. Full grid: A1."),
])

# ---- A8 · NSYS -----------------------------------------------------------
s = new_slide()
annex_chrome(s, 6, "benchmark artefacts",
             "Where nsys is sound, and where it is not", part=4, nparts=4)
bullets(s, M, 1.90, 12.0, [
    "Tracing a **host** call means running a callback on entry and on exit, "
    "**inside the interval being measured**. GPU work is different: the hardware "
    "stamps its own start/end and the host reads those records **afterwards**, so "
    "nothing is injected into the execution path.",
], size=10.5)
table(s, M, 2.60, 12.0,
      ["measurement", "sqlite table", "9×9 [f64 · s1 · cap 1500]", "verdict"],
      [["cudaLaunchKernel: the host call", "CUPTI_..._RUNTIME", "1.85 µs",
        "**inflated ~4×**"],
       ["the kernel executing", "CUPTI_..._KERNEL", "39.93 µs", "sound to ~2 %"],
       ["cudaMemcpyAsync: the host call", "CUPTI_..._RUNTIME", "1.65 µs",
        "**inflated ~4×**"],
       ["the H2D / D2H transfer", "CUPTI_..._MEMCPY", "13.25 / 19.44 µs",
        "sound to ~2 %"]],
      colw=[0.36, 0.24, 0.22, 0.18], size=9, rowh=0.52)
callout(s, M, 5.30, 11.9,
        "**Same cudaMemcpyAsync, two numbers: 1.65 µs to ask for the copy, 13.25 µs "
        "for the copy to happen.** If the profiler must be present at the moment to "
        "measure it, it distorts it; if the hardware records it anyway and the "
        "profiler reads it later, it does not.", h=0.86, size=10.5)
caption(s, M, 6.40, 11.9,
        "Every headline number in this deck (kernel times, transfer times, duty cycles, "
        "overlap and every engine floor) comes from the _KERNEL and _MEMCPY tables (see "
        "gpu_span.py). Proof that side is sound: opt7 sustains 25.14 µs unprofiled "
        "against a 25.24 µs estimate measured under the profiler, 0.4 % apart, which "
        "a 4× distortion could not survive. ⚠ Cap 1500: the only such numbers left in "
        "the deck. Why, and what the shipped bar is, in the notes.")
notes(s, """Why this one slide is still cap 1500, and what not to read off it.

The RUNTIME column exists only inside the trace: it is the cost of the host call
itself, which does not depend on how large the output slot is. So there is
nothing to re-run -- a cap-1700 trace would report the same ~1.65 and ~1.85 us
for cudaMemcpyAsync and cudaLaunchKernel.

What you must NOT read off this slide is the 19.44 us D2H. That is the cap-1500
figure. At the shipped cap of 1 700 the same engine reads 21.95 us [s1] and
25.24 us [s4], and it is the s4 value that binds the f32 build. A1 has the
shipped grid.

The one number in the whole deck taken from _RUNTIME is the CUDA Graph launch
budget in A2, and it is quoted to a single significant figure for exactly this
reason: a ~4x inflated host-call time can support "launch cost is about 2 us,
against a 24 us floor", and nothing finer than that.""")

# ============================================ ANNEX A7 · WHICH TEST, MEASURED
# Two slides. Slide 4's code panel is titled "THE WHOLE ALGORITHM" but shows a
# two-test simplification -- Test3 is not in it. So part 1 has to put the third
# branch back before part 2 can blame it for anything.
s = new_slide()
annex_chrome(s, 7, "the three tests · completes slide 4", "There is a third test",
             part=1, nparts=2)
bullets(s, M, 1.80, 11.9, [
    "Slide 4 showed the decision with **two** tests, which is the right "
    "simplification for the arc. The finder has **three**, and the missing one "
    "is what the next slide is about.",
], size=10.5)
code(s, M, 2.48, 7.3, [
    "v = frame[i] - ped_mean[i];   rms = ped_rms[i]",
    "if (v < -nSigma*rms)           -> skip, no update",
    "m     = «max» over the 3x3 window",
    "total = «sum» over the 3x3 window",
    "if (m > «nSigma»*rms)              // TEST 1",
    "    if (v == m) -> emit cluster  //   local-max gate",
    "    else        -> shadow, no update",
    "else if (total > «c3*nSigma»*rms)   // TEST 3",
    "    if (v == m) -> emit cluster",
    "else            -> update pedestal",
    "",   # code() budgets 0.0174 in/pt but LibreOffice sets Consolas at ~0.0187,
          # so a 10-line panel clips its last descender. One blank line buys the
          # slack without touching the constant every other panel depends on.
], size=9, title="ClusterFinder.hpp:104-142 · all three branches")
rail(s, [
    ("label", "what each test asks"),
    ("gap", 0.12),
    ("row", "Test 1  ·  nSigma · rms", "is any ONE pixel bright?", ACCENT),
    ("gap", 0.12),
    ("row", "Test 3  ·  c3 · nSigma · rms", "is the WINDOW bright?", AMBER),
    ("gap", 0.12),
    ("row", "local-max gate", "is this pixel the peak?", PALE),
    ("gap", 0.18),
    ("note", "Test 1 reads the MAX of the window; Test 3 reads its SUM. That "
             "one difference is the whole of the next slide."),
], y0=2.15)
callout(s, M, 5.86, 11.9,
        "**c3 = √(3×3) = 3 is not a fudge — it falls out of variance addition.** "
        "For independent samples the variance of a sum is the sum of the "
        "variances, so a 3×3 window of pixels each carrying noise σ gives "
        "**Var(Σ v) = 9σ²**, i.e. a sum whose noise is **√9 · σ = 3σ**. Requiring "
        "that sum to clear nSigma of ITS OWN noise is exactly **c3·nSigma·rms** — "
        "the **same 5σ criterion as Test 1, asked of the window instead of the "
        "pixel**. So Test 3 catches a photon whose charge is shared out so widely "
        "that no single pixel reaches 5σ, but the nine together do.",
        h=1.10, size=10.5)
caption(s, M, 7.02, 11.9,
        "Roughly 80 % of pixels reach the last branch and push the pedestal; "
        "~1.5 % are peaks and ~18 % sit in a peak's shadow.", size=9)
notes(s, """Why slide 4 leaves Test3 out: at 3x3 it is a small correction to the
cluster count, and the arc's point there is the THREE OUTCOMES shape -- store,
shadow, update -- not completeness. This slide is where completeness belongs.

Read c3 out loud, it is the part people find satisfying. Test1 asks whether one
pixel is 5 sigma above ITS OWN noise. Test3 asks the same question of the SUM of
nine pixels -- so the only thing needed is the noise on that sum, and that is
just variance addition:

    Var(v1 + ... + v9) = Var(v1) + ... + Var(v9) = 9 * sigma^2     (independent)
    sigma_sum = sqrt(9 * sigma^2) = 3 * sigma

Standard deviations do NOT add; variances do. That is the whole content of the
sqrt: nine pixels give nine times the variance but only three times the rms, so
a threshold on the sum has to be 3x larger to carry the same 5-sigma meaning.
It is also why summing helps at all -- the signal adds linearly (x9) while the
noise adds in quadrature (x3), a net sqrt(9) = 3x gain in signal-to-noise for a
photon that is genuinely spread over the window.

c3 = sqrt(ClusterSizeX * ClusterSizeY) in the constructor, so it generalises: at
9x9 it is sqrt(81) = 9.

If someone challenges the independence assumption: it is the pedestal noise that
is being added, which is per-pixel readout noise and is uncorrelated between
pixels to a good approximation. Correlated noise would need covariance terms and
would make c3 larger than sqrt(N).

The negative-value skip at the top is a fourth branch but not a test in the same
sense -- it drops pixels far BELOW pedestal, which are detector artefacts rather
than photons, and it does not update either.

What matters for the next slide: Test1 reads the MAX of the window, Test3 reads
the SUM. That is the whole difference in sensitivity.""")


# ---------------------------------------------------------------- A7 part 2
s = new_slide()
annex_chrome(s, 7, "which test causes the CPU/CPU gap · expands slide 30",
             "Test3 makes the clusters; Test1 only moves the pedestal",
             part=2, nparts=2, title_size=25)
bullets(s, M, 1.76, 11.9, [
    "**push_fast** touches only the pixel's **own** accumulators and never reads "
    "the stencil, so the update is **order-independent**: same set of updated "
    "pixels, bit-identical pedestal. Only a differing **decision** can diverge.",
], size=10.5)
figure(s, "fig_test3", M + 0.70, 2.34, 10.45)
callout(s, M, 6.22, 11.9,
        "Measured two ways. **Instrumented**: of the 19 disagreeing clusters, the "
        "**11 that only frozen finds are all Test 3**; the 8 only serial finds are "
        "all the local-max gate. **Ablated**: compile Test 3 out and the 11 go to "
        "**zero**.", h=0.72, size=10.5)
caption(s, M, 7.06, 11.9,
        "3×3 · 10 000 frames · 23 244 605 clusters · per-pixel branch codes "
        "diffed frame by frame, then re-run with Test 3 compiled out · "
        "python/tests/branch_trace.py, branch_site_dump.py.", size=9)
notes(s, """TWO EXPERIMENTS, INDEPENDENT ROUTES.

Instrumented (both finders shipped logic, branch codes recorded): 974 pixels out
of 1.6e9 take a different branch. Only some change the cluster set, and those
decompose onto slide 30's 8/11 with no remainder:

    frozen-only 11  =  QUIET_UPDATE -> TEST3_STORE      (all Test 3)
    cpu-only     8  =  TEST3_STORE  -> TEST3_SKIP  (5)  (local-max gate)
                    +  TEST1_STORE  -> SHADOW      (3)

Ablated (Test 3 compiled out of BOTH finders):

                        Test3 ON     Test3 OFF
    clusters (cpu)    23 244 602    23 241 342
    divergent pixels         974           584
    first divergence   frame   4     frame  30
    frozen-only               11             0
    cpu-only                   8             4

The 11 go to zero exactly. The cpu-only 4 are all TEST1_STORE -> SHADOW, the
local-max gate, which does not need Test 3 -- the count is not preserved because
ablating changes which pixels push, so the pedestal follows a different path and
the downstream ties are a different realisation.

WHY TEST1 FLIPS MOST AND COSTS NOTHING. A Test1 flip moves a pixel between
QUIET_UPDATE and SHADOW. Neither stores, so no cluster appears or disappears; it
only changes whether that pixel pushed, which feeds forward.

TEST1 IS NOT MERELY DOWNSTREAM. With Test3 off, the first divergence is still at
frame 30, and nothing diverged before it -- so the pedestals were identical and
that Test1 flip came straight from the within-frame asymmetry. Test1 initiates
independently; it is just ~7x slower to do so (frame 30 vs frame 4) and cannot
create a cluster on its own.

THE SITE. Frame 203, pixel (125,245). Threshold c3*5*rms = 256.511. Serial sums
256.489 and calls it a pedestal sample; frozen sums 256.581 and stores. The gap
is 0.09 ADU, from four neighbours whose pedestals differ by 0.12 ADU in total.
max is 56.473 in BOTH -- printed because it is the argument: Test1 could not
have caused this.

READING THE LEFT PANEL. It is the FINISHED frame, not a scan in progress -- the
branch map is read once find_clusters() returns, so every one of the 441 cells
carries a final decision. The arrows are raster ORDER, drawn outside the grid so
they cannot be mistaken for a cursor.

If asked why a photon looks 3x4 and not 3x3: it does not. The green cells are
the stored clusters and they are single pixels, nine of them in this patch. The
dark region around them is SHADOW, and shadow is not the photon's 3x3 -- it is
every pixel whose OWN 3x3 window contains something above 5 sigma. Note the
shadow pixel need not be bright itself. Three tiers, all visible here:

    810.3 ADU   IS the window max          -> stored
    345.0 ADU   above the 85.5 bar, not the peak  -> shadow
     17.7 ADU   nowhere near the bar, but its 3x3 reaches the 345 -> shadow

Charge shared across two adjacent pixels therefore lights up the union of every
window that can see either one, which here is 3x4. In the whole patch 27 pixels
clear 5 sigma, 9 are local maxima, and 98 end up shadowed.

The marked pixel is amber with a green ring on purpose: the panel is coloured by
the SERIAL finder, and serial sampled the pedestal there. Only frozen stored. If
it were painted plain green the picture would contradict the sigma lines beside
it. Amber means one thing in BOTH panels -- a pixel that pushed the pedestal --
which is why the frozen readings on the right are red rather than amber.

WORTH POINTING AT IF THE ROOM IS ENGINEERS, four rows below the marked pixel.
(129,245) is an isolated shadow cell with pedestal samples on both sides of it.
The cause is the 85.150 ADU pixel at (130,244), one row down and one column
left, which sits in the window of BOTH (129,245) and (129,244). Their window
maxima are IDENTICAL. They take different branches anyway, because the bar is
per-pixel -- the test is m > nSigma * rms[centre], the noise of the pixel being
tested. The branch codes alone bound the two:

    rms(129,245)  <  17.030  <=  rms(129,244)

The left neighbour is noisier, so its bar is higher, so the same 85.150 fails to
clear it. And (130,244) is itself only a pedestal sample: 85.2 does not clear its
OWN bar and its window sums to 119.5 against a ~256 Test3 threshold. So a pixel
can silence a neighbour's pedestal update without being bright enough to be
anything itself -- "bright" is not a property of a pixel, it is a relation
between a value and whose noise you measure it against.

Deliberately NOT marked on the figure. It is a statement about the algorithm,
not about serial-vs-frozen, and a second leader line would compete with the one
that carries the slide's actual argument. Full numbers in docs/deck/QA.md.

The branch_map property on both finders is diagnostic scaffolding. The ablation
is AARE_TEST3_ENABLED in the two headers, 1 by default.""")


prs.save(OUT)
print(f"saved {OUT}  ({len(prs.slides._sldIdLst)} slides)")
