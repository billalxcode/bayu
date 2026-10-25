#!/usr/bin/env python3
"""Build a static index.html per directory from `rclone lsjson -R --files-only` on stdin.

Usage: rclone lsjson -R --files-only r2:repo | mkindex.py OUTDIR
Links are relative and point at dir/index.html, so it works on any static host.
"""
import html, json, os, sys
from collections import defaultdict
from datetime import datetime, timedelta, timezone

# Without JavaScript the times read in Jakarta's (WIB); the page's script
# shows them in the visitor's own time zone.
WIB = timezone(timedelta(hours=7), "WIB")


def modified(stamp):
    """rclone's ModTime: an ISO time with Z or an offset, nanoseconds maybe."""
    day, _, rest = stamp.partition("T")
    clock = rest[:8]
    zone = rest[8:].lstrip("0123456789.")
    zone = "+00:00" if zone in ("", "Z") else zone
    when = datetime.fromisoformat(f"{day}T{clock}{zone}").astimezone(timezone.utc)
    return when.strftime("%Y-%m-%dT%H:%M:%SZ"), when.astimezone(WIB).strftime("%Y-%m-%d %H:%M WIB")

out = sys.argv[1]
files, dirs = defaultdict(list), defaultdict(set)
for e in json.load(sys.stdin):
    p = e["Path"]
    if p == "index.html" or p.endswith("/index.html"):
        continue
    d, name = os.path.split(p)
    files[d].append((name, e["Size"], *modified(e["ModTime"])))
    while d:  # register every ancestor so empty-of-files dirs still get a page
        parent, child = os.path.split(d)
        dirs[parent].add(child)
        d = parent
dirs[""]  # root always exists


def size(n):
    for u in ("B", "K", "M", "G"):
        if n < 1024:
            return f"{n:.0f}{u}" if u == "B" else f"{n:.1f}{u}"
        n /= 1024
    return f"{n:.1f}T"


PAGE = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>repo/{t}</title>
<style>body{{font:14px system-ui,sans-serif;margin:0 auto;max-width:960px;padding:16px;background:#fff;color:#222}}
@media(prefers-color-scheme:dark){{body{{background:#111;color:#ddd}}a{{color:#8ab4f8}}tr:hover{{background:#222}}}}
h1{{font-size:18px;word-break:break-all}}input{{width:100%;padding:8px;box-sizing:border-box;margin-bottom:8px}}
table{{width:100%;border-collapse:collapse}}td{{padding:4px 6px;border-bottom:1px solid #8883}}
td:nth-child(n+2){{text-align:right;white-space:nowrap;color:#888}}td a{{word-break:break-all}}tr:hover{{background:#f4f4f4}}</style>
</head><body><h1>{crumbs}</h1><input id=q placeholder="Filter…" autofocus>
<table id=t><tr><th align=left>Name</th><th>Size</th><th>Modified</th></tr>{rows}</table>
<script>q.oninput=()=>{{const v=q.value.toLowerCase();for(const r of t.rows)if(r.dataset.n!==undefined)r.hidden=!r.dataset.n.includes(v)}}
try{{const f=new Intl.DateTimeFormat(undefined,{{year:"numeric",month:"2-digit",day:"2-digit",hour:"2-digit",minute:"2-digit",hour12:false,timeZoneName:"short"}});
for(const c of document.querySelectorAll("td[data-t]")){{const d=new Date(c.dataset.t);c.textContent=f.format(d);c.title=d.toISOString()}}}}catch(e){{}}</script>
</body></html>"""

for d in sorted(set(dirs) | set(files)):
    depth = d.count("/") + 1 if d else 0
    parts = d.split("/") if d else []
    crumbs = [f'<a href="{"../" * depth}index.html">repo</a>']
    for i, p in enumerate(parts):
        crumbs.append(f'<a href="{"../" * (depth - i - 1)}index.html">{html.escape(p)}</a>')
    rows = [] if not d else ['<tr><td><a href="../index.html">../</a></td><td></td><td></td></tr>']
    for c in sorted(dirs[d]):
        q = html.escape(c, quote=True)
        rows.append(f'<tr data-n="{q.lower()}"><td><a href="{q}/index.html">{q}/</a></td><td>-</td><td></td></tr>')
    for name, n, utc, wib in sorted(files[d]):
        q = html.escape(name, quote=True)
        rows.append(f'<tr data-n="{q.lower()}"><td><a href="{q}">{q}</a></td><td>{size(n)}</td>'
                    f'<td data-t="{utc}">{wib}</td></tr>')
    target = os.path.join(out, d)
    os.makedirs(target, exist_ok=True)
    with open(os.path.join(target, "index.html"), "w") as f:
        f.write(PAGE.format(t=html.escape(d), crumbs=" / ".join(crumbs), rows="".join(rows)))
