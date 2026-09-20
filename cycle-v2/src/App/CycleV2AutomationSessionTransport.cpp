#include "App/CycleV2AutomationSessionTransport.h"

#include "App/CycleV2AutomationProtocol.h"

#include <cerrno>
#include <cstring>
#include <utility>

#if JUCE_MAC || JUCE_LINUX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace CycleV2 {

using namespace juce;
using namespace AutomationProtocol;

class CycleV2AutomationSessionTransport::Server :
        public Thread {
public:
    using RequestHandler = std::function<var(const var&)>;

    Server(RequestHandler requestHandler, String targetSocketPath) :
            Thread("CycleV2AutomationSession")
        ,   handleRequest(std::move(requestHandler))
        ,   socketPath(std::move(targetSocketPath)) {
    }

    ~Server() override {
        signalThreadShouldExit();
        closeServerSocket();
        stopThread(1000);
      #if JUCE_MAC || JUCE_LINUX
        ::unlink(socketPath.toRawUTF8());
      #else
        File(socketPath).deleteFile();
      #endif
    }

    bool start(String& message) {
      #if JUCE_MAC || JUCE_LINUX
        ::unlink(socketPath.toRawUTF8());
        serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (serverFd < 0) {
            message = "Could not create Cycle V2 session socket: " + String(std::strerror(errno));
            return false;
        }

        sockaddr_un address {};
        address.sun_family = AF_UNIX;
        const auto path = socketPath.toRawUTF8();
        std::strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
        if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            message = "Could not bind Cycle V2 session socket: " + String(std::strerror(errno));
            closeServerSocket();
            return false;
        }
        if (::listen(serverFd, 8) != 0) {
            message = "Could not listen on Cycle V2 session socket: " + String(std::strerror(errno));
            closeServerSocket();
            return false;
        }

        startThread();
        message = "Cycle V2 automation session listening: " + socketPath;
        return true;
      #else
        ignoreUnused(message);
        return false;
      #endif
    }

    void run() override {
      #if JUCE_MAC || JUCE_LINUX
        while (!threadShouldExit()) {
            const int clientFd = ::accept(serverFd, nullptr, nullptr);
            if (clientFd < 0) {
                if (!threadShouldExit()) {
                    Thread::sleep(25);
                }
                continue;
            }

            handleClient(clientFd);
            ::close(clientFd);
        }
      #endif
    }

private:
    static var errorResponse(const String& message) {
        var response = makeObject();
        auto* object = objectFor(response);
        object->setProperty("ok", false);
        object->setProperty("message", message);
        return response;
    }

    void closeServerSocket() {
      #if JUCE_MAC || JUCE_LINUX
        if (serverFd >= 0) {
            ::shutdown(serverFd, SHUT_RDWR);
            ::close(serverFd);
            serverFd = -1;
        }
      #endif
    }

    void handleClient(int clientFd) {
      #if JUCE_MAC || JUCE_LINUX
        String requestText;
        char buffer[1024];
        while (!threadShouldExit()) {
            const ssize_t count = ::read(clientFd, buffer, sizeof(buffer));
            if (count <= 0) {
                break;
            }

            requestText += String::fromUTF8(buffer, int(count));
            if (requestText.containsChar('\n')) {
                requestText = requestText.upToFirstOccurrenceOf("\n", false, false);
                break;
            }
        }

        const var request = JSON::parse(requestText);
        auto completed = std::make_shared<WaitableEvent>();
        auto response = std::make_shared<var>();
        const bool dispatched = MessageManager::callAsync([this, request, response, completed] {
            *response = handleRequest(request);
            completed->signal();
        });
        if (dispatched) {
            if (!completed->wait(30000)) {
                *response = errorResponse("Timed out waiting for Cycle V2 message thread");
            }
        } else {
            *response = errorResponse("Could not dispatch Cycle V2 session request to message thread");
        }

        const String responseText = JSON::toString(*response, true) + "\n";
        const CharPointer_UTF8 utf8 = responseText.toUTF8();
        ::write(clientFd, utf8.getAddress(), std::strlen(utf8.getAddress()));
      #else
        ignoreUnused(clientFd);
      #endif
    }

    RequestHandler handleRequest;
    String socketPath;
    int serverFd { -1 };
};

CycleV2AutomationSessionTransport::CycleV2AutomationSessionTransport(
        CommandHandler commandHandler) :
        handleCommand(std::move(commandHandler)) {
}

CycleV2AutomationSessionTransport::~CycleV2AutomationSessionTransport() = default;

bool CycleV2AutomationSessionTransport::start(const String& socketPath, String& message) {
    if (server != nullptr) {
        return true;
    }

    auto candidate = std::make_unique<Server>(
            [this](const var& request) { return handleRequest(request); },
            socketPath);
    if (!candidate->start(message)) {
        return false;
    }

    server = std::move(candidate);
    return true;
}

bool CycleV2AutomationSessionTransport::isRunning() const {
    return server != nullptr;
}

var CycleV2AutomationSessionTransport::handleRequest(const var& request) {
    var response = makeObject();
    auto* responseObject = objectFor(response);
    responseObject->setProperty("ok", false);
    if (request.isVoid()) {
        responseObject->setProperty("message", "Invalid JSON request");
        return response;
    }

    var command = request;
    if (const auto* requestObject = objectFor(request)) {
        const var id = requestObject->getProperty("id");
        if (!id.isVoid()) {
            responseObject->setProperty("id", id);
        }

        const var nestedCommand = requestObject->getProperty("command");
        if (!nestedCommand.isVoid()) {
            command = nestedCommand;
        }
    }

    const var result = handleCommand(command);
    responseObject->setProperty("ok", boolProperty(result, "ok"));
    responseObject->setProperty("result", result);
    if (stringProperty(command, "command") == "quit") {
        MessageManager::callAsync([] {
            JUCEApplicationBase::quit();
        });
    }
    return response;
}

}
