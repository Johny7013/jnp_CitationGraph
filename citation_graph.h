#ifndef CITATION_GRAPH_CITATION_GRAPH_H
#define CITATION_GRAPH_CITATION_GRAPH_H

#include <vector>
#include <memory>
#include <map>

class PublicationAlreadyCreated: public std::exception
{
    virtual const char* what() const throw()
    {
        return "Publication already created.";
    }
} pAlreadyCreated;

class PublicationNotFound: public std::exception
{
    virtual const char* what() const throw()
    {
        return "Publication not found.";
    }
} pNotFound;

class TriedToRemoveRoot: public std::exception
{
    virtual const char* what() const throw()
    {
        return "Tried to remove root";
    }
}triedToRemoveRoot;

template<class Publication>
class Node {
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    Publication publication;
    std::weak_ptr<Node<Publication>> thisNode;
    std::vector<std::weak_ptr<GNode>> parents;
    std::vector<size_t> positionInParent;
    std::vector<std::shared_ptr<GNode>> children;

public:
    explicit Node(const typename Publication::id_type &id) : publication(id) {}

    void setThisNodePointer(std::shared_ptr<Node<Publication>> &ptr) {
        thisNode = ptr;
    }

    Publication &getPublication() noexcept {
        return publication;
    }

    auto &getParents(){
        return parents;
    }

    auto &getChildren(){
        return children;
    }
};

template<class Publication>
class CitationGraph {
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    using NodesMap = std::map<PublId, std::weak_ptr<GNode>>;
    std::unique_ptr<NodesMap> nodes;
    std::unique_ptr<PublId> rootId;
    std::shared_ptr<GNode> root;

    std::weak_ptr<GNode> &getPointerToNode(typename Publication::id_type const &id) const{
        return nodes->find(id)->second;
    }

public:
    explicit CitationGraph(PublId const &stem_id)
            : nodes(std::make_unique<NodesMap>()),
            rootId(std::make_unique<PublId>(stem_id)),
            root(std::make_shared<GNode>(stem_id)) {
        //nodes.swap(std::make_unique<NodesMap>());
        //rootId.swap(std::make_unique<PublId>(stem_id));
        //root.swap(std::make_shared<GNode>(stem_id));
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

    PublId get_root_id() const noexcept(noexcept(root->getPublication().get_id())) {
        return root->getPublication().get_id();
    }

    bool exists(PublId const &id) const {
        return nodes->find(id) != nodes->end();
    }

    std::vector<typename Publication::id_type> get_children(typename Publication::id_type const &id) const {
        if (!exists(id)) {
            throw pNotFound;
        }
        else {
            std::vector<typename Publication::id_type> children;

            auto pointersToChildren = getPointerToNode(id).lock()->getChildrean();

            for (auto parentNode: pointersToChildren) {
                children.emplace_back(parentNode.lock()->getPublication().get_id());
            }

            return children;
        }
    }

    std::vector<typename Publication::id_type> get_parents(typename Publication::id_type const &id) const {
        if (!exists(id)) {
            throw pNotFound;
        }
        else {
            std::vector<typename Publication::id_type> parents;

            auto pointersToParents = getPointerToNode(id).lock()->getParents();

            for (auto parentNode: pointersToParents) {
                parents.emplace_back(parentNode.lock()->getPublication().get_id());
            }

            return parents;
        }
    }

};

#endif //CITATION_GRAPH_CITATION_GRAPH_H
