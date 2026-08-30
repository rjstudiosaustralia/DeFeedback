#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace defeedback
{
class PluginWindow final : public juce::DocumentWindow
{
public:
    using ClosedCallback = std::function<void(int, const juce::String&)>;

    PluginWindow (juce::AudioProcessorGraph::Node::Ptr,
                  juce::OwnedArray<PluginWindow>& activeWindows,
                  int laneNumber,
                  int laneId,
                  const juce::String& savedWindowState,
                  ClosedCallback);
    ~PluginWindow() override;

    void closeButtonPressed() override;
    juce::AudioProcessorGraph::Node::Ptr getNode() const { return node; }
    int getLaneId() const noexcept { return id; }
    juce::String captureWindowState() { return getWindowStateAsString(); }

private:
    juce::OwnedArray<PluginWindow>& windows;
    juce::AudioProcessorGraph::Node::Ptr node;
    ClosedCallback onUserClosed;
    int id = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
}
