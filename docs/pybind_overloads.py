# SPDX-License-Identifier: MPL-2.0
"""Sphinx extension that documents pybind11 overloads as separate entries.

pybind11 merges the docstrings of all overloads of a function into one::

    name(*args, **kwargs)
    Overloaded function.

    1. name(self: pkg.Cls, a: int) -> int

        docstring of overload 1

    2. name(self: pkg.Cls, a: float) -> float

        docstring of overload 2

This extension replaces autodoc's function and method documenters so that
every overload is rendered as its own entry, with its own signature and
docstring, as if it were a separate function::

    name(a: int) -> int
        docstring of overload 1

    name(a: float) -> float
        docstring of overload 2

Every overload body goes through ``autodoc-process-docstring`` on its own, so
napoleon renders the ``Parameters``/``Returns`` sections of each overload.

Works for any overloaded function, method, operator or ``__call__``.
"""

import re
import textwrap

from sphinx.ext.autodoc import FunctionDocumenter, MethodDocumenter
from sphinx.util.docstrings import prepare_docstring

OVERLOAD_MARKER = "Overloaded function."

# "1. name(args) -> ret"; args may contain nested parentheses (default values),
# so match greedily up to the last ")" before an optional "-> ret".
_HEADER_RE = re.compile(
    r"^(?P<idx>\d+)\.\s+(?P<name>[\w.]+)\((?P<args>.*)\)\s*(?:->\s*(?P<ret>.+?))?\s*$"
)
# Leading "self" / "self: pkg.Cls" argument of a bound method
_SELF_RE = re.compile(r"^\s*self(?:\s*:\s*[^,]+)?\s*(?:,\s*|$)")
# Fully qualified dotted names, e.g. aare._aare.ROI or numpy.typing.NDArray
#_QUALIFIED_RE = re.compile(r"\b(?:[A-Za-z_]\w*\.)+([A-Za-z_]\w*)")


def _split_overloads(lines):
    """Return [(header_match, body_lines), ...] or None if not overloaded."""
    try:
        start = next(i for i, l in enumerate(lines) if l.strip() == OVERLOAD_MARKER)
    except StopIteration:
        return None

    # Everything before the marker must be blank or the "name(*args, **kwargs)" line
    if any(l.strip() and not l.strip().endswith("(*args, **kwargs)") for l in lines[:start]):
        return None

    overloads = []
    func_name = None
    for line in lines[start + 1:]:
        m = _HEADER_RE.match(line)
        # Only accept "N. name(" at column 0 with the expected index and the same
        # name, so numbered lists inside a docstring are not mistaken for headers.
        if (m and int(m["idx"]) == len(overloads) + 1
                and func_name in (None, m["name"])):
            func_name = m["name"]
            overloads.append((m, []))
        elif overloads:
            overloads[-1][1].append(line)

    return overloads or None


class PybindOverloadMixin:
    """Render each pybind11 overload as a separate directive.

    The first overload uses the directive autodoc generates anyway; the others
    are appended as sibling directives (with ``:no-index:`` to avoid duplicate
    object warnings) after the first overload's content.
    """

    _overloads = None
    _current = 0

    def format_signature(self, **kwargs):
        doc = getattr(self.object, "__doc__", None)
        self._overloads = _split_overloads(prepare_docstring(doc)) if isinstance(doc, str) else None
        if not self._overloads:
            return super().format_signature(**kwargs)
        return self._overload_signature(0)

    def get_doc(self):
        if not self._overloads:
            return super().get_doc()
        body = self._overloads[self._current][1]
        return [prepare_docstring(textwrap.dedent("\n".join(body)))]

    def add_content(self, more_content):
        super().add_content(more_content)
        if not self._overloads or self._current:
            return

        sourcename = self.get_sourcename()
        content_indent = self.indent
        outer_indent = content_indent[: len(content_indent) - len(self.content_indent)]
        for i in range(1, len(self._overloads)):
            self._current = i
            self.indent = outer_indent
            self.add_line("", sourcename)
            self.add_directive_header(self._overload_signature(i))
            if not (self.options.no_index or self.options.noindex):
                self.add_line("   :no-index:", sourcename)
            self.add_line("", sourcename)
            self.indent = content_indent
            super().add_content(None)
        self._current = 0

    def _overload_signature(self, i):
        header = self._overloads[i][0]
        sig = f"({_SELF_RE.sub('', header['args'], count=1)})"
        if header["ret"]:
            sig += f" -> {header['ret']}"
        return sig


class PybindFunctionDocumenter(PybindOverloadMixin, FunctionDocumenter):
    pass


class PybindMethodDocumenter(PybindOverloadMixin, MethodDocumenter):
    pass


def setup(app):
    app.setup_extension("sphinx.ext.autodoc")
    #app.add_config_value("pybind_overloads_short_names", False, "env") # not sure if needed 
    app.add_autodocumenter(PybindFunctionDocumenter, override=True) # overwrites existing pybind autodoc function documenter
    app.add_autodocumenter(PybindMethodDocumenter, override=True)
    return {"parallel_read_safe": True, "parallel_write_safe": True} # extension metadata 
