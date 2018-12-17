#ifndef CITATION_GRAPH_CITATION_GRAPH_H
#define CITATION_GRAPH_CITATION_GRAPH_H

#include <vector>
#include <memory>
#include <map>

template <class Publication>
class Node {
private:
    Publication publication;
    std::vector<Node *> parents;
    std::vector<size_t> positionInParent;
    std::vector<std::shared_ptr<Node>> children;
    std::vector<size_t> positionInChild;
};

template <class Publication>
class CitationGraph {
private:
    std::map<typename Publication::id_type, Node<Publication> *> nodes;
    const typename Publication::id_type rootId;
};

#endif //CITATION_GRAPH_CITATION_GRAPH_H
