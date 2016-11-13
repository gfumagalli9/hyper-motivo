#include <iostream>

#include "Graph.h"
#include "treelet.h"
#include "TreeletTable.h"

int main()
{
    Graph G("../../star.txt");

    std::cout << G.number_of_vertices() << " " << G.number_of_edges() << std::endl;

    Treelet::treelet_t t1 = Treelet::singleton(3);
    Treelet::treelet_t t2 = Treelet::singleton(6);

    std::cout << Treelet::is_mergeable(t1, t2) << std::endl;

    Treelet::treelet_t t = Treelet::merge(t1, t2);
    std::cout << t << std::endl;

    GraphColoring coloring(G.number_of_vertices(), 5);

    TreeletTable tbl1(&G, &coloring, 1, NULL);
    tbl1.fill_table();

    std::cout << "Table 1 filled" << std::endl;


    const TreeletTable* lower[] = {&tbl1};
    TreeletTable tbl2(&G, &coloring, 2, lower);
    tbl2.fill_table();

    std::cout << "Table 2 filled" << std::endl;

    const TreeletTable* lower2[] = {&tbl1, &tbl2};
    TreeletTable tbl3(&G, &coloring, 3, lower2);
    tbl3.fill_table();

    std::cout << "Table 3 filled" << std::endl;

    return 0;
}