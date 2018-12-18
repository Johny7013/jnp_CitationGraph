#ifndef CITATION_GRAPH_CITATION_GRAPH_H
#define CITATION_GRAPH_CITATION_GRAPH_H

#include <vector>
#include <memory>
#include <map>

template<class Publication>
class Node {
public:
    explicit Node(const typename Publication::id_type &id) : publication(id) {}

    void setThisNodePointer(std::shared_ptr<Node<Publication>> &ptr) {
        thisNode = ptr;
    }

private:
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    Publication publication;
    std::weak_ptr<Node<Publication>> thisNode;
    std::vector<std::weak_ptr<GNode>> parents;
    std::vector<size_t> positionInParent;
    std::vector<std::shared_ptr<GNode>> children;
};

template<class Publication>
class CitationGraph {
public:
    explicit CitationGraph(typename Publication::id_type const &stem_id)
            : nodes(nullptr), rootId(nullptr), root(nullptr) {
        nodes.swap(std::make_unique<NodesMap>());
        rootId.swap(std::make_unique<PublId>(stem_id));
        root.swap(std::make_shared<GNode>(stem_id));
        root->setThisNodePointer(root);
        (*nodes)[stem_id] = root;
    }

    CitationGraph(CitationGraph<Publication> &) = delete;

    CitationGraph(CitationGraph<Publication> &&other) noexcept
            : nodes(nullptr), rootId(nullptr), root(nullptr) {
        nodes.swap(other.nodes);
        rootId.swap(other.rootId);
        root.swap(other.root);
    }

    CitationGraph<Publication> &operator=(CitationGraph<Publication> &&other) noexcept {
        if (this != &other) {
            nodes.swap(other.nodes);
            rootId.swap(other.rootId);
            root.swap(other.root);
        }

        return *this;
    }

private:
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    using NodesMap = std::map<PublId, std::weak_ptr<GNode>>;
    std::unique_ptr<NodesMap> nodes;
    std::unique_ptr<PublId> rootId;
    std::shared_ptr<GNode> root;
};

#endif //CITATION_GRAPH_CITATION_GRAPH_H
