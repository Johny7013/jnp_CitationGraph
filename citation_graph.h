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
};

class PublicationNotFound : public std::exception {
    virtual const char *what() const throw() {
        return "Publication not found.";
    }
};

class TriedToRemoveRoot : public std::exception {
    virtual const char *what() const throw() {
        return "Tried to remove root";
    }
};

template<class Publication>
class Node {
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    Publication publication;
    std::weak_ptr<Node<Publication>> thisNode;
    std::vector<std::weak_ptr<GNode>> parents;
    std::vector<size_t> positionInParent;
    std::vector<std::shared_ptr<GNode>> children;
    std::vector<size_t> positionInChild;

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

    std::vector<size_t> &getPositionsInParents() {
        return positionInParent;
    }

    bool citationExists(const std::shared_ptr<GNode> &parent) {
        if (parents.empty()) {
            return false;
        }

        bool exists = false;

        for (size_t i = parents.size(); i > 0; i--) {
            if (parents[i - 1].lock().get() == parent.get()) {
                exists = true;
            }
            else if (parents[i - 1].expired()) { // remove expired pointers
                if (parents.size() != i) {
                    std::swap(parents[i], parents[parents.size()]);
                    std::swap(positionInParent[i], positionInParent[parents.size()]);
                }
                parents.pop_back();
                positionInParent.pop_back();
            }
        }

        return exists;
    }

    void reverseChanges(const std::shared_ptr<GNode> &parent, size_t position) {
        if (!parents.empty()) {
            if (parents[parents.size() - 1].lock().get() == parent.get()) {
                parents.pop_back();

                if (positionInParent[positionInParent.size() - 1] == position) {
                    positionInParent.pop_back();
                }
            }
        }
    }

    auto &getParents(){
        return parents;
    }

    auto &getChildren(){
        return children;
    }

    void removeChild(size_t position) {
        children[position].swap(children[children.size() - 1]);
        children.pop_back();
    }

    void remove() {
        for (int i = 0; i < parents.size(); i++) {
            if (!parents[i].expired()) {
                auto parentPtr = parents[i].lock();
                parentPtr->removeChild(positionInParent[i]);
            }
        }
    }
};

template<class Publication>
class CitationGraph {
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    using NodesMap = std::map<PublId, std::weak_ptr<GNode>>;
    std::unique_ptr<NodesMap> nodes;
    std::shared_ptr<GNode> root;

    std::weak_ptr<GNode> &getPointerToNode(PublId const &id) const {
        return nodes->find(id)->second;
    }

    void eraseNodeIfExpired(typename NodesMap::iterator node) {
        if (node != nodes->end() && node->second.expired()) {
            nodes->erase(node);
        }
    }

    bool idIsOccupied(PublId const &id) {
        eraseNodeIfExpired(nodes->find(id));

        return nodes->find(id) != nodes->end();
    }

public:
    explicit CitationGraph(PublId const &stem_id)
            : nodes(std::make_unique<NodesMap>()),
              root(std::make_shared<GNode>(stem_id)) {
        root->setThisNodePointer(root);
        (*nodes)[stem_id] = root;
    }

    CitationGraph(CitationGraph<Publication> &) = delete;

    CitationGraph(CitationGraph<Publication> &&other) noexcept
            : nodes(nullptr), root(nullptr) {
        nodes.swap(other.nodes);
        root.swap(other.root);
    }

    CitationGraph<Publication> &operator=(CitationGraph<Publication> &&other) noexcept {
        if (this != &other) {
            nodes.swap(other.nodes);
            root.swap(other.root);
        }

        return *this;
    }

    PublId get_root_id() const noexcept(noexcept(root->getPublication().get_id())) {
        return root->getPublication().get_id();
    }

    bool exists(PublId const &id) const {
        typename NodesMap::iterator node = nodes->find(id);

        return (node != nodes->end() && !node->second.expired());
    }

    Publication &operator[](PublId const &id) const {
        if (!exists(id)) {
            throw PublicationNotFound();
        }
        return getPointerToNode(id).lock()->getPublication();
    }

    void create(PublId const &id, PublId const &parent_id) {
        if (idIsOccupied(id)) throw PublicationAlreadyCreated();
        if (!idIsOccupied(parent_id)) throw PublicationNotFound();

        auto parentPtr = getPointerToNode(parent_id).lock();
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

        if (idIsOccupied(id)) throw PublicationAlreadyCreated();

        std::unordered_set<std::shared_ptr<GNode>> parentsSet;
        std::vector<std::shared_ptr<GNode>> parents;
        for (int i = 0; i < parent_ids.size(); i++) {
            if (!idIsOccupied(parent_ids[i])) throw PublicationNotFound();
            auto parentPtr = getPointerToNode(parent_ids[i]).lock();
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

    std::vector<PublId> get_children(PublId const &id) const {
        if (!exists(id)) {
            throw PublicationNotFound();
        }

        std::vector<PublId> children;

        auto pointersToChildren = getPointerToNode(id).lock()->getChildren();

        for (auto parentNode: pointersToChildren) {
            children.emplace_back(parentNode->getPublication().get_id());
        }

        return children;
    }

    std::vector<PublId> get_parents(PublId const &id) const {
        if (!exists(id)) {
            throw PublicationNotFound();
        }

        std::vector<PublId> parents;

        auto pointersToParents = getPointerToNode(id).lock()->getParents();

        for (auto parentNode: pointersToParents) {
            if (!parentNode.expired()) {
                parents.emplace_back(parentNode.lock()->getPublication().get_id());
            }
        }

        return parents;
    }


    void add_citation(PublId const &child_id, PublId const &parent_id) {

        if (!idIsOccupied(child_id) || !idIsOccupied(parent_id)) {
            throw PublicationNotFound();
        }

        auto childPtr = getPointerToNode(child_id).lock();
        auto parentPtr = getPointerToNode(parent_id).lock();
        auto position = parentPtr->howManyChildren();

        try {
            if (!childPtr->citationExists(parentPtr)) {
                childPtr->addParent(parentPtr, position);
                parentPtr->addExistingChild(childPtr);
            }
        } catch (...) {
            childPtr->reverseChanges(parentPtr, position);
            throw;
        }
    }

    void remove(PublId const &id) {
        if (id == get_root_id()) throw TriedToRemoveRoot();
        if (!idIsOccupied(id)) throw PublicationNotFound();

        auto nodeIt = nodes->find(id);
        std::shared_ptr<GNode> nodePtr = nodeIt->second.lock();

        nodes->erase(nodeIt);
        nodePtr->remove();
    }

};

#endif //CITATION_GRAPH_CITATION_GRAPH_H
