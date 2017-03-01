#!/usr/bin/env python

import sys
import random

verts = {}
nverts=0
edges = {}

def get_or_add_vertex_id(u):
    global nverts
    
    if u not in verts:
        uid = nverts
        nverts += 1
        verts[u] = uid
        edges[uid] = set()
        return uid

    return verts[u]

argc = len(sys.argv)
if argc !=4 and argc != 5:
    print "Usage %s loe_graph output_graph output_map [seed]" % sys.argv[0]
    exit(0)

loe_graph, output_graph, output_map = sys.argv[1:4]

seed = 0
if argc == 5:
    seed = int(sys.argv[4])
    print "Using seed %d" % seed;
    random.seed(seed)

with open(loe_graph, "r") as f:
    for line in f:
        if line.startswith("#"):
                continue

        u,v = line.strip().split()
        u,v = u.strip(), v.strip()

        uid = get_or_add_vertex_id(u)
        vid = get_or_add_vertex_id(v)

        if uid==vid:
            continue

        edges[uid].add(vid)
        edges[vid].add(uid)

nedges = sum( [ len(edges[u]) for u in range(nverts) ] ) / 2
print "Loaded graph with %d vertices and %d edges" % (nverts, nedges)

#generate random permutation
newid = list(range(nverts))
oldid = list(range(nverts))

if seed!=0:
    print "Generating random permutation of vertices"

    #Knuth Shuffle
    for i in range(nverts-1):
        j = random.randint(i+1, nverts-1)
        newid[i],newid[j] = newid[j],newid[i]

    for i in range(nverts):
        oldid[newid[i]]=i

print "Writing output"

with open(output_graph, "w") as f:
    f.write("%d %d\n" % (nverts, nedges))
    for i in range(nverts):
        u = oldid[i]
        f.write("%d " % len(edges[u]))
        for v in sorted( [ newid[x] for x in edges[u] ]  ):
            f.write("%d " % v)
        f.write("\n")


with open(output_map, "w") as f:
    f.write("#Seed: %d\n#Format: oldname newname\n" % seed)
    for oldname, id in verts.iteritems():
        f.write("%s %d\n" % (oldname, newid[id]))
