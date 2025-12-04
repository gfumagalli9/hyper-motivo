#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import html
import re
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import defaultdict, OrderedDict, Counter
from pathlib import Path

import requests
import py7zr

# -----------------------------
# URL resolver (Archive.org)
# -----------------------------

def resolve_dump_url(site_domain: str) -> str:
    """
    Resolve the URL of the StackExchange dump (.7z) on Archive.org for a given site.
    Tries both '<site>-Posts.7z' and '<site>.7z'.
    """
    base = f"https://archive.org/download/stackexchange/{site_domain}"
    for url in (f"{base}-Posts.7z", f"{base}.7z"):
        try:
            r = requests.get(url, stream=True, timeout=30, allow_redirects=True)
            if r.status_code == 200:
                r.close()
                return url
        except requests.RequestException:
            pass
    raise RuntimeError(
        f"No dump archive found for {site_domain} on Archive.org. "
        "Please check /download/stackexchange/ manually."
    )

# -----------------------------
# Download + extraction
# -----------------------------

def download_file(url: str, dest_path: Path, chunk_mb: int = 1) -> None:
    """
    Download a file from 'url' to 'dest_path', with a simple progress indicator.
    """
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    with requests.get(url, stream=True, timeout=60) as r:
        r.raise_for_status()
        total = int(r.headers.get("Content-Length") or 0)
        done = 0
        chunk_size = chunk_mb * 1024 * 1024
        with open(dest_path, "wb") as f:
            for chunk in r.iter_content(chunk_size=chunk_size):
                if chunk:
                    f.write(chunk)
                    done += len(chunk)
                    if total:
                        pct = (done / total) * 100
                        print(
                            f"\rDownloading: {done/1e6:,.1f}/{total/1e6:,.1f} MB ({pct:,.1f}%)",
                            end="",
                        )
        print()
    print(f"Saved archive to: {dest_path}")

def extract_needed_xml(archive_path: Path, out_dir: Path,
                       wanted=("posts.xml", "tags.xml")) -> dict[str, Path]:
    """
    Extract only the XML files we need (e.g., posts.xml, tags.xml) from the .7z archive.
    Returns a mapping { 'posts.xml': Path, 'tags.xml': Path (if present) }.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    out: dict[str, Path] = {}
    with py7zr.SevenZipFile(archive_path, mode="r") as z:
        names = z.getnames()
        lower2real = {n.lower(): n for n in names}
        targets = {}
        for nlow, nreal in lower2real.items():
            for w in wanted:
                if nlow.endswith(w):
                    targets[w] = nreal
        if "posts.xml" not in targets:
            raise RuntimeError("posts.xml not found in the archive.")
        z.extract(path=out_dir, targets=list(targets.values()))
        for w, real in targets.items():
            out[w] = (out_dir / real)
    return out

# -----------------------------
# Parsing helpers
# -----------------------------

ANGLE_RE = re.compile(r"<([^<>]+)>")

def parse_tag_field(tag_field: str) -> list[str]:
    """
    Parse the tag field from StackExchange posts.

    Supports both dump formats:
      - HTML-escaped with angle brackets: '&lt;python&gt;&lt;pandas&gt;'
      - pipe-delimited: '|machine-learning|definitions|'
    """
    if not tag_field:
        return []
    s = html.unescape(tag_field.strip())

    # Case 1: angle-bracket encoding
    if "<" in s and ">" in s:
        return ANGLE_RE.findall(s)

    # Case 2: pipe-delimited
    if "|" in s:
        return [t for t in s.split("|") if t]

    return [s] if s else []

def read_tag_counts(tags_xml_path: Path) -> dict[str, int]:
    """
    Read tag counts from Tags.xml (TagName/Count or tagname/count).
    Returns: tag_name -> global count.
    """
    counts: dict[str, int] = {}
    for _, elem in ET.iterparse(tags_xml_path, events=("end",)):
        if elem.tag.rsplit("}", 1)[-1].lower() != "row":
            elem.clear()
            continue
        a = elem.attrib
        name = a.get("TagName") or a.get("tagname")
        cnt = a.get("Count") or a.get("count")
        if name and cnt:
            try:
                counts[name] = int(cnt)
            except ValueError:
                pass
        elem.clear()
    return counts

# -----------------------------
# Hypergraph construction
# -----------------------------

# Two synthetic posts included in the dumps starting July 2025 (should be excluded).
SYNTHETIC_POST_IDS = {"1000000001", "1000000010"}

def is_synthetic_post(qid: str | None) -> bool:
    """
    Heuristic filter for synthetic / placeholder posts in the dump.
    """
    if not qid:
        return False
    if qid in SYNTHETIC_POST_IDS:
        return True
    try:
        return int(qid) >= 1_000_000_000
    except ValueError:
        return False

def diagnose_posts_xml(posts_xml_path: Path, max_print: int = 5):
    """
    Quick diagnostic pass on posts.xml:
      - counts per PostTypeId
      - how many posts have Tags vs no Tags
      - a few example rows (for sanity checking the schema)
    """
    counts = Counter()
    with_tags = 0
    without_tags = 0
    examples = []

    for _, elem in ET.iterparse(posts_xml_path, events=("end",)):
        a = elem.attrib
        pt = a.get("PostTypeId") or a.get("posttypeid")
        if not pt:
            elem.clear()
            continue
        counts[pt] += 1
        if (a.get("Tags") or a.get("tags")):
            with_tags += 1
        else:
            without_tags += 1
        if len(examples) < max_print:
            examples.append({
                k: a.get(k) for k in ("Id", "PostTypeId", "ParentId", "Tags", "Title", "CreationDate")
            })
        elem.clear()

    print("=== Posts.xml diagnostics ===")
    print("PostTypeId counts:", dict(counts))
    print(f"With Tags: {with_tags}  |  Without Tags: {without_tags}")
    print("Example rows (max", max_print, "):")
    for ex in examples:
        print("  ", ex)
    print("================================")

def build_hypergraph_from_posts(posts_xml_path: Path,
                                allowed_tags: set[str] | None,
                                min_tag_size: int = 1,
                                limit_questions: int = 0,
                                debug: bool = False):
    """
    Build the hypergraph from posts.xml.

    - Vertices: questions (PostTypeId=1), each assigned a 1-based integer ID.
    - Hyperedges: tags.
      For each tag T we collect the set of question vertex IDs that use T.

    Filtering:
      - Only PostTypeId=1 (questions) are used.
      - Synthetic posts are dropped (see is_synthetic_post).
      - If 'allowed_tags' is not None, tags are intersected with that set.
      - After construction, tags with < min_tag_size distinct vertices are dropped.
    """
    vertex_map: "OrderedDict[str, int]" = OrderedDict()  # question_id -> vertex_id
    next_vid = 1
    tag_to_vertices: dict[str, list[int]] = defaultdict(list)

    seen_q = 0
    printed = 0

    for _, elem in ET.iterparse(posts_xml_path, events=("end",)):
        a = elem.attrib
        post_type = a.get("PostTypeId") or a.get("posttypeid")
        if not post_type:
            elem.clear()
            continue
        if post_type != "1":  # 1 = question (official schema)
            elem.clear()
            continue

        qid = a.get("Id") or a.get("id")
        if is_synthetic_post(qid):
            elem.clear()
            continue

        tags_raw = a.get("Tags") or a.get("tags")
        tags = parse_tag_field(tags_raw)

        if allowed_tags is not None:
            tags = [t for t in tags if t in allowed_tags]
        if not tags:
            elem.clear()
            continue

        if qid not in vertex_map:
            vertex_map[qid] = next_vid
            next_vid += 1
        vid = vertex_map[qid]
        for t in tags:
            tag_to_vertices[t].append(vid)

        seen_q += 1
        if debug and printed < 5:
            print(f"[debug] qid={qid} -> tags={tags[:6]}{'...' if len(tags)>6 else ''}")
            printed += 1

        if limit_questions and seen_q >= limit_questions:
            elem.clear()
            break
        elem.clear()

    if min_tag_size > 1:
        tag_to_vertices = {
            t: vs for t, vs in tag_to_vertices.items()
            if len(set(vs)) >= min_tag_size
        }

    if debug:
        print(f"[debug] #vertices(questions)={len(vertex_map)}  "
              f"#hyperedges(tags)={len(tag_to_vertices)}")
    return vertex_map, tag_to_vertices

# -----------------------------
# Writing
# -----------------------------

def order_edges(tag_to_vertices: dict[str, list[int]], order: str):
    """
    Return (tag, vertices) pairs ordered by tag name or by hyperedge size.
    """
    if order == "by_size_desc":
        return sorted(tag_to_vertices.items(), key=lambda kv: (-len(set(kv[1])), kv[0]))
    return sorted(tag_to_vertices.items(), key=lambda kv: kv[0])

def write_hgr(out_path: Path,
              vertex_map: "OrderedDict[str, int]",
              tag_to_vertices: dict[str, list[int]],
              order: str = "by_tag") -> list[tuple[str, list[int]]]:
    """
    Write a Motivo-style hypergraph (.hgr) with a header line:
      <num_vertices> <num_hyperedges>
    followed by one line per hyperedge:
      v1 v2 v3 ...
    where v_i are 1-based vertex IDs, unique and sorted.
    """
    edges = order_edges(tag_to_vertices, order)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(f"{len(vertex_map)} {len(edges)}\n")
        for _, vids in edges:
            uniq = sorted(set(vids))
            f.write(" ".join(map(str, uniq)) + "\n")
    return edges

def write_vertex_map(path: Path,
                     vertex_map: "OrderedDict[str, int]",
                     site_domain: str):
    """
    Write a TSV mapping:
      vertex_id <TAB> question_id <TAB> URL
    """
    with open(path, "w", encoding="utf-8") as f:
        f.write("vertex_id\tquestion_id\turl\n")
        for qid, vid in vertex_map.items():
            f.write(f"{vid}\t{qid}\thttps://{site_domain}/questions/{qid}\n")

def write_hyperedge_map(path: Path,
                        ordered_edges: list[tuple[str, list[int]]]):
    """
    Write a TSV mapping:
      hyperedge_line_index <TAB> tag <TAB> size
    where 'hyperedge_line_index' matches the line number (1-based) in the .hgr file.
    """
    with open(path, "w", encoding="utf-8") as f:
        f.write("hyperedge_line_index\ttag\tsize\n")
        for i, (tag, vids) in enumerate(ordered_edges, start=1):
            f.write(f"{i}\t{tag}\t{len(set(vids))}\n")

def write_tag_txt(out_path: Path,
                  tag_to_vertices: dict[str, list[int]],
                  order: str = "by_tag"):
    """
    Write a text file with one line per tag:
      tag: v1 v2 v3 ...
    mainly for debugging / inspection.
    """
    edges = order_edges(tag_to_vertices, order)
    with open(out_path, "w", encoding="utf-8") as f:
        for tag, vids in edges:
            uniq = sorted(set(vids))
            f.write(f"{tag}: {' '.join(map(str, uniq))}\n")

def write_edges_txt(out_path: Path,
                    tag_to_vertices: dict[str, list[int]],
                    order: str = "by_tag"):
    """
    Write a plain hypergraph file with ONLY hyperedges:
      - one hyperedge per line
      - each line: 'v1 v2 v3 ...' with 1-based vertex IDs (unique, sorted)
      - no header, no tag names

    This format is directly compatible with the HyperMotivo pipeline
    (ASCII hypergraph: one hyperedge per line).
    """
    edges = order_edges(tag_to_vertices, order)
    with open(out_path, "w", encoding="utf-8") as f:
        for _, vids in edges:
            uniq = sorted(set(vids))
            if uniq:
                f.write(" ".join(map(str, uniq)) + "\n")
            else:
                f.write("\n")  # unlikely, but kept for safety

# -----------------------------
# CLI
# -----------------------------

def main():
    ap = argparse.ArgumentParser(
        description=(
            "Build a hypergraph from a StackExchange dump "
            "(vertices = questions, hyperedges = tags). "
            "Can output both a Motivo-style .hgr and a plain edges-only .txt "
            "for use with the HyperMotivo pipeline."
        )
    )
    ap.add_argument("--site", required=True,
                    help="Site domain (e.g., 'datascience.stackexchange.com').")
    ap.add_argument("--posts-url", default=None,
                    help="Explicit URL of the .7z dump (if you do not want automatic resolution).")
    ap.add_argument("--workdir", default=None,
                    help="Working directory for download/extraction (default: temporary dir).")
    ap.add_argument("--out", default="hypergraph.hgr",
                    help="Output .hgr hypergraph file (Motivo format).")
    ap.add_argument("--nodes-out", default="vertex_map.tsv",
                    help="(optional) TSV with node mapping: vertex_id,question_id,url.")
    ap.add_argument("--labels-out", default="hyperedge_map.tsv",
                    help="(optional) TSV with hyperedge metadata: line_index,tag,size.")
    ap.add_argument("--min-tag-size", type=int, default=1,
                    help="Discard hyperedges (tags) with fewer than N distinct vertices (default=1).")
    ap.add_argument("--limit-questions", type=int, default=0,
                    help="Process at most N questions (0 = no limit).")
    ap.add_argument("--filter-tags-file", default=None,
                    help="Text file with one tag per line; only these tags will be kept (comments starting with '#').")
    ap.add_argument("--order", choices=["by_tag", "by_size_desc"], default="by_tag",
                    help="Order hyperedges by tag name or by decreasing size.")
    ap.add_argument("--tag-txt", default=None,
                    help="Also write a 'tag: v1 v2 ...' debug file at this path.")
    ap.add_argument("--edges-txt", default=None,
                    help="Also write a plain edges-only TXT file: one hyperedge per line, 'v1 v2 ...' (no header).")
    ap.add_argument("--keep-temp", action="store_true",
                    help="Keep extracted XML files (do not delete temp files).")
    ap.add_argument("--debug", action="store_true",
                    help="Print debug information during parsing.")
    ap.add_argument("--diagnose", action="store_true",
                    help="Run a diagnostic pass on posts.xml (counts + example rows).")

    args = ap.parse_args()

    url = args.posts_url or resolve_dump_url(args.site)
    workdir = Path(args.workdir) if args.workdir else Path(tempfile.mkdtemp(prefix="se_dump_"))
    archive_path = workdir / "dump.7z"

    print(f"Site:    {args.site}")
    print(f"URL:     {url}")
    print(f"Workdir: {workdir}")

    try:
        if not archive_path.exists():
            download_file(url, archive_path)
        else:
            print(f"Reusing existing archive: {archive_path}")

        paths = extract_needed_xml(archive_path, workdir, wanted=("posts.xml", "tags.xml"))
        posts_xml = paths["posts.xml"]
        tags_xml = paths.get("tags.xml")

        if args.diagnose:
            diagnose_posts_xml(posts_xml)

        allowed = None
        if tags_xml and tags_xml.exists():
            tag_counts = read_tag_counts(tags_xml)
            print(f"Found {len(tag_counts)} tags in Tags.xml")
            if args.min_tag_size > 1:
                allowed = {t for t, c in tag_counts.items() if c >= args.min_tag_size}

        if args.filter_tags_file:
            file_tags = set()
            with open(args.filter_tags_file, "r", encoding="utf-8") as f:
                for line in f:
                    t = line.strip()
                    if t and not t.startswith("#"):
                        file_tags.add(t)
            allowed = (allowed & file_tags) if allowed is not None else file_tags
            print(
                f"Tag filter file: {len(file_tags)} tags listed; "
                f"allowed after intersection: {len(allowed) if allowed is not None else 'all'}"
            )

        print("Parsing posts.xml and building the hypergraph...")
        vertex_map, tag_to_vertices = build_hypergraph_from_posts(
            posts_xml_path=posts_xml,
            allowed_tags=allowed,
            min_tag_size=args.min_tag_size,
            limit_questions=args.limit_questions,
            debug=args.debug,
        )

        print(f"Vertices (questions): {len(vertex_map)}")
        print(f"Hyperedges (tags):   {len(tag_to_vertices)}")

        out_hgr_path = Path(args.out)
        ordered_edges = write_hgr(out_hgr_path, vertex_map, tag_to_vertices, order=args.order)
        print(f"Wrote .hgr hypergraph to: {args.out}")

        if args.nodes_out:
            write_vertex_map(Path(args.nodes_out), vertex_map, args.site)
            print(f"Wrote node mapping to: {args.nodes_out}")

        if args.labels_out:
            write_hyperedge_map(Path(args.labels_out), ordered_edges)
            print(f"Wrote hyperedge metadata to: {args.labels_out}")

        if args.tag_txt:
            write_tag_txt(Path(args.tag_txt), tag_to_vertices, order=args.order)
            print(f"Wrote tag-txt debug file to: {args.tag_txt}")

        if args.edges_txt:
            write_edges_txt(Path(args.edges_txt), tag_to_vertices, order=args.order)
            print(f"Wrote edges-only TXT file to: {args.edges_txt}")

        if not args.keep_temp:
            for k in ("posts.xml", "tags.xml"):
                p = paths.get(k)
                if p:
                    try:
                        p.unlink()
                    except Exception:
                        pass

        print("Done.")

    except requests.HTTPError as e:
        print(f"HTTP error during download: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(2)

if __name__ == "__main__":
    main()
