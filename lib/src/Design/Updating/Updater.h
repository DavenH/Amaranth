#pragma once

#include <map>
#include <deque>
#include "Updateable.h"
#include "../../App/SingletonAccessor.h"
#include "../../Obj/Ref.h"
#include "JuceHeader.h"

using std::map;
using std::deque;

class Updater :
        public SingletonAccessor
    ,   public AsyncUpdater
    ,   public ChangeBroadcaster {
public:
    class Node {
    public:
        Node();
        explicit Node(Updateable* object);
        virtual ~Node() = default;

        // upstroke
        void marks              (Array<Node*> nodes);
        void marks              (Node* node);
        void doesntMark         (Node* node);
        void marksAndUpdatesAfter(Node* parent);
        void marksAll           (const Array<Node*>& nodes);

        // down stroke
        void doesntUpdateAfter  (Node* parent);
        void updatesAfter       (Node* parent);
        void updatesAfterAll    (const Array<Node*>& parents);

        void reset();
        void markPath();
        void performUpdate(String& path, UpdateType updateType, bool shouldPrintPath = false);

        [[nodiscard]] bool isDirty() const   { return dirty;    }
        [[nodiscard]] bool isUpdated() const { return updated;  }
        [[nodiscard]] const Array<Node*>& getParents() const     { return parents;     }
        [[nodiscard]] const Array<Node*>& getChildren() const    { return children;    }
        [[nodiscard]] const Array<Node*>& getMarkedNodes() const { return nodesToMark; }
        [[nodiscard]] Updateable* getTarget() const              { return toUpdate;    }
        [[nodiscard]] const String& getDebugName() const         { return debugName;   }
        void markDirty() { dirty = true;    }
        void setDebugName(const String& name) { debugName = name; }

        virtual void executeUpdate(UpdateType updateType);

    private:
        bool updated;
        bool dirty;

        String debugName;
        Array<Node*> parents;
        Array<Node*> children;
        Array<Node*> nodesToMark;
        Updateable* toUpdate;

        friend class Updater;
        friend class Graph;

        JUCE_LEAK_DETECTOR(Node);
    };

    class Graph : public SingletonAccessor {
    public:
        explicit Graph(SingletonRepo* repo);
        ~Graph() override = default;

        void addHeadNode    (Node* node);
        void addHeadNodes   (Array<Node*> nodes);
        void removeHeadNode (Node* node);
        void update         (Node* node);
        void setUpdateContext(const String& context) { updateContext = context; }

        [[nodiscard]] const Array<Node*>& getHeadNodes() const { return headNodes; }
        void setUpdateType(UpdateType type)     { updateType = type; }
        void setPrintsPath(bool does)   { printsPath = does; }

        virtual String getUpdateString() { return {}; }

    private:
        bool printsPath;
        UpdateType updateType;
        String lastPath;
        String updateContext;
        Array<Node*> headNodes;

        void reset() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Graph);
    };

    struct PendingUpdate {
        PendingUpdate(int source, UpdateType type) : type(type), source(source) {}

        int source;
        UpdateType type;
        bool operator==(const PendingUpdate& that) const { return type == that.type && source == that.source; }
        bool operator!=(const PendingUpdate& that) const { return ! operator==(that); }
    };

    /* ----------------------------------------------------------------------------- */

    explicit Updater(SingletonRepo* repo);
    ~Updater() override = default;

    void setThrottling(bool doThrottle, int threshMillis);
    void update(int code, UpdateType type = Update);
    void clearPendingUpdates();
    void handleAsyncUpdate() override;
    void addListener(ChangeListener* listener) { listeners.add(listener); }
    void setStartingNode(int code, Node* node);
    Graph& getGraph() { return graph; }
    const Graph& getGraph() const { return graph; }

protected:
    bool throttleUpdates;
    int millisThresh;

    int64 lastUpdateMillis;

    Graph graph;
    map<int, Node*> startingNodes;
    Array<ChangeListener*> listeners;
    deque<PendingUpdate> pendingUpdates;

    void queuePendingUpdate(const PendingUpdate& update);
    void optimizePendingUpdates();
};
