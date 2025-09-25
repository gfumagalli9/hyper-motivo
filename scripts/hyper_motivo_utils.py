#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, csv, os, binascii, hashlib, sys
from typing import Tuple, List, Optional
import numpy as np
import matplotlib.pyplot as plt
import hypernetx as hnx
import math

# ----------------------- decoding utils -----------------------

def _hex_to_bytes(s: str) -> bytes:
    s = s.strip().replace(" ", "").replace("0x","").replace("0X","")
    if len(s) % 2:
        raise ValueError("Odd-length hex")
    return binascii.unhexlify(s.encode("ascii"))

def decode_hyper_motif(sig_hex: str) -> Tuple[int, int, np.ndarray]:
    """
    Motivo hyper signature layout:
      [k:2B][b:2B][k*b bit row-major, MSB-first]
    Returns (k, b, M) where M is uint8 in {0,1} with shape (k,b).
    """
    raw = _hex_to_bytes(sig_hex)
    if len(raw) < 4:
        raise ValueError("Signature too short")
    k = (raw[0] << 8) | raw[1]
    b = (raw[2] << 8) | raw[3]
    bits = k * b
    payload = raw[4:]
    need = (bits + 7)//8
    if len(payload) < need:
        raise ValueError(f"Payload too short: {len(payload)} < {need}")
    M = np.zeros((k, b), dtype=np.uint8)
    for bitpos in range(bits):
        byte = payload[bitpos >> 3]
        off  = bitpos & 7
        if byte & (0x80 >> off):
            r = bitpos // b
            c = bitpos %  b
            M[r, c] = 1
    return k, b, M

def short_name(sig_hex: str) -> str:
    raw = _hex_to_bytes(sig_hex)
    if len(raw) < 4:
        return "invalid"
    k = (raw[0]<<8)|raw[1]
    b = (raw[2]<<8)|raw[3]
    h = hashlib.blake2b(raw[4:], digest_size=8).hexdigest()
    return f"{k:04X}{b:04X}{h}"

# ----------------------- I/O helpers --------------------------

def parse_vertices_field(s: Optional[str], expected_k: int) -> Optional[List[str]]:
    """Parse 'vertices' column; returns None if absent or wrong length."""
    if not s:
        return None
    toks = [t for t in s.replace(",", " ").split() if t]
    return toks if len(toks)==expected_k else None

def parse_int_field(s: Optional[str]) -> Optional[int]:
    if s is None or s.strip() == "":
        return None
    try:
        return int(s)
    except ValueError:
        return None

# ----------------------- drawing (HyperNetX) ------------------

def draw_hypergraph_hnx(
    M: np.ndarray,
    labels: Optional[List[str]],
    outpath: str,
    fmt: str = "pdf",
    samples: Optional[int] = None
):
    """
    Draw a hypergraph given its incidence matrix M (k x m).
    If labels is None, use '0'..'k-1'.
    Saves to f"{outpath}.{fmt}".
    Shows an annotation with the number of samples, if provided.
    """
    k, m = (int(M.shape[0]), int(M.shape[1])) if M.ndim==2 else (0, 0)
    if labels is None:
        labels = [str(i) for i in range(k)]
    else:
        labels = [str(x) for x in labels]

    # Build hyperedges from columns (only those with size >= 2)
    E = {}
    for j in range(m):
        everts = [labels[i] for i in range(k) if M[i, j] != 0]
        if len(everts) >= 2:
            E[f"e{j}"] = everts

    fig, ax = plt.subplots(figsize=(4.8, 4.8))

    # Handle edge-less motif (no valid columns): draw just the nodes
    if not E:
        pos = {}
        R = 1.0 if k else 0.0
        for i, v in enumerate(labels):
            ang = 2 * math.pi * i / max(1, k)
            pos[v] = (R * math.cos(ang), R * math.sin(ang))
        if k:
            xs, ys = zip(*[pos[v] for v in labels])
            ax.scatter(xs, ys, s=300, edgecolors="black", facecolors="white", linewidths=1.5)
            for v, (x, y) in pos.items():
                ax.text(x, y, v, fontsize=9, ha="center", va="center")
    else:
        H = hnx.Hypergraph(E)

        # Circular node layout
        pos = {}
        R = 1.0
        for i, v in enumerate(labels):
            ang = 2 * math.pi * i / max(1, k)
            pos[v] = (R * math.cos(ang), R * math.sin(ang))

        # Keep kwargs minimal/portable across hnx versions
        hnx.draw(H, pos=pos, ax=ax)

        # Node labels via matplotlib (portable)
        for v, (x, y) in pos.items():
            ax.text(x, y, v, fontsize=9, ha="center", va="center")

    # Annotation: number of samples (top-left, inside axes)
    if samples is not None:
        ax.text(
            0.02, 0.98, f"samples: {samples}",
            transform=ax.transAxes,
            ha="left", va="top", fontsize=10,
            bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="none", alpha=0.8)
        )

    ax.set_aspect("equal")
    ax.axis("off")
    fig.tight_layout(pad=0.05)
    fig.savefig(f"{outpath}.{fmt}", bbox_inches="tight", pad_inches=0, dpi=300)
    plt.close(fig)

# ----------------------- runner --------------------------------

def run(csv_path: str, outdir: str, fmt: str, limit: Optional[int]):
    os.makedirs(outdir, exist_ok=True)
    n = 0
    with open(csv_path, newline="") as f:
        # 1) ignora gli spazi subito dopo la virgola nell'header
        r = csv.DictReader(f, skipinitialspace=True)

        # controllo di base (dopo lo strip)
        fieldnames = [ (fn or "").strip() for fn in (r.fieldnames or []) ]
        if "hyper_motif" not in fieldnames:
            raise RuntimeError(f"CSV must contain a 'hyper_motif' column; got {fieldnames}")

        for idx, raw_row in enumerate(r, start=1):
            # 2) normalizza chiavi e valori della riga
            row = { (k or "").strip(): (v or "").strip() for k, v in raw_row.items() }

            sig = row.get("hyper_motif", "")
            if not sig:
                continue

            try:
                k, b, M = decode_hyper_motif(sig)
            except Exception as e:
                print(f"[warn] row {idx}: {e}", file=sys.stderr)
                continue

            # ora queste funzionano anche se nel CSV c'è ' vertices' / ' samples'
            # labels  = parse_vertices_field(row.get("vertices"), k) Se voglio avere le etichette di vertices
            labels = None
            samples = parse_int_field(row.get("samples"))

            base = f"{idx:05d}_{short_name(sig)}"
            out  = os.path.join(outdir, base)

            draw_hypergraph_hnx(M, labels, out, fmt, samples=samples)
            n += 1
            if limit and n >= limit:
                break

    print(f"Wrote {n} hypergraphlet figure(s) to {outdir}")

# ----------------------- cli -----------------------------------

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Plot hypergraphlets as hypergraphs (HyperNetX) with sample counts")
    ap.add_argument("csv", help="Input CSV from Motivo hyper sampler")
    ap.add_argument("--outdir", default="hyperplots", help="Output directory")
    ap.add_argument("--fmt", default="pdf", help="File format (pdf/png/...)")
    ap.add_argument("--limit", type=int, default=None, help="Stop after N rows")
    args = ap.parse_args()
    run(args.csv, args.outdir, args.fmt, args.limit)