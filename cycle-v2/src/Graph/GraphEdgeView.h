#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView {
public:
    class ConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Edge;
        using difference_type = std::ptrdiff_t;
        using pointer = const Edge*;
        using reference = const Edge&;

        ConstIterator(const GraphEdgeView& edgeView, size_t edgeIndex) :
                view(&edgeView)
            ,   index(edgeIndex) {}

        reference operator*() const { return (*view)[index]; }
        pointer operator->() const { return &(*view)[index]; }
        ConstIterator& operator++() {
            ++index;
            return *this;
        }
        ConstIterator operator++(int) {
            ConstIterator previous = *this;
            ++*this;
            return previous;
        }
        bool operator==(const ConstIterator& other) const {
            return view == other.view && index == other.index;
        }
        bool operator!=(const ConstIterator& other) const { return !(*this == other); }

    private:
        const GraphEdgeView* view {};
        size_t index {};
    };

    explicit GraphEdgeView(const std::vector<Edge>& existingEdges) :
            existing(existingEdges) {}

    GraphEdgeView(
            const std::vector<Edge>& existingEdges,
            std::vector<size_t> removedIndices,
            std::vector<Edge> addedEdges) :
            existing(existingEdges)
        ,   removed(std::move(removedIndices))
        ,   added(std::move(addedEdges)) {
        std::sort(removed.begin(), removed.end());
        removed.erase(std::unique(removed.begin(), removed.end()), removed.end());
        jassert(removed.empty() || removed.back() < existing.size());
    }

    size_t size() const { return existing.size() - removed.size() + added.size(); }
    ConstIterator begin() const { return { *this, 0 }; }
    ConstIterator end() const { return { *this, size() }; }

    const Edge& operator[](size_t index) const {
        const size_t retainedCount = existing.size() - removed.size();
        if (index >= retainedCount) {
            return added[index - retainedCount];
        }

        size_t existingIndex = index;
        for (const size_t removedIndex : removed) {
            if (removedIndex > existingIndex) {
                break;
            }
            ++existingIndex;
        }
        return existing[existingIndex];
    }

private:
    const std::vector<Edge>& existing;
    std::vector<size_t> removed;
    std::vector<Edge> added;
};

}
