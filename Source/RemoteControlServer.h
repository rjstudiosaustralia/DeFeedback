#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

namespace defeedback
{
class RemoteControlServer final : private juce::Thread
{
public:
    using CommandSink = std::function<void(const juce::var&)>;

    RemoteControlServer();
    ~RemoteControlServer() override;

    bool startServer (int port,
                      const juce::String& accessCode,
                      bool accessCodeRequired,
                      const juce::String& browserToken,
                      CommandSink commandSink,
                      juce::String& error);
    void stopServer();

    bool isListening() const noexcept { return listening.load(); }
    int getPort() const noexcept { return listeningPort.load(); }
    juce::StringArray getDisplayUrls() const;

    void publishState (const juce::var& state);

    static juce::String generateAccessCode();
    static juce::String generateBrowserToken();
    static bool validateAccessCode (const juce::String&, juce::String& error);

private:
    void run() override;
    void handleClient (juce::StreamingSocket&);
    bool isAuthorised (const juce::String& authorisationHeader,
                       const juce::String& cookieHeader) const;
    juce::String getCookieName() const;

    std::unique_ptr<juce::StreamingSocket> listener;
    CommandSink commandHandler;

    mutable juce::CriticalSection stateLock;
    juce::String currentStateJson { "{}" };

    mutable juce::CriticalSection authLock;
    juce::String currentAccessCode;
    juce::String currentBrowserToken;
    bool currentAccessCodeRequired = true;

    std::atomic<bool> listening { false };
    std::atomic<int> listeningPort { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RemoteControlServer)
};
}
