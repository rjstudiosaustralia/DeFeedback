#include "RemoteControlServer.h"

#include "RemoteControlPage.h"

#include <array>
#include <string>

#if JUCE_MAC
 #include <stdlib.h>
#endif

namespace defeedback
{
namespace
{
constexpr size_t maxRequestBytes = 64 * 1024;

struct HttpRequest
{
    juce::String method;
    juce::String path;
    juce::String authorisation;
    juce::String body;
};

bool constantTimeEquals (const juce::String& first, const juce::String& second)
{
    const auto a = first.toRawUTF8();
    const auto b = second.toRawUTF8();
    const auto aLength = first.getNumBytesAsUTF8();
    const auto bLength = second.getNumBytesAsUTF8();
    const auto length = juce::jmax (aLength, bLength);
    unsigned int difference = static_cast<unsigned int> (aLength ^ bLength);

    for (size_t index = 0; index < length; ++index)
    {
        const auto aByte = index < aLength ? static_cast<unsigned char> (a[index]) : 0u;
        const auto bByte = index < bLength ? static_cast<unsigned char> (b[index]) : 0u;
        difference |= aByte ^ bByte;
    }

    return difference == 0;
}

bool readRequest (juce::StreamingSocket& socket, HttpRequest& result)
{
    std::string bytes;
    bytes.reserve (4096);
    size_t headerEnd = std::string::npos;
    size_t expectedSize = 0;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000.0;

    while (bytes.size() < maxRequestBytes)
    {
        if (juce::Thread::currentThreadShouldExit()
            || juce::Time::getMillisecondCounterHiRes() > deadline)
            return false;

        if (socket.waitUntilReady (true, 2000) <= 0)
            return false;

        char buffer[4096];
        const auto count = socket.read (buffer, static_cast<int> (sizeof (buffer)), false);
        if (count <= 0)
            return false;

        bytes.append (buffer, static_cast<size_t> (count));
        if (bytes.size() > maxRequestBytes)
            return false;

        if (headerEnd == std::string::npos)
        {
            headerEnd = bytes.find ("\r\n\r\n");
            if (headerEnd == std::string::npos)
                continue;

            const auto header = juce::String::fromUTF8 (bytes.data(), static_cast<int> (headerEnd));
            auto lines = juce::StringArray::fromLines (header);
            if (lines.isEmpty())
                return false;

            auto requestParts = juce::StringArray::fromTokens (lines[0], " ", {});
            if (requestParts.size() < 2)
                return false;

            result.method = requestParts[0].toUpperCase();
            result.path = requestParts[1].upToFirstOccurrenceOf ("?", false, false);

            size_t contentLength = 0;
            for (int index = 1; index < lines.size(); ++index)
            {
                const auto line = lines[index];
                const auto key = line.upToFirstOccurrenceOf (":", false, false).trim();
                const auto value = line.fromFirstOccurrenceOf (":", false, false).trim();
                if (key.equalsIgnoreCase ("Content-Length"))
                    contentLength = static_cast<size_t> (juce::jmax (0, value.getIntValue()));
                else if (key.equalsIgnoreCase ("Authorization"))
                    result.authorisation = value;
            }

            if (headerEnd + 4 > maxRequestBytes
                || contentLength > maxRequestBytes - headerEnd - 4)
                return false;

            expectedSize = headerEnd + 4 + contentLength;
        }

        if (headerEnd != std::string::npos && bytes.size() >= expectedSize)
        {
            const auto bodyStart = headerEnd + 4;
            result.body = juce::String::fromUTF8 (bytes.data() + bodyStart,
                                                   static_cast<int> (expectedSize - bodyStart));
            return true;
        }
    }

    return false;
}

bool writeAll (juce::StreamingSocket& socket, const void* data, size_t size)
{
    auto* bytes = static_cast<const char*> (data);
    size_t written = 0;
    while (written < size)
    {
        if (socket.waitUntilReady (false, 2000) <= 0)
            return false;

        const auto count = socket.write (bytes + written,
                                         static_cast<int> (juce::jmin (size - written,
                                                                       static_cast<size_t> (16384))));
        if (count <= 0)
            return false;
        written += static_cast<size_t> (count);
    }
    return true;
}

void sendResponse (juce::StreamingSocket& socket,
                   int status,
                   const juce::String& statusText,
                   const juce::String& contentType,
                   const juce::String& body,
                   const juce::String& extraHeaders = {})
{
    juce::MemoryBlock bodyBytes;
    bodyBytes.append (body.toRawUTF8(), body.getNumBytesAsUTF8());

    auto header = "HTTP/1.1 " + juce::String (status) + " " + statusText + "\r\n"
                + "Content-Type: " + contentType + "\r\n"
                + "Content-Length: " + juce::String (bodyBytes.getSize()) + "\r\n"
                + "Cache-Control: no-store\r\n"
                + "X-Content-Type-Options: nosniff\r\n"
                + "X-Frame-Options: DENY\r\n"
                + "Referrer-Policy: no-referrer\r\n"
                + "Connection: close\r\n"
                + extraHeaders
                + "\r\n";

    writeAll (socket, header.toRawUTF8(), header.getNumBytesAsUTF8());
    if (bodyBytes.getSize() > 0)
        writeAll (socket, bodyBytes.getData(), bodyBytes.getSize());
}

void sendJson (juce::StreamingSocket& socket, int status, const juce::String& body)
{
    sendResponse (socket, status, status == 200 ? "OK" : (status == 202 ? "Accepted" : "Error"),
                  "application/json; charset=utf-8", body);
}
}

RemoteControlServer::RemoteControlServer()
    : Thread ("DeFeedback LAN Remote")
{
}

RemoteControlServer::~RemoteControlServer()
{
    stopServer();
}

bool RemoteControlServer::startServer (int port,
                                       const juce::String& accessCode,
                                       CommandSink sink,
                                       juce::String& error)
{
    stopServer();

    if (port < 1024 || port > 65535)
    {
        error = "Remote port must be between 1024 and 65535.";
        return false;
    }

    if (accessCode.length() != 8 || ! accessCode.containsOnly ("0123456789"))
    {
        error = "Remote access code must contain exactly eight digits.";
        return false;
    }

    auto nextListener = std::make_unique<juce::StreamingSocket>();
    if (! nextListener->createListener (port))
    {
        error = "Could not listen on TCP port " + juce::String (port)
              + ". Another app may already be using it.";
        return false;
    }

    {
        const juce::ScopedLock lock (authLock);
        currentAccessCode = accessCode;
        sessionTokens.clear();
    }

    commandHandler = std::move (sink);
    listener = std::move (nextListener);
    listeningPort.store (port);
    listening.store (true);
    if (! startThread())
    {
        error = "Could not start the LAN remote server thread.";
        stopServer();
        return false;
    }

    return true;
}

void RemoteControlServer::stopServer()
{
    signalThreadShouldExit();
    if (listener != nullptr)
        listener->close();
    waitForThreadToExit (5000);
    listener.reset();
    commandHandler = {};
    listening.store (false);
    listeningPort.store (0);
    clearSessions();
}

void RemoteControlServer::publishState (const juce::var& state)
{
    const auto json = juce::JSON::toString (state, false);
    const juce::ScopedLock lock (stateLock);
    currentStateJson = json;
}

juce::String RemoteControlServer::generateAccessCode()
{
   #if JUCE_MAC
    const auto number = static_cast<int> (::arc4random_uniform (100000000u));
   #else
    const auto number = juce::Random::getSystemRandom().nextInt (juce::Range<int> (0, 100000000));
   #endif
    return juce::String (number).paddedLeft ('0', 8);
}

juce::StringArray RemoteControlServer::getDisplayUrls() const
{
    juce::StringArray result;
    if (! isListening())
        return result;

    for (const auto& address : juce::IPAddress::getAllAddresses (false))
    {
        const auto host = address.toString();
        if (! address.isIPv6 && host != "0.0.0.0" && host != "127.0.0.1")
            result.addIfNotAlreadyThere ("http://" + host + ":" + juce::String (getPort()));
    }

    if (result.isEmpty())
        result.add ("http://127.0.0.1:" + juce::String (getPort()));
    return result;
}

void RemoteControlServer::run()
{
    while (! threadShouldExit())
    {
        std::unique_ptr<juce::StreamingSocket> client (listener != nullptr
                                                        ? listener->waitForNextConnection()
                                                        : nullptr);
        if (client == nullptr)
            continue;
        handleClient (*client);
    }
}

void RemoteControlServer::handleClient (juce::StreamingSocket& socket)
{
    HttpRequest request;
    if (! readRequest (socket, request))
    {
        sendJson (socket, 400, R"({"error":"Invalid request"})");
        return;
    }

    if (request.method == "GET" && request.path == "/")
    {
        sendResponse (socket, 200, "OK", "text/html; charset=utf-8", remoteControlPageHtml(),
                      "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; "
                      "script-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:\r\n");
        return;
    }

    if (request.method == "POST" && request.path == "/api/login")
    {
        juce::var parsed;
        const auto parseResult = juce::JSON::parse (request.body, parsed);
        const auto suppliedCode = parsed.getProperty ("code", {}).toString();
        juce::String expectedCode;
        {
            const juce::ScopedLock lock (authLock);
            expectedCode = currentAccessCode;
        }

        if (parseResult.failed() || ! constantTimeEquals (suppliedCode, expectedCode))
        {
            juce::Thread::sleep (350);
            sendJson (socket, 401, R"({"error":"Incorrect access code"})");
            return;
        }

        auto response = std::make_unique<juce::DynamicObject>();
        response->setProperty ("token", createSessionToken());
        sendJson (socket, 200, juce::JSON::toString (juce::var (response.release()), false));
        return;
    }

    if (! isAuthorised (request.authorisation))
    {
        sendJson (socket, 401, R"({"error":"Authentication required"})");
        return;
    }

    if (request.method == "POST" && request.path == "/api/logout")
    {
        revokeSession (request.authorisation);
        sendJson (socket, 200, R"({"loggedOut":true})");
        return;
    }

    if (request.method == "GET" && request.path == "/api/state")
    {
        juce::String state;
        {
            const juce::ScopedLock lock (stateLock);
            state = currentStateJson;
        }
        sendJson (socket, 200, state);
        return;
    }

    if (request.method == "POST" && request.path == "/api/command")
    {
        juce::var command;
        const auto parseResult = juce::JSON::parse (request.body, command);
        if (parseResult.failed() || ! command.isObject())
        {
            sendJson (socket, 400, R"({"error":"Command must be a JSON object"})");
            return;
        }

        if (commandHandler != nullptr)
            commandHandler (command);
        sendJson (socket, 202, R"({"accepted":true})");
        return;
    }

    sendJson (socket, 404, R"({"error":"Not found"})");
}

bool RemoteControlServer::isAuthorised (const juce::String& header) const
{
    if (! header.startsWithIgnoreCase ("Bearer "))
        return false;

    const auto token = header.substring (7).trim();
    const juce::ScopedLock lock (authLock);
    for (const auto& session : sessionTokens)
        if (constantTimeEquals (token, session))
            return true;
    return false;
}

juce::String RemoteControlServer::createSessionToken()
{
   #if JUCE_MAC
    std::array<unsigned char, 32> randomBytes {};
    ::arc4random_buf (randomBytes.data(), randomBytes.size());
    const auto token = juce::String::toHexString (randomBytes.data(),
                                                   static_cast<int> (randomBytes.size()),
                                                   0);
   #else
    const auto token = juce::Uuid().toString().removeCharacters ("-")
                     + juce::Uuid().toString().removeCharacters ("-");
   #endif
    const juce::ScopedLock lock (authLock);
    sessionTokens.add (token);
    while (sessionTokens.size() > 8)
        sessionTokens.remove (0);
    return token;
}

void RemoteControlServer::revokeSession (const juce::String& header)
{
    const auto token = header.startsWithIgnoreCase ("Bearer ")
                     ? header.substring (7).trim()
                     : juce::String {};
    const juce::ScopedLock lock (authLock);
    for (int index = sessionTokens.size(); --index >= 0;)
        if (constantTimeEquals (token, sessionTokens[index]))
            sessionTokens.remove (index);
}

void RemoteControlServer::clearSessions()
{
    const juce::ScopedLock lock (authLock);
    sessionTokens.clear();
    currentAccessCode.clear();
}
}
