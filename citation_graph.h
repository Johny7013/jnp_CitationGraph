#ifndef CITATION_GRAPH_CITATION_GRAPH_H
#define CITATION_GRAPH_CITATION_GRAPH_H

#include <vector>
#include <memory>
#include <map>
#include <unordered_set>

class PublicationAlreadyCreated : public std::exception {
    virtual const char *what() const throw() {
        return "PublicationAlreadyCreated";
    }
};

class PublicationNotFound : public std::exception {
    virtual const char *what() const throw() {
        return "PublicationNotFound";
    }
};

class TriedToRemoveRoot : public std::exception {
    virtual const char *what() const throw() {
        return "TriedToRemoveRoot";
    }
};

template<class Publication>
class Node {
    using PublId = typename Publication::id_type;
    using GNode = Node<Publication>;
    using NodesMap = std::map<PublId, std::weak_ptr<GNode>>;
    Publication publication;
    std::weak_ptr<Node<Publication>> thisNode;
    std::vector<std::weak_ptr<GNode>> parents;
    std::vector<size_t> positionInParent;
    std::vector<std::shared_ptr<GNode>> children;
    std::vector<size_t> positionInChild;
    NodesMap *map;
    std::unique_ptr<typename NodesMap::iterator> mapIt;

public:
    explicit Node(const PublId &id) : publication(id), mapIt(nullptr) {}

    Node(const PublId &id, std::weak_ptr<GNode> parent, size_t index)
            : publication(id),
              parents(),
              positionInParent(),
              mapIt(nullptr) {
        parents.push_back(parent);
        positionInParent.push_back(index);
    }

    ~Node() {
        for (size_t i = 0; i < children.size(); i++) {
            children[i]->removeParent(positionInChild[i]);
        }

        if (mapIt.get() != nullptr) map->erase(*mapIt);
    }

    void setThisNodePointer(const std::shared_ptr<GNode> &ptr) {
        thisNode = ptr;
    }

    void setMapAndIt(NodesMap *newMap, std::unique_ptr<typename NodesMap::iterator> newIt) {
        map = newMap;
        mapIt = std::move(newIt);
    }

    Publication &getPublication() noexcept {
        return publication;
    }

    size_t howManyChildren() noexcept {
        return children.size();
    }

    size_t howManyParents() noexcept {
        return parents.size();
    }

    // only pop_bakcs which have no-throw guarantee as long as
    // vectors aren't empty
    void removeChildImmediately() {
        children.pop_back();
        positionInChild.pop_back();
    }

    // if childrean push_back fails then we are sure that childrean vector stays
    // unchanged, pop_back have no-throw guarantee as long as vectors aren't empty
    void addExistingChild(const std::shared_ptr<GNode> &child, size_t position) {
        children.push_back(child);
        try {
            positionInChild.push_back(position);
        } catch (...) {
            children.pop_back();
            throw;
        }
    }

    // similar to above
    std::weak_ptr<GNode> addNewChild(const PublId &id) {
        auto child = std::make_shared<GNode>(id, thisNode, children.size());
        child->setThisNodePointer(child);
        children.push_back(child);
        try {
            positionInChild.push_back(0);
        } catch (...) {
            children.pop_back();
            throw;
        }
        return std::weak_ptr(child);
    }

    // similar to above
    std::weak_ptr<GNode> addNewChild(const PublId &id, const std::vector<std::shared_ptr<GNode>> &parents) {
        auto child = std::make_shared<GNode>(id, thisNode, children.size());
        child->setThisNodePointer(child);
        children.push_back(child);
        try {
            positionInChild.push_back(0);
        } catch (...) {
            children.pop_back();
            throw;
        }

        size_t i = 1;
        try {
            for (; i < parents.size(); i++) {
                size_t position = parents[i]->howManyChildren();
                child->addParent(parents[i], position);
                parents[i]->addExistingChild(child, i);
            }
        } catch (...) {
            for (size_t j = 0; j < i; j++) {
                parents[j]->removeChildImmediately();
            }
            throw;
        }

        return std::weak_ptr(child);
    }

    // same as addExistingChild
    void addParent(const std::shared_ptr<GNode> &parent, size_t position) {
        parents.push_back(std::weak_ptr(parent));
        try {
            positionInParent.push_back(position);
        } catch (...) {
            parents.pop_back();
            throw;
        }

    }

    std::vector<size_t> &getPositionsInParents() {
        return positionInParent;
    }

    // just swaps and pop_backs
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

    // just pop_backs
    void reverseChangesInChild(const std::shared_ptr<GNode> &parent) {
        if (!parents.empty()) {
            if (parents[parents.size() - 1].lock().get() == parent.get()) {
                parents.pop_back();

                if (positionInParent.size() != parents.size()) {
                    positionInParent.pop_back();
                }
            }
        }
    }

    // as above
    void reverseChangesInParent(const std::shared_ptr<GNode> &child) {
        if (!children.empty()) {
            if (children[children.size() - 1].get() == child.get()) {
                children.pop_back();

                if (positionInChild.size() != children.size()) {
                    positionInChild.pop_back();
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

    // just swaps and pop_backs
    void removeChild(size_t position) {
        children[position].swap(children[children.size() - 1]);
        children.pop_back();
        std::swap(positionInChild[position], positionInChild[positionInChild.size() - 1]);
        positionInChild.pop_back();

        if (children.size() > position) {
            auto child = children[position];
            child->positionInParent[positionInChild[position]] = position;
        }
    }

    // same as above
    void removeParent(size_t position) {
        parents[position].swap(parents[parents.size() - 1]);
        parents.pop_back();
        std::swap(positionInParent[position], positionInParent[positionInParent.size() - 1]);
        positionInParent.pop_back();

        if (parents.size() > position) {
            auto parent = parents[position].lock();
            parent->positionInChild[positionInParent[position]] = position;
        }
    }

    void remove() {
        for (size_t i = 0; i < parents.size(); i++) {
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
        auto newIt = nodes->insert(std::make_pair(stem_id, std::weak_ptr<GNode>(root))).first;
        root->setMapAndIt(nodes.get(), std::make_unique<typename NodesMap::iterator>(newIt));
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
        auto newSharedPtr = newPtr.lock();

        try {
            auto newIt = nodes->insert(std::make_pair(id, newPtr)).first;
            newSharedPtr->setMapAndIt(nodes.get(), std::make_unique<typename NodesMap::iterator>(newIt));
        } catch (...) {
            parentPtr->removeChildImmediately();
            throw;
        }
    }

    void create(PublId const &id, std::vector<PublId> const &parent_ids) {
        if (parent_ids.size() <= 0) throw PublicationNotFound();

        if (idIsOccupied(id)) throw PublicationAlreadyCreated();

        std::unordered_set<std::shared_ptr<GNode>> parentsSet;
        std::vector<std::shared_ptr<GNode>> parents;
        for (size_t i = 0; i < parent_ids.size(); i++) {
            if (!idIsOccupied(parent_ids[i])) throw PublicationNotFound();
            auto parentPtr = getPointerToNode(parent_ids[i]).lock();
            if (parentsSet.find(parentPtr) == parentsSet.end()) {
                parents.push_back(parentPtr);
                parentsSet.insert(parentPtr);
            }
        }

        auto newPtr = parents[0]->addNewChild(id, parents);
        auto newSharedPtr = newPtr.lock();

        try {
            auto newIt = nodes->insert(std::make_pair(id, newPtr)).first;
            newSharedPtr->setMapAndIt(nodes.get(), std::make_unique<typename NodesMap::iterator>(newIt));
        } catch (...) {
            for (size_t i = 0; i < parents.size(); i++) {
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
        auto positionInParent = parentPtr->howManyChildren();
        auto positionInChild = childPtr->howManyParents();

        try {
            if (!childPtr->citationExists(parentPtr)) {
                childPtr->addParent(parentPtr, positionInParent);
                parentPtr->addExistingChild(childPtr, positionInChild);
            }
        } catch (...) {
            childPtr->reverseChangesInChild(parentPtr);
            parentPtr->reverseChangesInParent(childPtr);
            throw;
        }
    }

    void remove(PublId const &id) {
        if (id == get_root_id()) throw TriedToRemoveRoot();
        if (!idIsOccupied(id)) throw PublicationNotFound();

        auto nodeIt = nodes->find(id);
        std::shared_ptr<GNode> nodePtr = nodeIt->second.lock();

        nodePtr->remove();
    }

};

#endif //CITATION_GRAPH_CITATION_GRAPH_H