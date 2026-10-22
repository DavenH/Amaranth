#include <catch2/catch_test_macros.hpp>

#include "App/CycleV2AutomationProtocol.h"
#include "App/CycleV2AutomationSessionTransport.h"

#if JUCE_MAC || JUCE_LINUX
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace CycleV2;
using namespace CycleV2::AutomationProtocol;
using namespace juce;

namespace {

struct SocketClient {
    explicit SocketClient(const String& path) {
        fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        REQUIRE(fd >= 0);
      #if JUCE_MAC
        const int enabled = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
      #endif
        sockaddr_un address {};
        address.sun_family = AF_UNIX;
        std::strcpy(address.sun_path, path.toRawUTF8());
        REQUIRE(::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    }

    ~SocketClient() {
        ::close(fd);
    }

    void send(const std::string& text) const {
        size_t sent = 0;
        while (sent < text.size()) {
          #if JUCE_LINUX
            const int flags = MSG_NOSIGNAL;
          #else
            const int flags = 0;
          #endif
            const auto count = ::send(fd, text.data() + sent, text.size() - sent, flags);
            REQUIRE(count > 0);
            sent += size_t(count);
        }
    }

    var response() {
        const double deadline = Time::getMillisecondCounterHiRes() + 3000;
        while (input.find('\n') == std::string::npos
                && Time::getMillisecondCounterHiRes() < deadline) {
            MessageManager::getInstance()->runDispatchLoopUntil(5);
            char bytes[4096];
            const auto count = ::recv(fd, bytes, sizeof(bytes), MSG_DONTWAIT);
            if (count > 0) {
                input.append(bytes, size_t(count));
            } else if (count == 0) {
                break;
            } else {
                REQUIRE((errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR));
            }
        }
        const auto newline = input.find('\n');
        REQUIRE(newline != std::string::npos);
        const auto result = JSON::parse(String::fromUTF8(input.data(), int(newline)));
        input.erase(0, newline + 1);
        return result;
    }

    int fd;
    std::string input;
};

String socketPath() {
    return File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cv2-" + Uuid().toString().substring(0, 12) + ".sock").getFullPathName();
}

var echoCommand(const var& command) {
    var response = makeObject();
    objectFor(response)->setProperty("ok", true);
    objectFor(response)->setProperty("echo", command);
    return response;
}

}

TEST_CASE("Automation sessions preserve split UTF8 and multiple requests on one connection",
        "[cycle-v2][automation][session-transport]") {
    ScopedJuceInitialiser_GUI gui;
    const auto path = socketPath();
    int calls = 0;
    CycleV2AutomationSessionTransport transport([&](const var& command) {
        REQUIRE(MessageManager::getInstance()->isThisTheMessageThread());
        ++calls;
        return echoCommand(command);
    });
    String message;
    REQUIRE(transport.start(path, message));
    SocketClient client(path);
    const std::string lines = u8"{\"id\":\"first\",\"command\":{\"command\":\"café\"}}\n"
            "{\"id\":2,\"command\":{\"command\":\"ping\"}}\n";
    const auto split = lines.find("\xc3") + 1;
    client.send(lines.substr(0, split));
    Thread::sleep(25);
    REQUIRE(calls == 0);
    client.send(lines.substr(split));
    const auto first = client.response();
    REQUIRE(stringProperty(first, "id") == "first");
    REQUIRE(stringProperty(objectFor(objectFor(first)->getProperty("result"))->getProperty("echo"), "command")
            == String::fromUTF8("caf\xc3\xa9"));
    const auto second = client.response();
    REQUIRE(intProperty(second, "id") == 2);
    client.send("{\"id\":\"third\",\"command\":{\"command\":\"ping\"}}\n");
    REQUIRE(stringProperty(client.response(), "id") == "third");
    REQUIRE(calls == 3);
}

TEST_CASE("Idle persistent clients do not block other clients or truncate large replies",
        "[cycle-v2][automation][session-transport]") {
    ScopedJuceInitialiser_GUI gui;
    const auto path = socketPath();
    const String largeReply = String::repeatedString("response", 40000);
    CycleV2AutomationSessionTransport transport([&](const var& command) {
        auto result = echoCommand(command);
        if (stringProperty(command, "command") == "large") {
            objectFor(result)->setProperty("large", largeReply);
        }
        return result;
    });
    String message;
    REQUIRE(transport.start(path, message));
    SocketClient idle(path);
    idle.send("{\"id\":\"idle\",");
    SocketClient blockedReader(path);
    const int smallBuffer = 4096;
    ::setsockopt(blockedReader.fd, SOL_SOCKET, SO_RCVBUF, &smallBuffer, sizeof(smallBuffer));
    blockedReader.send("{\"id\":\"large\",\"command\":{\"command\":\"large\"}}\n");
    MessageManager::getInstance()->runDispatchLoopUntil(50);
    {
        SocketClient active(path);
        active.send("{\"id\":\"active\",\"command\":{\"command\":\"ping\"}}\n");
        const auto reply = active.response();
        REQUIRE(stringProperty(reply, "id") == "active");
    }
    const auto large = blockedReader.response();
    REQUIRE(stringProperty(large, "id") == "large");
    REQUIRE(stringProperty(objectFor(large)->getProperty("result"), "large") == largeReply);
    idle.send("\"command\":{\"command\":\"ping\"}}\n");
    REQUIRE(stringProperty(idle.response(), "id") == "idle");
    SocketClient reconnected(path);
    reconnected.send("{\"id\":\"legacy\",\"command\":{\"command\":\"large\"}}\n");
    ::shutdown(reconnected.fd, SHUT_WR);
    const auto legacy = reconnected.response();
    REQUIRE(stringProperty(legacy, "id") == "legacy");
    REQUIRE(stringProperty(objectFor(legacy)->getProperty("result"), "large") == largeReply);
}

TEST_CASE("Automation transport shutdown closes idle clients and cancels queued dispatch",
        "[cycle-v2][automation][session-transport]") {
    ScopedJuceInitialiser_GUI gui;
    const auto path = socketPath();
    int calls = 0;
    auto transport = std::make_unique<CycleV2AutomationSessionTransport>([&](const var& command) {
        ++calls;
        return echoCommand(command);
    });
    String message;
    REQUIRE(transport->start(path, message));
    SocketClient idle(path);
    SocketClient queued(path);
    queued.send("{\"id\":\"queued\",\"command\":{\"command\":\"ping\"}}\n");
    Thread::sleep(80);
    const auto started = Time::getMillisecondCounterHiRes();
    transport.reset();
    REQUIRE(Time::getMillisecondCounterHiRes() - started < 500);
    REQUIRE_FALSE(File(path).exists());
    MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE(calls == 0);
    char byte;
    REQUIRE(::recv(idle.fd, &byte, 1, MSG_DONTWAIT) == 0);
    REQUIRE(::recv(queued.fd, &byte, 1, MSG_DONTWAIT) == 0);
}

TEST_CASE("Oversized session input is bounded without blocking another client",
        "[cycle-v2][automation][session-transport]") {
    ScopedJuceInitialiser_GUI gui;
    const auto path = socketPath();
    int calls = 0;
    CycleV2AutomationSessionTransport transport([&](const var& command) {
        ++calls;
        return echoCommand(command);
    });
    String message;
    REQUIRE(transport.start(path, message));
    SocketClient oversized(path);
    const std::string bytes(16 * 1024 * 1024 + 8192, 'x');
    size_t sent = 0;
    while (sent < bytes.size()) {
      #if JUCE_LINUX
        constexpr int flags = MSG_NOSIGNAL;
      #else
        constexpr int flags = 0;
      #endif
        const auto count = ::send(oversized.fd, bytes.data() + sent, bytes.size() - sent, flags);
        if (count <= 0) {
            break;
        }
        sent += size_t(count);
    }
    pollfd socket { oversized.fd, POLLIN, 0 };
    REQUIRE(::poll(&socket, 1, 1000) == 1);
    char byte;
    REQUIRE(::recv(oversized.fd, &byte, 1, MSG_DONTWAIT) <= 0);
    REQUIRE(calls == 0);
    SocketClient healthy(path);
    healthy.send("{\"id\":\"healthy\",\"command\":{\"command\":\"ping\"}}\n");
    REQUIRE(stringProperty(healthy.response(), "id") == "healthy");
    REQUIRE(calls == 1);
}
#endif
