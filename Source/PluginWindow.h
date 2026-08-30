#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace defeedback
{
class PluginWindow final : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioProcessorGraph::Node::Ptr,
                  juce::OwnedArray<PluginWindow>& activeWindows,
                  int laneNumber);
    ~PluginWindow() override;

    void closeButtonPressed() override;
    juce::AudioProcessorGraph::Node::Ptr getNode() const { return node; }

private:
    juce::OwnedArray<PluginWindow>& windows;
    juce::AudioProcessorGraph::Node::Ptr node;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
}
