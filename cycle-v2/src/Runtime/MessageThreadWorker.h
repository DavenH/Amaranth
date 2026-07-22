#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <juce_events/juce_events.h>

namespace CycleV2 {

class MessageThreadWorker {
public:
    MessageThreadWorker() : state(std::make_shared<State>()) {}

    ~MessageThreadWorker() {
        shutdown();
    }

    void post(std::function<bool()> work, std::function<void()> publication) {
        worker.addJob(new Job(state, std::move(work), std::move(publication)), true);
    }

    void cancelAndWait() {
        constexpr int waitForLifetimeSafety = -1;
        worker.removeAllJobs(true, waitForLifetimeSafety);
    }

    void shutdown() {
        state->alive.store(false);
        cancelAndWait();
    }

private:
    struct State {
        std::atomic<bool> alive { true };
    };

    class Job final : public juce::ThreadPoolJob {
    public:
        Job(
                std::shared_ptr<State> stateToUse,
                std::function<bool()> workToUse,
                std::function<void()> publicationToUse) :
                ThreadPoolJob("Cycle V2 causal preview")
            ,   state       (std::move(stateToUse))
            ,   work        (std::move(workToUse))
            ,   publication (std::move(publicationToUse)) {
        }

        JobStatus runJob() override {
            if (!state->alive.load() || !work()) {
                return jobHasFinished;
            }
            auto stateToPublish = state;
            auto publicationToRun = std::move(publication);
            juce::MessageManager::callAsync([
                    stateToPublish,
                    publicationToRun = std::move(publicationToRun)] {
                if (stateToPublish->alive.load()) {
                    publicationToRun();
                }
            });
            return jobHasFinished;
        }

    private:
        std::shared_ptr<State> state;
        std::function<bool()> work;
        std::function<void()> publication;
    };

    juce::ThreadPool worker { 1 };
    std::shared_ptr<State> state;
};

}
