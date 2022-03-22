#include "tree.hpp"
#include <ostream>
#include "primitives/utility.hpp"

Tree g_tree;

std::ostream& operator<<(std::ostream& os, const Node &n) {
    for (uint8_t i = 0; i < n.ply; ++i)
        os << "  ";

    os << "( "
       << n.played << ", " 
       << "[ " << n.alpha << ", " << n.beta << " ], "
       << n.score << ", " << int(n.depth) << ", " 
       << int(n.ply) << " )";
    return os;
}

void Tree::clear() { nodes.clear(); }

size_t Tree::begin_node(Move prev, int16_t alpha, int16_t beta, 
        uint8_t depth, uint8_t ply)
{
#ifdef TRACE
    nodes.push_back(Node{ prev, alpha, beta, 0, depth, 
            ply, NodeType::NonTerminal, 0 });
    return size() - 1;
#else
    (void)(prev);
    (void)(alpha);
    (void)(beta);
    (void)(depth);
    (void)(ply);
    return 0;
#endif
}

void Tree::end_node(size_t node_idx, int16_t score) {
#ifdef TRACE
    nodes[node_idx].subtree_size = static_cast<uint32_t>(
        size() - (node_idx + 1)
    );
    nodes[node_idx].score = score;
#else
    (void)(node_idx);
    (void)(score);
#endif
}

void Tree::set_last_type(NodeType ntp) {
#ifdef TRACE
    nodes.back().ntp = ntp;
#else
    (void)(ntp);
#endif
}

size_t Tree::size() const { return nodes.size(); }

const Node& Tree::root() const { return nodes[0]; }

size_t Tree::first_child(size_t node_idx) const {
    if (!nodes[node_idx].subtree_size)
        return npos;
    return node_idx + 1;
}

size_t Tree::next_child(size_t cur_idx) const {
    size_t next_idx = cur_idx + nodes[cur_idx].subtree_size + 1;

    if (next_idx >= size() 
            || nodes[next_idx].ply != nodes[cur_idx].ply)
        return npos;
    return next_idx;
}

size_t Tree::parent(size_t node_idx) const {
    if (node_idx >= size())
        return npos;

    size_t parent = npos;
    size_t idx = 0;

    while (idx != npos && idx != node_idx) {
        size_t last = idx + nodes[idx].subtree_size;
        if (node_idx <= last) {
            parent = idx;
            idx = first_child(parent);
            continue;
        }

        idx = next_child(idx);
    }

    return parent;
}

void Tree::pretty_print(std::ostream &os) const {
    if (nodes.empty())
        return;
    size_t idx = 0;
    while (idx != npos) {
        pretty_print(os, idx);
        idx = next_child(idx);
    }
}

void Tree::pretty_print(std::ostream &os, size_t parent) const {
    os << nodes[parent] << '\n';

    size_t child = first_child(parent);
    while (child != npos) {
        pretty_print(os, child);
        child = next_child(child);
    }
}

void Tree::json(std::ostream &os) const {
    if (nodes.empty()) {
        os << "[]\n";
        return;
    }

    os << '[';
    size_t idx = 0;
    while (true) {
        json(os, idx);
        idx = next_child(idx);

        if (idx == npos)
            break;
        os << ",\n";
    }
    os << "]\n";
}

void Tree::json(std::ostream &os, size_t parent) const {
    const Node &n = nodes[parent];
    os << "{\"move\": \"" << n.played << "\",\n"
       << "\"alpha\": " << n.alpha << ",\n"
       << "\"beta\": " << n.beta << ",\n"
       << "\"score\": " << n.score << ",\n"
       << "\"depth\": " << int(n.depth) << ",\n"
       << "\"ply\": " << int(n.ply) << ",\n"
       << "\"children\":";

    size_t child = first_child(parent);
    if (child == npos) {
        os << "[]}";
        return;
    }

    os << "[\n";
    while (true) {
        json(os, child);
        child = next_child(child);
        if (child == npos)
            break;
        os << ",\n";
    }

    os << "]}";
}

