#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class CycleV2AutomationSessionTransport {
public:
    using CommandHandler = std::function<juce::var(const juce::var&)>;

    explicit CycleV2AutomationSessionTransport(CommandHandler commandHandler);
    ~CycleV2AutomationSessionTransport();

    bool start(const juce::String& socketPath, juce::String& message);
    bool isRunning() const;

private:
    class Server;

    CommandHandler handleCommand;
    std::unique_ptr<Server> server;

    juce::var handleRequest(const juce::var& request);
};

}
