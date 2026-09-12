#include <catch2/catch_test_macros.hpp>

#include <App/AppConstants.h>
#include <App/Doc/Document.h>
#include <App/EditWatcher.h>
#include <App/SingletonRepo.h>

namespace {

class TestEditAction final : public UndoableAction {
public:
    bool perform() override {
        value = 1;
        return true;
    }

    bool undo() override {
        value = 0;
        return true;
    }

    int value {};
};

class EditWatcherClient final : public EditWatcher::Client {
public:
    void updateTitle(const String& nextTitle) override {
        title = nextTitle;
        ++titleUpdates;
    }

    void notifyChange() override {
        ++changeNotifications;
    }

    String title;
    int titleUpdates {};
    int changeNotifications {};
};

}

TEST_CASE("Edit watcher publishes undo-backed dirty state to its clients",
        "[edit-watcher][dirty]") {
    ScopedJuceInitialiser_GUI juceGui;
    SingletonRepo repo;
    auto* constants = new AppConstants(&repo);
    constants->setConstant(Constants::ProductName, "Cycle");
    repo.add(constants);
    auto* document = new Document(&repo);
    document->getDetails().setName("Test preset");
    repo.add(document);
    auto* watcher = new EditWatcher(&repo);
    repo.add(watcher);

    EditWatcherClient client;
    watcher->addClient(&client);
    auto* action = new TestEditAction();

    REQUIRE(watcher->addAction(action));
    watcher->handleUpdateNowIfNeeded();
    REQUIRE(watcher->getHaveEdited());
    REQUIRE(client.title == "Cycle - Test preset*");
    REQUIRE(client.titleUpdates == 1);
    REQUIRE(client.changeNotifications == 1);

    watcher->undo();
    REQUIRE_FALSE(watcher->getHaveEdited());
    REQUIRE(client.title == "Cycle - Test preset");
    REQUIRE(client.titleUpdates == 2);
}
