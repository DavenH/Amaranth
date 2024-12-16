#pragma once
#include "JuceHeader.h"
#include <iterator>

using namespace juce;

template<class T>
class FilterableList {
protected:
    Array <T, CriticalSection> allItems;

    void setListBox(ListBox* listbox) {
        this->listBox = listbox;
    }

    virtual bool containsString(T& item, const String& str) = 0;

    class ReverseElementComparator {
    private:
    public:
        static int compareElements(const T& first, const T& second) {
            return (first < second) ? 1 : ((second < first) ? -1 : 0);
        }
    };

public:
    virtual ~FilterableList() = default;

    FilterableList() : sortForwards(false), listBox(nullptr) {
    }

    virtual void doChangeUpdate() = 0;

    void resetList() {
        filterList(StringArray());
    }

    void filterList(const StringArray& searchStrings) {
        filteredItems.clear();
        String name;

        if (searchStrings.size() == 0 || searchStrings[0].isEmpty()) {
            filteredItems.addArray(allItems);
        } else {
            for (int i = 0; i < (int) allItems.size(); ++i) {
                bool containsAll = true;

                for (const auto& searchString : searchStrings) {
                    containsAll &= containsString(allItems.getReference(i), searchString);
                }

                if (containsAll) {
                    filteredItems.add(allItems.getReference(i));
                }
            }
        }

        if (sortForwards) {
            filteredItems.sort(reverseComparator);
        } else {
            filteredItems.sort(defaultComparator);
        }

        listBox->updateContent();

        if (!filteredItems.size() == 0) {
            listBox->selectRow(0);
        }

        doChangeUpdate();
    }

    const Array <T, CriticalSection>& getFilteredItems() const {
        return filteredItems;
    }

    Array <T, CriticalSection>& getFilteredItemsMutable() {
        return filteredItems;
    }

    void setSortDirection(bool isForwards) {
        sortForwards = isForwards;
    }

    virtual void triggerLoadItem() = 0;

protected:
    DefaultElementComparator <T> defaultComparator;
    ReverseElementComparator reverseComparator;

    bool sortForwards;
    Array <T, CriticalSection> filteredItems;
    ListBox* listBox;
};