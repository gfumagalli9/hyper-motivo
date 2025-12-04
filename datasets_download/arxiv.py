#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
arXiv (OAI-PMH) → hypergraph: articles × arXiv categories

- Vertices = arXiv articles (remapped to integers 0..N-1)
- Hyperedges = arXiv categories (e.g., 'cs.LG', 'math.PR', ...)
- Output .txt: ONE LINE = 1 hyperedge (category), members = integer node IDs
  separated by commas (sorted, unique).

Dependencies: only Python standard library.

Examples:
  # Sample one week of submissions, all categories (primary + secondary)
  python3 arxiv_concepts_hypergraph.py \
    --from 2025-01-01 --until 2025-01-07 \
    --out arxiv_cats_sample.txt \
    --nodes-out nodes.csv --labels-out hyperedges.csv

  # Only categories whose archive prefix is in {cs, stat, math} (primary + secondary)
  python3 arxiv_concepts_hypergraph.py \
    --from 2025-01-01 --until 2025-01-07 \
    --include-archives cs,stat,math \
    --out arxiv_cs_math_stat.txt

  # Only PRIMARY category per article (fewer labels per article, lower beta),
  # for the last month
  python3 arxiv_concepts_hypergraph.py \
    --from 2025-10-01 --until 2025-10-31 \
    --primary-only \
    --out arxiv_primary_only.txt
"""

import argparse, io, sys, time, urllib.parse, urllib.request, xml.etree.ElementTree as ET
from collections import defaultdict

BASE = "https://export.arxiv.org/oai2"  # official OAI-PMH endpoint
NS = {
    "oai": "http://www.openarchives.org/OAI/2.0/",
    "arxiv": "http://arxiv.org/OAI/arXiv/",
}

def fetch(url, retries=5, sleep_sec=3):
    """HTTP GET with simple retry/backoff (arXiv asks for polite access)."""
    last = None
    for i in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "arxiv-hypergraph/1.1"})
            with urllib.request.urlopen(req, timeout=60) as r:
                return r.read()
        except Exception as e:
            last = e
            time.sleep(sleep_sec * (1 + i))
    raise RuntimeError(f"fetch failed: {url} -> {last}")

def list_records(date_from, date_until, metadata_prefix="arXiv", max_records=0):
    """
    Iterator over OAI-PMH records using ListRecords, handling resumptionToken.

    Yields tuples (xml_record_element, raw_xml_tree_root) for each record.
    """
    params = {
        "verb": "ListRecords",
        "metadataPrefix": metadata_prefix,
        "from": date_from,
        "until": date_until,
    }
    recs = 0
    url = BASE + "?" + urllib.parse.urlencode(params)
    while True:
        data = fetch(url)
        root = ET.fromstring(data)
        for rec in root.findall(".//oai:record", NS):
            header = rec.find("oai:header", NS)
            if header is not None and header.get("status") == "deleted":
                continue
            yield rec, root
            recs += 1
            if max_records and recs >= max_records:
                return
        # handle resumptionToken (pagination)
        token_el = root.find(".//oai:resumptionToken", NS)
        if token_el is None or (token_el.text is None) or (token_el.text.strip() == ""):
            break
        token = token_el.text.strip()
        url = BASE + "?" + urllib.parse.urlencode({"verb": "ListRecords", "resumptionToken": token})

def parse_record(rec, primary_only=False):
    """
    Extract from one OAI-PMH <record>:
      - arXiv ID (string), e.g., '0704.0001', '2101.01234'
      - list of categories (primary + secondary) OR only primary if primary_only=True
    """
    md = rec.find("oai:metadata", NS)
    if md is None: return None, []
    ax = md.find("arxiv:arXiv", NS)
    if ax is None: return None, []
    # arXiv ID
    id_el = ax.find("arxiv:id", NS)
    if id_el is None or not id_el.text: return None, []
    aid = id_el.text.strip()

    cats = set()
    # primary category
    p = ax.find("arxiv:primary_category", NS)
    if p is not None and p.get("term"):
        cats.add(p.get("term").strip())
    # all categories (space-separated) if not in primary_only mode
    if not primary_only:
        cats_el = ax.find("arxiv:categories", NS)
        if cats_el is not None and cats_el.text:
            for c in cats_el.text.strip().split():
                cats.add(c.strip())

    return aid, sorted(cats)

def main():
    ap = argparse.ArgumentParser(
        description="Build a hypergraph (articles × categories) from arXiv via OAI-PMH."
    )
    ap.add_argument("--from", dest="date_from", required=True,
                    help="start date (YYYY-MM-DD, inclusive)")
    ap.add_argument("--until", dest="date_until", required=True,
                    help="end date (YYYY-MM-DD, inclusive)")
    ap.add_argument("--out", required=True,
                    help="TXT hypergraph: one line = one category; members = integer node IDs, comma-separated")
    ap.add_argument("--nodes-out", default="",
                    help="(optional) CSV node map: node_int_id,arxiv_id")
    ap.add_argument("--labels-out", default="",
                    help="(optional) CSV hyperedge metadata: line_index,category,size")
    ap.add_argument("--include-archives", default="",
                    help="filter categories by archive prefix (e.g.: 'cs,stat,math,physics'); "
                         "prefix is the part before the dot, e.g., 'cs' in 'cs.LG'")
    ap.add_argument("--primary-only", action="store_true",
                    help="use only the primary category per article (fewer labels per node; lower beta)")
    ap.add_argument("--max-records", type=int, default=0,
                    help="limit the number of records for testing (0 = no limit)")
    args = ap.parse_args()

    allowed_prefixes = {s.strip() for s in args.include_archives.split(",") if s.strip()}

    # groups: category -> list of integer node IDs
    group = defaultdict(list)
    node_id = {}     # arxiv_id -> int node ID
    next_id = 0

    # download and parse OAI-PMH records
    total = 0
    for rec, _root in list_records(args.date_from, args.date_until, max_records=args.max_records):
        aid, cats = parse_record(rec, primary_only=args.primary_only)
        if not aid or not cats:
            continue
        # filter by archive prefix (part before '.', e.g., 'cs' in 'cs.LG')
        if allowed_prefixes:
            cats = [c for c in cats if c.split(".", 1)[0] in allowed_prefixes]
            if not cats:
                continue

        # assign integer node ID to this article
        if aid not in node_id:
            node_id[aid] = next_id
            next_id += 1
        vid = node_id[aid]

        # add this article to all its categories (hyperedges)
        for c in cats:
            group[c].append(vid)

        total += 1
        if total % 1000 == 0:
            print(f"[progress] records fetched: {total:,} | "
                  f"articles: {len(node_id):,} | categories so far: {len(group):,}")

    print(f"[stats] total articles: {len(node_id):,} | total categories: {len(group):,}")

    # write hypergraph as a plain-text file
    labels = sorted(group.keys())
    with open(args.out, "w", encoding="utf-8") as out:
        for lab in labels:
            ids = sorted(set(group[lab]))
            if ids:
                out.write(",".join(map(str, ids)) + "\n")
    print(f"[ok] hypergraph written to: {args.out}")

    # optional: write node mapping (integer ID -> arXiv ID)
    if args.nodes_out:
        with open(args.nodes_out, "w", encoding="utf-8") as nout:
            nout.write("node_int_id,arxiv_id\n")
            for aid, nid in sorted(node_id.items(), key=lambda kv: kv[1]):
                nout.write(f"{nid},{aid}\n")
        print(f"[ok] node mapping written to: {args.nodes_out}")

    # optional: write hyperedge metadata (one row per line in the TXT hypergraph)
    if args.labels_out:
        with open(args.labels_out, "w", encoding="utf-8") as lout:
            lout.write("line_index,category,size\n")
            for i, lab in enumerate(labels):
                size = len(set(group[lab]))
                if size > 0:
                    lout.write(f"{i},{lab},{size}\n")
        print(f"[ok] hyperedge metadata written to: {args.labels_out}")

if __name__ == "__main__":
    main()
