#!/usr/bin/env python3

"""
MOTIVO companion module (see https://bitbucket.org/steven_/motivo/).
It contains tools to manipulate motifs (aka graphlets) and counts.
For example, you can plot the graphlets returned by MOTIVO in its CSV file.
To do so, simply invoke this script with no option, or use
"""

import itertools
import numpy as np

class MatrixSizeMismatchError(Exception):
    pass

class Graphlet:
    """
    A graphlet (that is, just a small graph).
    You can create a graphlet from:
        (1) the adjacency list 
        (2) the adjacency matrix
        (3) the edge list
        (4) MOTIVO's string signature (example: "ADMAAAA" for the 5-star)
    Once created, you can convert a graphlet to any of these representations.
    You can also plot a graphlet and save it to file.
    """

    # Layout for graphlet drawing (see NetworkX's documentation).
    import networkx as nx
    graphlet_layout = nx.circular_layout

    def __init__(self, x):
        if type(x) == str:
            self.__A__ = signature_to_matrix(x)
            self.__signature__ = x
            self.__k__ = self.__A__.shape[0]
        else:
            if x.shape[0] != x.shape[1]:
                raise MatrixSizeMismatchError()
            self.__A__ = np.abs(np.sign(x))
            self.__signature__ = matrix_to_signature(self.__A__)
            self.__k__ = self.__A__.shape[0]

    def star(k):
        """For convenience."""
        A = np.zeros((k,k), dtype=int)
        A[-1,:-1] = 1
        A[:-1,-1] = 1
        return Graphlet(A)

    def clique(k):
        """For convenience."""
        A = np.ones((k,k), dtype=int) - np.eye(k, dtype=int)
        return Graphlet(A)

    def A(self):
        """The adjacency matrix of the graphlet."""
        return self.__A__.copy()

    def signature(self):
        """MOTIVO's string signature of the graphlet."""
        return self.__signature__[:]

    def average_degree(self):
        """Average degree of the graphlet."""
        return self.__A__.sum()/self.__k__

    def density(self):
        """Edge density of the graphlet."""
        return self.__A__.sum()/(self.__k__*(self.__k__-1))

    def n(self):
        """Number of nodes in the graphlet."""
        return self.__k__

    def m(self):
        """Number of edges in the graphlet."""
        return self.__A__.sum()//2

    def adj_list(self):
        """Adjacency list of the graphlet."""
        L = [np.where(self.A()[u] > 0)[0] for u in range(self.n())]
        return L

    def edge_list(self):
        """Edge list of the graphlet."""
        return np.argwhere(self.A() > 0)

    def num_spanning_trees(self):
        """Number of spanning trees."""
        self.__spt__ = num_spanning_trees(self.A())
        return self.__spt__

    def draw(self, fname=None):
        """
        Draw the graphlet and save it to the specified filename.
        The filename itself specifies the format, as it is passed
            to matplotlib.plt.savefig().
        """
        import networkx as nx
        import matplotlib.pyplot as plt
        plt.clf()
        G = nx.Graph()
        G.add_edges_from(self.edge_list())
        ly = Graphlet.graphlet_layout(G, scale=.1)
        nx.draw(G, ly, width=2, node_size=1500)
        cut = 1.2
        xmax= cut*max(xx for xx,yy in ly.values())
        ymax= cut*max(yy for xx,yy in ly.values())
        xmin= cut*min(xx for xx,yy in ly.values())
        ymin= cut*min(yy for xx,yy in ly.values())
        plt.xlim(xmin,xmax)
        plt.ylim(ymin,ymax)
        if fname is not None:
            plt.gca().set_aspect('equal')
            plt.axis("off")
            plt.savefig(fname, bbox_inches = 'tight', pad_inches = 0)
            plt.close()

def matrix_to_signature(A, padding=30):
    """
    Convert an adjacency matrix in MOTIVO's string representation.
    Optionally, pad to a given number of characters (by default 30).
    """
    b = ''
    for i in range(A.shape[0]):
        for j in range(i):
            b = b + str(A[i][j])
    b += '0' * (4 - len(b) % 4)
    ords = [ord('A') + int(b[4*i:4*(i+1)], 2) for i in range(len(b) // 4)]
    return "".join([chr(o) for o in ords]) + 'A' * (max(0, padding - len(ords)))


def signature_to_matrix(s):
    """
    Convert from MOTIVO's string representation to an adjacency matrix.
    For example, 'ADMAAAAAAAAAAAAAAAAAAAAAAAAAAA' is converted to the adjacency matrix of the 5-star.
    """
    bits = [ord(c) - ord('A') for c in s]
    last_nonzero = 0
    L = ""
    for b in bits:
        bb = bin(b)[2:]
        bb = '0'*(4 - len(bb)) + bb
        L = L + bb
    L = "".join(L)
    nbits = len(L) - max(0, L[::-1].find('1'))
    k = 2
    while k * (k-1) < 2 * nbits:
        k += 1
    A = np.zeros((k, k), dtype=int)
    idx = 0
    for i in range(k):
        for j in range(i):
            A[i, j] = L[idx]
            idx += 1
    return A + A.T


def num_spanning_trees(A):
    """
    Compute the number of spanning trees in an undirected graph
    """
    from scipy.sparse import csgraph
    L = csgraph.laplacian(A)
    return np.linalg.det(L[1:, 1:])


def complete_graph_matrix(n):
    """For convenience."""
    return np.ones((n,n), dtype=int) - np.eye(n, dtype=int)


def num_spanning_paths(A):
    """
    Compute the number of spanning paths by brute-force.
    """    
    p = 0
    for L in itertools.permutations(range(A.shape[0])):
        p += np.array([A[L[i], L[i+1]] for i in range(A.shape[0]-1)]).all()
    return p // 2



def plot_graphlets(csv_file, fmt="pdf"):
        with open(sys.argv[2]) as csv_file:
            csv_reader = csv.reader(csv_file, delimiter=',')
            line_count = 0
            for row in csv_reader:
                if line_count > 0 and len(row) > 0:
                    motif = row[0]
#                    print(motif)
                    Graphlet(motif).draw(motif+"."+fmt)
                line_count += 1


if __name__ == '__main__':
    import sys
    import csv
    if len(sys.argv) > 2 and sys.argv[1] == 'plotmotif':
        fmt = sys.argv[3] if (len(sys.argv) > 3) else "pdf"
        print("Plotting from", sys.argv[2], "in", fmt, "format")
        plot_graphlets(sys.argv[2], fmt)
    else:
        print("Usage: " +
              sys.argv[0] +
              " plotmotif counts.csv [FORMAT], where:\n"
              "\tcounts.csv is a file produced by motivo.sh\n"
              "\tFORMAT defaults to 'pdf' (can be: 'png', 'jpeg', etc; see matplotlib.pyplot.savefig)"
              "\nThe script writes one figure for each motif found in the csv file, in the specified format."
              "\n\nExample: " + sys.argv[0] + " plotmotif mygraph-k5.csv"
              " can produce:\nADMAAAAAAAAAAAAAAAAAAAAAAAAAAA.pdf, AHEAAAAAAAAAAAAAAAAAAAAAAAAAAA.pdf, ..."
             )
