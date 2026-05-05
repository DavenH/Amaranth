#include "Updater.h"
#include "../../App/Settings.h"
#include "../../App/SingletonRepo.h"
#include "../../Definitions.h"

namespace {

const char* getUpdateTypeName(UpdateType type) {
    switch (type) {
        case Null:          return "Null";
        case Update:        return "Update";
        case ReduceDetail:  return "ReduceDetail";
        case RestoreDetail: return "RestoreDetail";
        case Repaint:       return "Repaint";
        default:            return "Unknown";
    }
}

}

Updater::Updater(SingletonRepo* repo) :
        SingletonAccessor   (repo, "Updater")
    ,   graph               (repo)
    ,   lastUpdateMillis    (0)
    ,   millisThresh        (10)
    ,   throttleUpdates     (true) {
}

void Updater::update(int code, UpdateType type) {
    Node* node = startingNodes[code];

    if(getSetting(IgnoringEditMessages)) {
        return;
    }

    if(node != nullptr) {
        int64 millis = Time::currentTimeMillis();

        if (throttleUpdates && lastUpdateMillis > 0 && millis - lastUpdateMillis < millisThresh) {
            queuePendingUpdate(PendingUpdate(code, type));
            triggerAsyncUpdate();

            return;
        }

        lastUpdateMillis = Time::currentTimeMillis();

        graph.setUpdateType(type);
        graph.setUpdateContext(String(getUpdateTypeName(type)) + " from " +
                               (node != nullptr && node->getDebugName().isNotEmpty() ? node->getDebugName()
                                                                                     : "unnamed"));
        graph.update(node);

        sendChangeMessage();
    }
}

void Updater::setThrottling(bool doThrottle, int threshMillis) {
    throttleUpdates = doThrottle;
    millisThresh = threshMillis;
}

void Updater::clearPendingUpdates() {
    pendingUpdates.clear();
    cancelPendingUpdate();
}

void Updater::queuePendingUpdate(const PendingUpdate& update) {
    pendingUpdates.push_back(update);
    optimizePendingUpdates();
}

void Updater::optimizePendingUpdates() {
    deque<PendingUpdate> optimized;

    for (const auto& update : pendingUpdates) {
        if (! optimized.empty() && optimized.back() == update) {
            continue;
        }

        optimized.push_back(update);

        while (optimized.size() >= 3) {
            auto restoreIt = optimized.end();
            --restoreIt;
            auto updateIt = restoreIt;
            --updateIt;
            auto reduceIt = updateIt;
            --reduceIt;

            if (reduceIt->source != updateIt->source || updateIt->source != restoreIt->source) {
                break;
            }

            if (reduceIt->type != ReduceDetail || updateIt->type != Update || restoreIt->type != RestoreDetail) {
                break;
            }

            int source = updateIt->source;
            optimized.erase(reduceIt, optimized.end());
            optimized.push_back(PendingUpdate(source, Update));
        }
    }

    pendingUpdates.swap(optimized);
}

void Updater::handleAsyncUpdate() {
    if (!pendingUpdates.empty()) {
        PendingUpdate pu = pendingUpdates.front();
        pendingUpdates.pop_front();

        update(pu.source, pu.type);
    }
}

/* ----------------------------------------------------------------------------- */

Updater::Graph::Graph(SingletonRepo* repo) :
        SingletonAccessor(repo, "UpdateGraph")
    ,   updateType(Update)
    ,   printsPath(false) {
}

void Updater::Graph::update(Node* startingNode) {
    reset();
    startingNode->markPath();

    String path = String();

    for (auto headNode : headNodes) {
        if(headNode->isDirty()) {
            headNode->performUpdate(path, updateType, printsPath);
        }
    }

    if(printsPath) {
        String action = updateContext;

        if (String extra = getUpdateString(); extra.isNotEmpty()) {
            action << " " << extra;
        }

        path = "Update: " + action + "\t" + path;
        DBG(path);

        lastPath = path;
    }
}

void Updater::Graph::addHeadNode(Node* node) {
    headNodes.addIfNotAlreadyThere(node);
}

void Updater::Graph::addHeadNodes(Array<Node*> nodes) {
    headNodes.addArray(nodes);
}

void Updater::Graph::removeHeadNode(Node* node) {
    headNodes.removeFirstMatchingValue(node);
}

void Updater::Graph::reset() {
    for (auto headNode : headNodes) {
        headNode->reset();
    }
}

/* ----------------------------------------------------------------------------- */

Updater::Node::Node() :
        updated(false)
    ,   dirty(false)
    ,   toUpdate(nullptr) {}

Updater::Node::Node(Updateable* objectToUpdate) :
        updated(false)
    ,   dirty(false)
    ,   toUpdate(objectToUpdate) {
    if (toUpdate != nullptr) {
        debugName = toUpdate->getUpdateName();
    }
}

void Updater::Node::updatesAfter(Node* parent) {
    parent->children.addIfNotAlreadyThere(this);
    parents.addIfNotAlreadyThere(parent);
}

void Updater::Node::marks(Array<Node*> nodes) {
    for (int i = 0; i < nodes.size(); ++i) {
        nodesToMark.addIfNotAlreadyThere(nodes.getUnchecked(i));
    }
}

void Updater::Node::marks(Node* headNode) {
    nodesToMark.addIfNotAlreadyThere(headNode);
}

void Updater::Node::marksAndUpdatesAfter(Node* parent) {
    updatesAfter(parent);
    marks(parent);
}

void Updater::Node::doesntUpdateAfter(Node* parent) {
    parents.removeFirstMatchingValue(parent);
    parent->children.removeFirstMatchingValue(this);
}

void Updater::Node::doesntMark(Node* node) {
    nodesToMark.removeFirstMatchingValue(node);
}

void Updater::Node::performUpdate(String& updatePath, UpdateType updateType, bool shouldPrintPath) {
    if(updated || ! dirty) {
        return;
    }

    for(auto i : children) {
        i->markDirty();
    }

    for(auto parent : parents) {
        if(parent->isDirty()) {
            parent->performUpdate(updatePath, updateType, shouldPrintPath);
        }
    }

    // we can get here by a parent updating this
    if (!updated) {
        if(shouldPrintPath && toUpdate != nullptr) {
            String name = debugName.isNotEmpty() ? debugName : toUpdate->getUpdateName();
            updatePath << name << " ";
        }

        executeUpdate(updateType);

        updated = true;
        dirty = false;

        for(auto node : children) {
            node->performUpdate(updatePath, updateType, shouldPrintPath);
        }
    }
}

void Updater::Node::executeUpdate(UpdateType updateType) {
    if (toUpdate != nullptr) {
        toUpdate->update(updateType);
    }
}

void Updater::Node::reset() {
    updated = false;
    dirty = false;

    for (auto node: children) {
        node->reset();
    }
}

void Updater::Node::markPath() {
    for (auto i: nodesToMark) {
        i->markDirty();
    }
}

void Updater::Node::marksAll(const Array<Node*>& nodes) {
    nodesToMark.addArray(nodes);
}

void Updater::Node::updatesAfterAll(const Array<Node*>& nodes) {
    for (int i = 0; i < parents.size(); ++i) {
        nodes[i]->children.addIfNotAlreadyThere(this);
        parents.addIfNotAlreadyThere(nodes[i]);
    }
}

void Updater::setStartingNode(int code, Node* node) {
    startingNodes[code] = node;
}
