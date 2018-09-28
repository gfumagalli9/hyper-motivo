#!/usr/bin/env python3
import pandas as pd
import sys

dfs = [pd.read_csv(f).set_index("motif") for f in sys.argv[2:]]
d = dfs[0]
for df in dfs[1:]:
    d = d + df
d.estim_occur /= len(dfs)
d.estim_freq /= len(dfs)
d.sort_values(by="estim_occur", ascending=False, inplace=True)
d.to_csv(sys.argv[1])
