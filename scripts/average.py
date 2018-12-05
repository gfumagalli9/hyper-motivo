#!/usr/bin/env python3
# Averages (the count tables of) several runs of Motivo 
import pandas as pd
import sys

dfs = [pd.read_csv(f).set_index("motif") for f in sys.argv[2:]]
d = dfs[0]
sptrees = {}
sptrees.update(d.spanning_trees.to_dict())
for df in dfs[1:]:
    d = d.add(df, fill_value=0)
    sptrees.update(df.spanning_trees.to_dict())
r = len(dfs)
d.estim_occur //= r
d.estim_freq /= r
d.spanning_trees = d.index.to_series().map(sptrees)
d.sort_values(by="estim_occur", ascending=False, inplace=True)
d.to_csv(sys.argv[1], float_format='%.5g')
