#ifndef TREE_HPP
#define TREE_HPP

//#define TRACE

#include "primitives/common.hpp"
#include <vector>
#include <iosfwd>
#include <limits>

enum class NodeType : uint8_t {
    NonTerminal,
    Leaf,
    NoReplies,

    TTExact,
    TTLower,
    TTUpper,

    Null,
    Razoring,
    Futility,
    ReverseFutility,
};

struct Node {
    Move played;
    int16_t alpha, beta;
    int16_t score;
    uint8_t depth;
    uint8_t ply;
    NodeType ntp;

    uint32_t subtree_size;
};


/*
 * The nodes are added according to the order of DFS (alpha_beta)
 * Using this invariant, we can store them this way:
 * [(root), (child_1), [subtree of child1], 
 *      (child_2), [subtree of child_2], ..., etc]
 * Or, more generally (ss stands for (s)ubtree (s)ize):
 * [(1), [(1.1), ..., (1.ss)], (2), [(2.1), ..., (2.ss)], ..., 
 *      (n), [(n.1), ..., (n.ss)]]
 * */
struct Tree {
    //maybe deque?
    std::vector<Node> nodes;
    static constexpr size_t npos = 
        std::numeric_limits<size_t>::max();

    void clear();

    size_t begin_node(Move played, int16_t alpha, int16_t beta, 
        uint8_t depth, uint8_t ply);

    void end_node(size_t node_idx, int16_t score);

    void set_last_type(NodeType ntp);

    size_t size() const;

    const Node& root() const;
    
    size_t first_child(size_t node_idx) const;
    size_t next_child(size_t cur_idx) const;

    //Walks the tree from the root
    size_t parent(size_t node_idx) const;

    void pretty_print(std::ostream &os) const;
    void pretty_print(std::ostream &os, size_t parent) const;

    void json(std::ostream &os) const;
    void json(std::ostream &os, size_t parent) const;
};

extern Tree g_tree;

std::ostream& operator<<(std::ostream& os, const Node &n);

#endif
