"""MkDocs hook: titles for boxes written in the GitHub alert syntax (docs/STYLE.md, "Boxes").

A box is written so that GitHub and this site both render it:

    > [!WARNING]
    > **Decoder mode only without the CAN box**
    >
    > Text of the box.

GitHub shows "Warning" with the bold line under it. pymdownx.quotes (callouts) turns the alert into
a Material admonition with the default title "Warning"; this hook replaces that default title with
the bold first line, so the site shows the box title as before. It also gives "> [!CAUTION]" the red
"danger" style (Material styles "caution" like "warning"). Only the HTML of the page changes.
Standard library only.
"""
import re

_DEFAULT = {"note": "Note", "tip": "Tip", "warning": "Warning", "caution": "Caution"}
_BOX_RE = re.compile(
    r'<div class="admonition (note|tip|warning|caution)">(\s*)'
    r'<p class="admonition-title">(Note|Tip|Warning|Caution)</p>\s*'
    r'<p><strong>((?:(?!</?p\b).)*?)</strong></p>',
    re.S)


def _title(m):
    kind, space, default, title = m.groups()
    if _DEFAULT[kind] != default:
        return m.group(0)
    css = "danger" if kind == "caution" else kind
    return '<div class="admonition %s">%s<p class="admonition-title">%s</p>' % (css, space, title)


def on_page_content(html, **kwargs):
    html = _BOX_RE.sub(_title, html)
    # a caution box without a bold first line keeps its default title, in the danger colour
    return html.replace('<div class="admonition caution">', '<div class="admonition danger">')
