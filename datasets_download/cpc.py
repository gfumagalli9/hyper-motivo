#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PatentsView (USPTO) → Hypergraph: patents × CPC

- Vertices  = patents (patent_id → integers 0..N-1)
- Hyperedges = CPC codes at the chosen granularity:
    section | class | subclass | group | subgroup
- Output .txt: ONE LINE = 1 hyperedge (CPC code),
    members = integer vertex IDs separated by commas (sorted, unique).

Key options:
  --granularity                  CPC level (default: group)
  --patent / --year-from / --year-to
                                 filter by grant year (requires g_patent.tsv.zip)
  --inventive-only               keep only "inventive/invention" CPC assignments
  --primary-only                 keep only the primary CPC (fallback on inventive if no explicit flags)
  --keep-sections "A,B,..."
                                 keep only selected CPC sections (e.g., "G,H")
  --keep-prefix "H01M,H04L"      keep only CPC codes with these prefixes
  --min-size K                   write only hyperedges with at least K members
  --nodes-out / --labels-out     optional CSVs for node/hyperedge metadata

Dependencies: Python standard library only (csv, zipfile, urllib, ...).
No manual unzip needed: reads .tsv directly from the .zip archives.
"""

import argparse, csv, io, os, re, sys, urllib.request, zipfile
from collections import defaultdict

# Multiple URLs (fall-through: try data.patentsview first, then S3)
PV_CPC_CURRENT_URLS = [
    "https://data.patentsview.org/download/g_cpc_current.tsv.zip",
    "https://s3.amazonaws.com/data.patentsview.org/download/g_cpc_current.tsv.zip",
]
PV_CPC_TITLE_URLS = [
    "https://data.patentsview.org/download/g_cpc_title.tsv.zip",
    "https://s3.amazonaws.com/data.patentsview.org/download/g_cpc_title.tsv.zip",
]
PV_PATENT_URLS = [
    "https://data.patentsview.org/download/g_patent.tsv.zip",
    "https://s3.amazonaws.com/data.patentsview.org/download/g_patent.tsv.zip",
]

def download(url: str, out_path: str):
    """
    Download a file from 'url' to 'out_path', unless it already exists.
    """
    if os.path.exists(out_path):
        print(f"[download] using cached file: {out_path}")
        return out_path
    print(f"[download] {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "pv-cpc-hypergraph/1.4"})
    with urllib.request.urlopen(req, timeout=60) as r, open(out_path, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    print(f"[download] saved to: {out_path}")
    return out_path

def download_any(urls, out_path):
    """
    Try a list of candidate URLs until one succeeds.
    """
    import urllib.error
    last = None
    for u in urls:
        try:
            return download(u, out_path)
        except Exception as e:
            print(f"[warn] download failed {u} → {e}")
            last = e
    raise SystemExit(f"Unable to download {out_path}: {last}")

# ---------- CPC level extraction helpers ----------

def _first(row, idx, keys):
    """
    Return the first non-empty, non-'\N' field among 'keys' in this TSV row.
    """
    for k in keys:
        j = idx.get(k)
        if j is not None and j < len(row):
            v = row[j].strip()
            if v and v != "\\N":
                return v
    return None

def normalize_cpc_levels(row, idx):
    """
    Reconstruct CPC codes at various levels (without spaces) from a g_cpc_current row.

    Returns a dict with keys in:
        'section', 'class', 'subclass', 'group', 'subgroup'
    Examples:
        section   → 'H'
        class     → 'H01'
        subclass  → 'H01M'
        group     → 'H01M8/00'
        subgroup  → 'H01M8/02'
    """
    sec  = _first(row, idx, ["cpc_section","section_id","cpc_section_id","section"])
    cls  = _first(row, idx, ["cpc_class","class_id","cpc_class_id","class"])
    subc = _first(row, idx, ["cpc_subclass","subclass_id","cpc_subclass_id","subclass"])
    grp  = _first(row, idx, ["cpc_group","group_id","cpc_group_id","group"])
    sgrp = _first(row, idx, ["cpc_subgroup","subgroup_id","cpc_subgroup_id","subgroup","cpc_subgroup_symbol","symbol"])

    levels = {}
    if sec:
        levels["section"] = sec
    if sec and cls:
        levels["class"] = f"{sec}{cls}"
    if sec and cls and subc:
        levels["subclass"] = f"{sec}{cls}{subc}"

    if levels.get("subclass") and (grp or sgrp):
        base = levels["subclass"]
        if sgrp and "/" in sgrp:
            # e.g., H01M + "8/02" → H01M8/02
            code = f"{base}{sgrp.replace(' ', '')}"
        elif grp:
            # group without explicit subgroup → */00
            code = f"{base}{grp}/00"
        else:
            code = None
        if code:
            # 'group' = main group (*/00); 'subgroup' = full code (if present)
            levels["group"] = re.sub(r"\s+","", f"{base}{grp}/00") if grp else re.sub(r"\s+","", code)
            levels["subgroup"] = re.sub(r"\s+","", code)
    return levels

# ---------- "inventive"/"primary" flags ----------

def _truthy(v: str) -> bool:
    """
    Interpret typical truthy markers like 1, t, true, y, yes.
    """
    return v.lower() in {"1", "t", "true", "y", "yes"}

def row_is_inventive(row, idx):
    """
    Try to infer whether this CPC assignment is 'inventive' or 'invention'.
    Returns:
      True  = explicitly inventive
      False = explicitly not inventive
      None  = unknown / no information
    """
    cat = _first(row, idx, ["cpc_category","category"])
    if cat and "invent" in cat.lower():
        return True
    inv = _first(row, idx, ["is_inventive","inventive"])
    if inv is not None:
        return _truthy(inv)
    return None  # unknown

def row_is_primary(row, idx):
    """
    Try to infer whether this CPC assignment is the primary one.

    Checks explicit flags like is_primary/is_main/...,
    or falls back to sequence/ordinal fields (sequence 0 or 1).
    """
    for key in ("is_primary","is_main","is_first","primary"):
        v = _first(row, idx, [key])
        if v is not None:
            return _truthy(v)
    seq = _first(row, idx, ["cpc_sequence","sequence","ordinal","position"])
    if seq is not None:
        try:
            s = int(seq)
            return (s == 0) or (s == 1)
        except:
            pass
    return None  # unknown

# ---------- year-based patent filtering (g_patent) ----------

def load_allowed_patents_by_year(patent_zip_path, y_from, y_to):
    """
    Load the set of patent_ids whose grant date falls in [y_from, y_to].

    Reads g_patent.tsv from the given .zip, finds a date column (grant_date/patent_date/date),
    and keeps only patents whose year is within the requested range.
    """
    allowed = set()
    with zipfile.ZipFile(patent_zip_path) as zf:
        tsv = [n for n in zf.namelist() if n.endswith(".tsv")][0]
        with zf.open(tsv) as f:
            rdr = csv.reader(io.TextIOWrapper(f, encoding="utf-8", newline=""), delimiter="\t")
            header = next(rdr)
            ix = {h: i for i, h in enumerate(header)}
            pid_col = "patent_id" if "patent_id" in ix else ("id" if "id" in ix else None)
            if not pid_col:
                raise SystemExit("g_patent.tsv: 'patent_id' column not found")
            date_col = next((k for k in ("grant_date","patent_date","date") if k in ix), None)
            if not date_col:
                raise SystemExit("g_patent.tsv: 'grant_date'/'patent_date' column not found")
            for row in rdr:
                if len(row) <= max(ix[pid_col], ix[date_col]):
                    continue
                pid = row[ix[pid_col]].strip()
                d   = row[ix[date_col]].strip()
                if not pid or not d or len(d) < 4:
                    continue
                try:
                    y = int(d[:4])
                except:
                    continue
                if (y_from is not None and y < y_from) or (y_to is not None and y > y_to):
                    continue
                allowed.add(pid)
    print(f"[years] patents in range {y_from or '-inf'}–{y_to or '+inf'}: {len(allowed):,}")
    return allowed

def main():
    ap = argparse.ArgumentParser(description="PatentsView CPC → hypergraph (.txt)")
    ap.add_argument("--out", required=True,
                    help=".txt hypergraph: one line = one CPC code; members = integer IDs separated by commas")
    ap.add_argument("--granularity",
                    choices=["section","class","subclass","group","subgroup"],
                    default="group",
                    help="CPC level to use as hyperedges (default: group).")
    ap.add_argument("--nodes-out", default="",
                    help="(optional) CSV mapping nodes: node_int_id,patent_id")
    ap.add_argument("--labels-out", default="",
                    help="(optional) CSV hyperedge metadata: line_index,cpc,size[,title]")
    ap.add_argument("--cpc-current", default="",
                    help="path to g_cpc_current.tsv.zip (if empty, try to download automatically)")
    ap.add_argument("--cpc-title",   default="",
                    help="(optional) path to g_cpc_title.tsv.zip (for CPC titles)")
    ap.add_argument("--patent",      default="",
                    help="(optional) path to g_patent.tsv.zip to enable year-based filtering")
    ap.add_argument("--year-from", type=int, default=None,
                    help="minimum grant year (inclusive) for patents to keep")
    ap.add_argument("--year-to",   type=int, default=None,
                    help="maximum grant year (inclusive) for patents to keep")
    ap.add_argument("--keep-sections", default="",
                    help="e.g., 'A,B,C,D,E,F,G,H' (if set, sections outside this set are dropped)")
    ap.add_argument("--keep-prefix",   default="",
                    help="e.g., 'H01M,H04L' (keep only CPC codes whose prefix starts with one of these)")
    ap.add_argument("--inventive-only", action="store_true",
                    help="keep only CPC assignments marked as inventive/invention")
    ap.add_argument("--primary-only",   action="store_true",
                    help="keep only the primary CPC (fallback on inventive when no explicit primary flag)")
    ap.add_argument("--min-size", type=int, default=0,
                    help="write only hyperedges with at least MIN distinct members (0 = no filter)")
    ap.add_argument("--max-rows", type=int, default=0,
                    help="for testing: limit the number of rows read from g_cpc_current.tsv (0 = no limit)")
    args = ap.parse_args()

    if (args.year_from is not None or args.year_to is not None) and not args.patent:
        print("[info] year filtering requested: attempting to download g_patent.tsv.zip ...")
        args.patent = download_any(PV_PATENT_URLS, "g_patent.tsv.zip")
    if args.inventive_only and args.primary_only:
        raise SystemExit("Choose exactly one: --inventive-only OR --primary-only")

    # Ensure CPC input file is available
    if not args.cpc_current:
        args.cpc_current = download_any(PV_CPC_CURRENT_URLS, "g_cpc_current.tsv.zip")
    if args.labels_out and not args.cpc_title:
        try:
            args.cpc_title = download_any(PV_CPC_TITLE_URLS, "g_cpc_title.tsv.zip")
        except SystemExit:
            args.cpc_title = ""

    # Map of patents allowed by grant year
    allowed_patents = None
    if args.year_from is not None or args.year_to is not None:
        allowed_patents = load_allowed_patents_by_year(args.patent, args.year_from, args.year_to)

    # Load CPC titles (optional, for labels_out)
    cpc_titles = {}
    if args.cpc_title and os.path.exists(args.cpc_title):
        with zipfile.ZipFile(args.cpc_title) as zf:
            tsv = [n for n in zf.namelist() if n.endswith(".tsv")][0]
            with zf.open(tsv) as f:
                rdr = csv.reader(io.TextIOWrapper(f, encoding="utf-8", newline=""), delimiter="\t")
                header = next(rdr)
                ix = {h: i for i, h in enumerate(header)}
                symcol = next((k for k in ("symbol","cpc_subgroup_symbol","cpc_symbol","cpc") if k in ix), None)
                ttlcol = next((k for k in ("title","cpc_title","title_full") if k in ix), None)
                if symcol and ttlcol:
                    for row in rdr:
                        if len(row) <= max(ix[symcol], ix[ttlcol]):
                            continue
                        sym = row[ix[symcol]].strip().replace(" ", "")
                        ttl = row[ix[ttlcol]].strip()
                        if sym:
                            cpc_titles[sym] = ttl

    keep_sections = {s.strip().upper() for s in args.keep_sections.split(",") if s.strip()}
    keep_prefixes = [p.strip().upper() for p in args.keep_prefix.split(",") if p.strip()]

    # Build hypergraph: CPC code → list of integer patent vertex IDs
    group = defaultdict(list)   # CPC code -> [node_int_id, ...]
    node_id, next_id = {}, 0    # patent_id -> int node_id

    with zipfile.ZipFile(args.cpc_current) as zf:
        tsv = [n for n in zf.namelist() if n.endswith(".tsv")][0]
        with zf.open(tsv) as f:
            rdr = csv.reader(io.TextIOWrapper(f, encoding="utf-8", newline=""), delimiter="\t")
            header = next(rdr)
            ix = {h: i for i, h in enumerate(header)}

            pid_col = next((k for k in ("patent_id","patent","id") if k in ix), None)
            if not pid_col:
                raise SystemExit("g_cpc_current.tsv: 'patent_id' column not found")

            # Check if role flags (inventive/primary) are available
            has_inv  = any(k in ix for k in ("cpc_category","category","is_inventive","inventive"))
            has_prim = any(k in ix for k in ("is_primary","is_main","is_first","primary",
                                             "cpc_sequence","sequence","ordinal","position"))
            fallback_primary_to_inventive = args.primary_only and not has_prim and has_inv
            if args.primary_only and not has_prim and not has_inv:
                print("[warn] no 'primary'/'inventive' indicator found in g_cpc_current.tsv; "
                      "no role-based filtering will be applied.")

            n = 0
            for row in rdr:
                n += 1
                if args.max_rows and n > args.max_rows:
                    break
                if len(row) <= ix[pid_col]:
                    continue
                pid = row[ix[pid_col]].strip()
                if not pid:
                    continue

                # Filter by grant year (if enabled)
                if allowed_patents is not None and pid not in allowed_patents:
                    continue

                # Role filters (inventive / primary)
                if args.inventive_only:
                    inv = row_is_inventive(row, ix) if has_inv else None
                    if inv is False:   # explicitly non-inventive
                        continue
                elif args.primary_only:
                    if fallback_primary_to_inventive:
                        inv = row_is_inventive(row, ix)
                        if inv is False:
                            continue
                    else:
                        pri = row_is_primary(row, ix) if has_prim else None
                        if pri is False:
                            continue

                levels = normalize_cpc_levels(row, ix)
                code = levels.get(args.granularity)
                if not code:
                    continue
                sym = re.sub(r"\s+","", code).upper()

                # Semantic CPC filters
                if keep_sections and (sym[0] not in keep_sections):
                    continue
                if keep_prefixes and not any(sym.startswith(p) for p in keep_prefixes):
                    continue

                # Assign integer node ID to this patent
                if pid not in node_id:
                    node_id[pid] = next_id
                    next_id += 1
                vid = node_id[pid]
                group[sym].append(vid)

                if n % 1_000_000 == 0:
                    print(f"[parse] rows processed: {n:,}")

    print(f"[stats] #patents: {len(node_id):,} | "
          f"#hyperedges (CPC {args.granularity}): {len(group):,}")

    # Write main hypergraph .txt, with min-size filter
    labels = sorted(
        k for k, v in group.items()
        if (len(set(v)) >= max(0, args.min_size))
    )
    with open(args.out, "w", encoding="utf-8") as out:
        for lab in labels:
            ids = sorted(set(group[lab]))
            if ids:
                out.write(",".join(map(str, ids)) + "\n")
    print(f"[ok] hypergraph written to: {args.out}  |  hyperedges written: {len(labels):,}")

    # Optional node mapping CSV
    if args.nodes_out:
        with open(args.nodes_out, "w", encoding="utf-8") as nout:
            nout.write("node_int_id,patent_id\n")
            for pid, nid in sorted(node_id.items(), key=lambda kv: kv[1]):
                nout.write(f"{nid},{pid}\n")
        print(f"[ok] node mapping written to: {args.nodes_out}")

    # Optional hyperedge metadata CSV
    if args.labels_out:
        with open(args.labels_out, "w", encoding="utf-8") as lout:
            lout.write("line_index,cpc,size,title\n")
            for i, lab in enumerate(labels):
                size = len(set(group[lab]))
                title = ""
                if args.granularity in ("group", "subgroup"):
                    title = cpc_titles.get(lab, "")
                lout.write(f"{i},{lab},{size},{title}\n")
        print(f"[ok] hyperedge metadata written to: {args.labels_out}")

if __name__ == "__main__":
    main()
