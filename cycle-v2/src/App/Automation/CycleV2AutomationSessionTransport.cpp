#include "App/Automation/CycleV2AutomationSessionTransport.h"

#include "App/Automation/CycleV2AutomationProtocol.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#if JUCE_MAC || JUCE_LINUX
#include <fcntl.h>
#include <poll.h>
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
        active->store(false);
        signalThreadShouldExit();
        stopThread(1000);
      #if JUCE_MAC || JUCE_LINUX
        if (serverFd >= 0) {
            ::close(serverFd);
        }
        ::unlink(socketPath.toRawUTF8());
      #else
        File(socketPath).deleteFile();
      #endif
    }

    bool start(String& message) {
      #if JUCE_MAC || JUCE_LINUX
        sockaddr_un address {};
        const auto path = socketPath.toRawUTF8();
        if (std::strlen(path) >= sizeof(address.sun_path)) {
            message = "Cycle V2 session socket path is too long";
            return false;
        }
        ::unlink(path);
        serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (serverFd < 0 || !makeNonblocking(serverFd)) {
            message = "Could not create Cycle V2 session socket: " + String(std::strerror(errno));
            return false;
        }
        address.sun_family = AF_UNIX;
        std::strcpy(address.sun_path, path);
        if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
                || ::listen(serverFd, maximumClients) != 0) {
            message = "Could not bind/listen Cycle V2 session socket: " + String(std::strerror(errno));
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
        std::vector<std::unique_ptr<Client>> clients;
        while (!threadShouldExit()) {
            std::vector<pollfd> sockets { { serverFd, POLLIN, 0 } };
            for (auto& client : clients) {
                serviceRequest(*client);
                const short events = client->output.empty() ? POLLIN : POLLOUT;
                const int fd = client->readClosed && client->output.empty() ? -1 : client->fd;
                sockets.push_back({ fd, events, 0 });
            }
            const int ready = ::poll(sockets.data(), sockets.size(), 20);
            if (ready < 0 && errno != EINTR) {
                break;
            }
            for (size_t index = 0; index < clients.size(); ++index) {
                auto& client = *clients[index];
                const short events = sockets[index + 1].revents;
                if (!client.readClosed && (events & (POLLIN | POLLHUP)) != 0) {
                    readClient(client);
                }
                if ((events & POLLOUT) != 0) {
                    writeClient(client);
                }
                if ((events & (POLLERR | POLLNVAL)) != 0) {
                    client.closed = true;
                }
                if (client.readClosed && client.input.empty() && client.output.empty()
                        && client.pending == nullptr) {
                    client.closed = true;
                }
            }
            clients.erase(std::remove_if(clients.begin(), clients.end(), [](const auto& client) {
                return client->closed;
            }), clients.end());
            if ((sockets.front().revents & POLLIN) != 0) {
                const int fd = ::accept(serverFd, nullptr, nullptr);
                if (fd >= 0) {
                    if (clients.size() < maximumClients && makeNonblocking(fd)) {
                      #if JUCE_MAC
                        const int enabled = 1;
                        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
                      #endif
                        clients.push_back(std::make_unique<Client>(fd));
                    } else {
                        ::close(fd);
                    }
                }
            }
        }
      #endif
    }

private:
    static constexpr int maximumClients = 16;
    static constexpr size_t maximumRequestBytes = 16 * 1024 * 1024;
    static constexpr size_t maximumResponseBytes = 64 * 1024 * 1024;

    struct Pending {
        std::atomic<bool> cancelled { false };
        WaitableEvent completed;
        var response;
        var request;
        double started { Time::getMillisecondCounterHiRes() };
    };

    struct Client {
        explicit Client(int socket) : fd(socket) {}

        ~Client() {
            if (pending != nullptr) {
                pending->cancelled.store(true);
            }
          #if JUCE_MAC || JUCE_LINUX
            ::close(fd);
          #endif
        }

        bool closed { false };
        bool readClosed { false };
        int fd;
        size_t written { 0 };
        std::string input;
        std::string output;
        std::shared_ptr<Pending> pending;
    };

    static var errorResponse(const String& message, const var& request = {}) {
        var response = makeObject();
        auto* object = objectFor(response);
        object->setProperty("ok", false);
        object->setProperty("message", message);
        if (const auto* requestObject = objectFor(request)) {
            object->setProperty("id", requestObject->getProperty("id"));
        }
        return response;
    }

    static bool makeNonblocking(int fd) {
      #if JUCE_MAC || JUCE_LINUX
        const int flags = ::fcntl(fd, F_GETFL, 0);
        return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
      #else
        ignoreUnused(fd);
        return false;
      #endif
    }

    static void queueResponse(Client& client, const var& response) {
        const auto text = JSON::toString(response, true).toStdString();
        if (text.size() > maximumResponseBytes) {
            client.closed = true;
            return;
        }
        client.output = text + "\n";
        client.written = 0;
    }

    void serviceRequest(Client& client) {
        if (client.pending != nullptr) {
            auto& pending = *client.pending;
            if (pending.completed.wait(0)) {
                queueResponse(client, pending.response);
                client.pending.reset();
            } else if (Time::getMillisecondCounterHiRes() - pending.started > 30000) {
                pending.cancelled.store(true);
                queueResponse(client, errorResponse("Timed out waiting for Cycle V2 message thread", pending.request));
                client.pending.reset();
            }
        }
        if (client.pending != nullptr || !client.output.empty() || client.closed) {
            return;
        }
        const auto newline = client.input.find('\n');
        if (newline == std::string::npos) {
            return;
        }
        auto pending = std::make_shared<Pending>();
        pending->request = JSON::parse(String::fromUTF8(client.input.data(), int(newline)));
        client.input.erase(0, newline + 1);
        client.pending = pending;
        const auto handler = handleRequest;
        const auto running = active;
        if (!MessageManager::callAsync([handler, running, pending] {
            if (running->load() && !pending->cancelled.load()) {
                pending->response = handler(pending->request);
                pending->completed.signal();
            }
        })) {
            queueResponse(client, errorResponse("Could not dispatch Cycle V2 session request", pending->request));
            client.pending.reset();
        }
    }

    static void readClient(Client& client) {
      #if JUCE_MAC || JUCE_LINUX
        char buffer[8192];
        const ssize_t count = ::recv(client.fd, buffer, sizeof(buffer), 0);
        if (count > 0) {
            client.input.append(buffer, size_t(count));
            if (client.input.size() > maximumRequestBytes) {
                client.closed = true;
            }
        } else if (count == 0) {
            client.readClosed = true;
            if (!client.input.empty() && client.input.back() != '\n') {
                client.input += '\n';
            }
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            client.closed = true;
        }
      #else
        ignoreUnused(client);
      #endif
    }

    static void writeClient(Client& client) {
      #if JUCE_MAC || JUCE_LINUX
      #if JUCE_LINUX
        constexpr int sendFlags = MSG_NOSIGNAL;
      #else
        constexpr int sendFlags = 0;
      #endif
        const ssize_t count = ::send(client.fd, client.output.data() + client.written,
                client.output.size() - client.written, sendFlags);
        if (count > 0) {
            client.written += size_t(count);
            if (client.written == client.output.size()) {
                client.output.clear();
                client.written = 0;
            }
        } else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            client.closed = true;
        }
      #else
        ignoreUnused(client);
      #endif
    }

    RequestHandler handleRequest;
    String socketPath;
    std::shared_ptr<std::atomic<bool>> active { std::make_shared<std::atomic<bool>>(true) };
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
            [handler = handleCommand](const var& request) { return handleRequest(request, handler); },
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

var CycleV2AutomationSessionTransport::handleRequest(
        const var& request, const CommandHandler& handler) {
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

    const var result = handler(command);
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
