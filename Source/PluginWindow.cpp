#include "PluginWindow.h"

namespace defeedback
{
PluginWindow::PluginWindow (juce::AudioProcessorGraph::Node::Ptr processorNode,
                            juce::OwnedArray<PluginWindow>& activeWindows,
                            int laneNumber,
                            int laneId,
                            const juce::String& savedWindowState,
                            ClosedCallback closedCallback)
    : DocumentWindow ("De-Feedback — Lane " + juce::String (laneNumber),
                      juce::Colours::black,
                      minimiseButton | closeButton),
      windows (activeWindows),
      node (std::move (processorNode)),
      onUserClosed (std::move (closedCallback)),
      id (laneId)
{
    setUsingNativeTitleBar (true);
    setAlwaysOnTop (false);

    if (node != nullptr)
    {
        if (auto* editor = node->getProcessor()->createEditorAndMakeActive())
        {
            setContentOwned (editor, true);
            setResizable (editor->isResizable(), false);
        }
    }

    if (getContentComponent() == nullptr)
    {
        auto* message = new juce::Label();
        message->setText ("The plugin did not provide an editor.", juce::dontSendNotification);
        message->setJustificationType (juce::Justification::centred);
        setContentOwned (message, true);
        setSize (420, 180);
    }

    if (savedWindowState.isEmpty() || ! restoreWindowStateFromString (savedWindowState))
        centreWithSize (getWidth(), getHeight());

    setVisible (true);
    toFront (true);
}

PluginWindow::~PluginWindow()
{
    clearContentComponent();
}

void PluginWindow::closeButtonPressed()
{
    if (onUserClosed != nullptr)
        onUserClosed (id, getWindowStateAsString());

    windows.removeObject (this);
}
}
