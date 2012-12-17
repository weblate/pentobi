//-----------------------------------------------------------------------------
/** @file libpentobi_mcts/Util.h */
//-----------------------------------------------------------------------------

#ifndef LIBPENTOBI_MCTS_UTIL_H
#define LIBPENTOBI_MCTS_UTIL_H

#include "Search.h"

namespace libpentobi_mcts {
namespace util {

using namespace std;
using libboardgame_mcts::Node;

//-----------------------------------------------------------------------------

/** Comparison function for sorting children of a node by count.
    Prefers nodes with higher counts. Uses the node value as a tie breaker. */
bool compare_node(const Node<Move>* n1, const Node<Move>* n2);

/** Dump the search tree in SGF format. */
void dump_tree(ostream& out, const Search& search);

/** Suggest how much memory to use for the trees depending on the total
    physical memory available on the system. */
size_t get_memory();

/** Suggest how many threads to use in the search depending on the current
    system. */
unsigned get_nu_threads();

//-----------------------------------------------------------------------------

} // namespace util
} // namespace libpentobi_mcts

#endif // LIBPENTOBI_MCTS_UTIL_H
