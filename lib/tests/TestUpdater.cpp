#include <catch2/catch_test_macros.hpp>

#include "../src/Design/Updating/Updater.h"

namespace {
    class TestableUpdater :
            public Updater {
    public:
        TestableUpdater() :
                Updater(nullptr) {
        }

        void addPending(int source, UpdateType type) {
            pendingUpdates.push_back(PendingUpdate(source, type));
        }

        void optimize() {
            optimizePendingUpdates();
        }

        const deque<PendingUpdate>& getPendingUpdates() const {
            return pendingUpdates;
        }
    };

    void requirePendingUpdate(const Updater::PendingUpdate& update, int source, UpdateType type) {
        REQUIRE(update.source == source);
        REQUIRE(update.type == type);
    }
}

TEST_CASE("Updater optimizes redundant pending updates", "[updating]") {
    TestableUpdater updater;

    SECTION("collapses adjacent duplicate source and type updates") {
        updater.addPending(1, Update);
        updater.addPending(1, Update);
        updater.addPending(2, Update);
        updater.addPending(1, Update);
        updater.addPending(1, Update);

        updater.optimize();

        const auto& pending = updater.getPendingUpdates();
        REQUIRE(pending.size() == 3);
        requirePendingUpdate(pending[0], 1, Update);
        requirePendingUpdate(pending[1], 2, Update);
        requirePendingUpdate(pending[2], 1, Update);
    }

    SECTION("collapses reduce update restore sequence from the same source") {
        updater.addPending(3, ReduceDetail);
        updater.addPending(3, Update);
        updater.addPending(3, RestoreDetail);

        updater.optimize();

        const auto& pending = updater.getPendingUpdates();
        REQUIRE(pending.size() == 1);
        requirePendingUpdate(pending[0], 3, Update);
    }

    SECTION("preserves reduce update restore sequence across different sources") {
        updater.addPending(4, ReduceDetail);
        updater.addPending(5, Update);
        updater.addPending(4, RestoreDetail);

        updater.optimize();

        const auto& pending = updater.getPendingUpdates();
        REQUIRE(pending.size() == 3);
        requirePendingUpdate(pending[0], 4, ReduceDetail);
        requirePendingUpdate(pending[1], 5, Update);
        requirePendingUpdate(pending[2], 4, RestoreDetail);
    }
}
