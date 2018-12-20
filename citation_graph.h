#ifndef CITATION_GRAPH_CITATION_GRAPH_H
#define CITATION_GRAPH_CITATION_GRAPH_H

#include <vector>
#include <memory>
#include <map>
#include <unordered_set>

class PublicationAlreadyCreated : public std::exception {
    virtual const char *what() const throw() {
        return "Publication already created.";
    }
} pAlreadyCreated;

class PublicationNotFound : public std::exception {
    virtual const char *what() const throw() {
        return "Publication not found.";
    }
} pNotFound;

class TriedToRemoveRoot : public std::exception {
    virtual const char *what() const throw() {
        return "Tried to remove root";
    }
} triedToRemoveRoot;

template<class Publication>
class Node {
private:
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
public:
    explicit Node(const PublId &id) : publication(id) {}

    Node(const PublId &id, std::weak_ptr<GNode> parent, size_t index)
            : publication(id),
              parents(),
              positionInParent() {
        parents.push_back(parent);
        positionInParent.push_back(index);
    }

    void setThisNodePointer(const std::shared_ptr<GNode> &ptr) {
        thisNode = ptr;
    }

    Publication &getPublication() noexcept {
        return publication;
    }

    size_t howManyChildren() noexcept {
        return children.size();
    }

    void removeChildImmediately() {
        children.pop_back();
    }

    void addExistingChild(const std::shared_ptr<GNode> &child) {
        children.push_back(child);
    }

    std::weak_ptr<GNode> addNewChild(const PublId &id) {
        auto child = std::make_shared<GNode>(id, thisNode, children.size());
        child->setThisNodePointer(child);
        children.push_back(child);
        return std::weak_ptr(child);
    }

    std::weak_ptr<GNode> addNewChild(const PublId &id, const std::vector<std::shared_ptr<GNode>> &parents) {
        auto child = std::make_shared<GNode>(id, thisNode, children.size());
        child->setThisNodePointer(child);
        children.push_back(child);

        int i = 1;
        try {
            for (; i < parents.size(); i++) {
                size_t position = parents[i]->howManyChildren();
                child->addParent(parents[i], position);
                parents[i]->addExistingChild(child);
            }
        } catch (...) {
            for (int j = 0; j < i; j++) {
                parents[j]->removeChildImmediately();
            }
            throw;
        }

        return std::weak_ptr(child);
    }

    void addParent(const std::shared_ptr<GNode> &parent, size_t position) {
        parents.push_back(std::weak_ptr(parent));
        positionInParent.push_back(position);
    }

private:
    Publication publication;
    std::weak_ptr<GNode> thisNode;
    std::vector<std::weak_ptr<GNode>> parents;
    std::vector<size_t> positionInParent;
    std::vector<std::shared_ptr<GNode>> children;
};

template<class Publication>
class CitationGraph {
private:
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    using NodesMap = std::map<PublId, std::weak_ptr<GNode>>;
    std::unique_ptr<NodesMap> nodes;
    std::unique_ptr<PublId> rootId;
    std::shared_ptr<GNode> root;
public:
    explicit CitationGraph(PublId const &stem_id)
            : nodes(std::make_unique<NodesMap>()),
              rootId(std::make_unique<PublId>(stem_id)),
              root(std::make_shared<GNode>(stem_id)) {
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

    Publication &operator[](PublId const &id) const {
        auto publ = nodes->find(id);
        if (publ == nodes->end()) {
            throw PublicationNotFound();
        }
        return *publ;
    }

    void create(PublId const &id, PublId const &parent_id) {
        auto newPtrIt = nodes->find(id);
        if (newPtrIt != nodes->end()) throw PublicationAlreadyCreated();
        auto parentPtrIt = nodes->find(parent_id);
        if (parentPtrIt == nodes->end()) throw PublicationNotFound();

        auto parentPtr = parentPtrIt->second.lock();
        auto newPtr = parentPtr->addNewChild(id);

        try {
            nodes->insert(std::make_pair(id, newPtr));
        } catch (...) {
            parentPtr->removeChildImmediately();
            throw;
        }
    }

    void create(PublId const &id, std::vector<PublId> const &parent_ids) {
        if (parent_ids.size() <= 0) return;

        auto newPtrIt = nodes->find(id);
        if (newPtrIt != nodes->end()) throw PublicationAlreadyCreated();

        std::unordered_set<std::shared_ptr<GNode>> parentsSet;
        std::vector<std::shared_ptr<GNode>> parents;
        for (int i = 0; i < parent_ids.size(); i++) {
            auto parentPtrIt = nodes->find(parent_ids[i]);
            if (parentPtrIt == nodes->end()) throw PublicationNotFound();
            auto parentPtr = parentPtrIt->second.lock();
            if (parentsSet.find(parentPtr) == parentsSet.end()) {
                parents.push_back(parentPtr);
                parentsSet.insert(parentPtr);
            }
        }

        auto newPtr = parents[0]->addNewChild(id, parents);

        try {
            nodes->insert(std::make_pair(id, newPtr));
        } catch (...) {
            for (int i = 0; i < parents.size(); i++) {
                parents[i]->removeChildImmediately();
            }
            throw;
        }
    }
};

#endif //CITATION_GRAPH_CITATION_GRAPH_H
