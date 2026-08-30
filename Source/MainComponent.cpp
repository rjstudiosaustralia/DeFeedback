#include "MainComponent.h"

#include "LoginItem.h"

namespace defeedback
{
namespace
{
const auto background = juce::Colour (0xff101316);
const auto panel = juce::Colour (0xff1b2025);
const auto border = juce::Colour (0xff343c44);
const auto text = juce::Colour (0xffedf2f5);
const auto mutedText = juce::Colour (0xff9ba8b2);
const auto green = juce::Colour (0xff47d18c);
const auto amber = juce::Colour (0xffffb454);
const auto red = juce::Colour (0xffff5d62);

class PeakMeter final : public juce::Component
{
public:
    void setLevel (float newLevel)
    {
        level = juce::jlimit (0.0f, 1.0f, newLevel);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0b0d0f));
        g.fillRoundedRectangle (bounds, 3.0f);

        const auto width = bounds.getWidth() * std::sqrt (level);
        g.setColour (level > 0.9f ? red : (level > 0.65f ? amber : green));
        g.fillRoundedRectangle (bounds.withWidth (width), 3.0f);
    }

private:
    float level = 0.0f;
};
}

class LaneRow final : public juce::Component
{
public:
    LaneRow (int rowIndex,
             const LaneConfig& initial,
             const juce::StringArray& inputNames,
             const juce::StringArray& outputNames)
        : index (rowIndex), config (initial)
    {
        numberLabel.setText (juce::String (rowIndex + 1), juce::dontSendNotification);
        numberLabel.setJustificationType (juce::Justification::centred);
        numberLabel.setColour (juce::Label::textColourId, mutedText);
        addAndMakeVisible (numberLabel);

        nameEditor.setText (config.name, false);
        nameEditor.setSelectAllWhenFocused (true);
        nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff101316));
        nameEditor.setColour (juce::TextEditor::outlineColourId, border);
        nameEditor.onFocusLost = [this] { commit(); };
        nameEditor.onReturnKey = [this] { commit(); };
        addAndMakeVisible (nameEditor);

        populateChannels (inputCombo, inputNames, config.inputChannel, "Input");
        populateChannels (outputCombo, outputNames, config.outputChannel, "Output");
        inputCombo.onChange = [this] { commit(); };
        outputCombo.onChange = [this] { commit(); };
        addAndMakeVisible (inputCombo);
        addAndMakeVisible (outputCombo);

        dryToggle.setToggleState (config.dry, juce::dontSendNotification);
        dryToggle.setColour (juce::ToggleButton::textColourId, amber);
        dryToggle.onClick = [this] { commit(); };
        addAndMakeVisible (dryToggle);

        statusLabel.setText ("WAITING", juce::dontSendNotification);
        statusLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        statusLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statusLabel);

        editorButton.onClick = [this]
        {
            if (onOpenEditor != nullptr)
                onOpenEditor (index);
        };
        addAndMakeVisible (editorButton);

        removeButton.setColour (juce::TextButton::textColourOffId, red);
        removeButton.onClick = [this]
        {
            if (onRemove != nullptr)
                onRemove (index);
        };
        addAndMakeVisible (removeButton);
        addAndMakeVisible (meter);
    }

    LaneConfig getConfig() const
    {
        auto result = config;
        result.name = nameEditor.getText().trim().isNotEmpty()
                    ? nameEditor.getText().trim()
                    : "Vocal " + juce::String (index + 1);
        result.inputChannel = juce::jmax (0, inputCombo.getSelectedId() - 1);
        result.outputChannel = juce::jmax (0, outputCombo.getSelectedId() - 1);
        result.dry = dryToggle.getToggleState();
        return result;
    }

    void setStatus (const LaneStatus& status)
    {
        statusLabel.setText (status.text, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, status.isDryFallback ? amber : green);
        editorButton.setEnabled (status.editorAvailable);
        meter.setLevel (status.peak);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8, 6);
        numberLabel.setBounds (area.removeFromLeft (34));
        area.removeFromLeft (6);
        nameEditor.setBounds (area.removeFromLeft (130));
        area.removeFromLeft (8);
        inputCombo.setBounds (area.removeFromLeft (155));
        area.removeFromLeft (8);
        outputCombo.setBounds (area.removeFromLeft (155));
        area.removeFromLeft (8);
        dryToggle.setBounds (area.removeFromLeft (92));
        area.removeFromLeft (8);
        meter.setBounds (area.removeFromLeft (90).withSizeKeepingCentre (90, 10));
        area.removeFromLeft (12);
        removeButton.setBounds (area.removeFromRight (34));
        area.removeFromRight (8);
        editorButton.setBounds (area.removeFromRight (76));
        area.removeFromRight (10);
        statusLabel.setBounds (area);
    }

    std::function<void()> onChanged;
    std::function<void(int)> onOpenEditor;
    std::function<void(int)> onRemove;

private:
    static void populateChannels (juce::ComboBox& combo,
                                  const juce::StringArray& names,
                                  int selectedChannel,
                                  const juce::String& prefix)
    {
        const auto requiredCount = juce::jmax (names.size(), selectedChannel + 1);
        for (int channel = 0; channel < requiredCount; ++channel)
        {
            const auto hardwareName = juce::isPositiveAndBelow (channel, names.size())
                                    ? names[channel]
                                    : "Unavailable";
            combo.addItem (prefix + " " + juce::String (channel + 1) + " — " + hardwareName,
                           channel + 1);
        }
        combo.setSelectedId (selectedChannel + 1, juce::dontSendNotification);
    }

    void commit()
    {
        config = getConfig();
        if (onChanged != nullptr)
            onChanged();
    }

    int index;
    LaneConfig config;
    juce::Label numberLabel;
    juce::TextEditor nameEditor;
    juce::ComboBox inputCombo;
    juce::ComboBox outputCombo;
    juce::ToggleButton dryToggle { "DRY PASS" };
    juce::Label statusLabel;
    PeakMeter meter;
    juce::TextButton editorButton { "OPEN UI" };
    juce::TextButton removeButton { "X" };
};

MainComponent::MainComponent (bool shouldUseSafeLaunch)
    : config (settings.load()), safeLaunch (shouldUseSafeLaunch)
{
    setOpaque (true);

    titleLabel.setText ("DEFEEDBACK LIVE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (27.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Ten-lane Apple Silicon live Audio Unit host", juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, mutedText);
    addAndMakeVisible (subtitleLabel);

    for (auto* label : { &deviceLabel, &rateLabel, &bufferLabel })
    {
        label->setColour (juce::Label::textColourId, mutedText);
        label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (*label);
    }
    deviceLabel.setText ("CORE AUDIO DEVICE", juce::dontSendNotification);
    rateLabel.setText ("SAMPLE RATE", juce::dontSendNotification);
    bufferLabel.setText ("BUFFER", juce::dontSendNotification);

    for (auto* combo : { &deviceCombo, &rateCombo, &bufferCombo })
        addAndMakeVisible (*combo);

    deviceCombo.onChange = [this]
    {
        if (refreshingControls || ! juce::isPositiveAndBelow (deviceCombo.getSelectedItemIndex(), deviceChoices.size()))
            return;

        const auto error = engine.selectDevice (deviceChoices.getReference (deviceCombo.getSelectedItemIndex()));
        showMessage (error.isEmpty() ? "Audio device changed." : error, error.isNotEmpty());
        refreshAllControls();
        saveConfig();
    };

    rateCombo.onChange = [this]
    {
        if (refreshingControls || ! juce::isPositiveAndBelow (rateCombo.getSelectedItemIndex(), sampleRates.size()))
            return;

        const auto error = engine.setSampleRate (sampleRates[rateCombo.getSelectedItemIndex()]);
        showMessage (error.isEmpty() ? "Sample rate changed." : error, error.isNotEmpty());
        refreshAllControls();
        saveConfig();
    };

    bufferCombo.onChange = [this]
    {
        if (refreshingControls || ! juce::isPositiveAndBelow (bufferCombo.getSelectedItemIndex(), bufferSizes.size()))
            return;

        const auto error = engine.setBufferSize (bufferSizes[bufferCombo.getSelectedItemIndex()]);
        showMessage (error.isEmpty() ? "Buffer changed." : error, error.isNotEmpty());
        refreshAllControls();
        saveConfig();
    };

    startStopButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff236b4b));
    startStopButton.onClick = [this] { startOrStop(); };
    addAndMakeVisible (startStopButton);

    emergencyButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8f2f33));
    emergencyButton.onClick = [this]
    {
        engine.setEmergencyMuted (! engine.isEmergencyMuted());
        updateRuntimeStatus();
    };
    addAndMakeVisible (emergencyButton);

    addLaneButton.onClick = [this]
    {
        if (config.lanes.size() >= maxLanes)
        {
            showMessage ("The live rack is limited to ten lanes.", true);
            return;
        }

        const auto id = config.lanes.size() + 1;
        config.lanes.add ({ id, "Vocal " + juce::String (id), id - 1, id - 1, false, {} });
        rebuildLaneRows();
        applyLanes();
    };
    addAndMakeVisible (addLaneButton);

    autoStartToggle.setToggleState (config.autoStart, juce::dontSendNotification);
    autoStartToggle.onClick = [this]
    {
        config.autoStart = autoStartToggle.getToggleState();
        saveConfig();
    };
    addAndMakeVisible (autoStartToggle);

    launchAtLoginToggle.setToggleState (config.launchAtLogin, juce::dontSendNotification);
    launchAtLoginToggle.onClick = [this]
    {
        juce::String error;
        const auto enabled = launchAtLoginToggle.getToggleState();
        if (! setLaunchAtLogin (enabled, error))
        {
            launchAtLoginToggle.setToggleState (! enabled, juce::dontSendNotification);
            showMessage ("Launch at login: " + error, true);
            return;
        }

        config.launchAtLogin = enabled;
        saveConfig();
    };
    addAndMakeVisible (launchAtLoginToggle);

    metricsLabel.setJustificationType (juce::Justification::centredRight);
    metricsLabel.setColour (juce::Label::textColourId, text);
    metricsLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (metricsLabel);

    pluginLabel.setColour (juce::Label::textColourId, mutedText);
    pluginLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (pluginLabel);

    alertLabel.setJustificationType (juce::Justification::centred);
    alertLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (alertLabel);

    laneHeaderLabel.setText ("#     NAME                         INPUT                         OUTPUT                 MODE                 LEVEL             STATUS",
                             juce::dontSendNotification);
    laneHeaderLabel.setColour (juce::Label::textColourId, mutedText);
    laneHeaderLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (laneHeaderLabel);

    laneViewport.setViewedComponent (&laneContainer, false);
    laneViewport.setScrollBarsShown (true, false);
    laneViewport.setColour (juce::ScrollBar::thumbColourId, border);
    addAndMakeVisible (laneViewport);

    engine.addChangeListener (this);
    const auto initialiseError = engine.initialise (config);
    showMessage (initialiseError.isEmpty() ? engine.getPluginDiagnostic() : initialiseError,
                 initialiseError.isNotEmpty());

    refreshAllControls();
    rebuildLaneRows();

    if (! safeLaunch
        && config.launchAtLogin
        && getLoginItemStatus() == LoginItemStatus::disabled)
    {
        juce::String loginError;
        if (! setLaunchAtLogin (true, loginError) && loginError.isNotEmpty())
            showMessage ("Launch at login needs attention: " + loginError, true);
    }

    setSize (1080, 720);
    startTimerHz (10);

    if (config.autoStart && ! safeLaunch)
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe != nullptr && ! safe->engine.isRunning())
                safe->startOrStop();
        });
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.removeChangeListener (this);
    saveConfig();
    engine.stop();
    laneRows.clear();
    laneViewport.setViewedComponent (nullptr, false);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto topPanel = juce::Rectangle<int> (20, 76, getWidth() - 40, 146).toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (topPanel, 8.0f);
    g.setColour (border);
    g.drawRoundedRectangle (topPanel, 8.0f, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto heading = area.removeFromTop (46);
    titleLabel.setBounds (heading.removeFromLeft (360));
    subtitleLabel.setBounds (heading);
    area.removeFromTop (10);

    auto setupPanel = area.removeFromTop (146).reduced (16, 12);
    auto firstLine = setupPanel.removeFromTop (54);

    auto deviceArea = firstLine.removeFromLeft (390);
    deviceLabel.setBounds (deviceArea.removeFromTop (18));
    deviceCombo.setBounds (deviceArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto rateArea = firstLine.removeFromLeft (140);
    rateLabel.setBounds (rateArea.removeFromTop (18));
    rateCombo.setBounds (rateArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto bufferArea = firstLine.removeFromLeft (140);
    bufferLabel.setBounds (bufferArea.removeFromTop (18));
    bufferCombo.setBounds (bufferArea.removeFromTop (30));
    firstLine.removeFromLeft (18);

    startStopButton.setBounds (firstLine.removeFromLeft (130).reduced (0, 3));
    firstLine.removeFromLeft (10);
    emergencyButton.setBounds (firstLine.reduced (0, 3));

    auto secondLine = setupPanel.removeFromTop (38);
    autoStartToggle.setBounds (secondLine.removeFromLeft (155));
    launchAtLoginToggle.setBounds (secondLine.removeFromLeft (145));
    metricsLabel.setBounds (secondLine);

    pluginLabel.setBounds (setupPanel.removeFromTop (24));
    area.removeFromTop (10);
    alertLabel.setBounds (area.removeFromTop (34));
    area.removeFromTop (8);

    auto laneTop = area.removeFromTop (32);
    addLaneButton.setBounds (laneTop.removeFromRight (120));
    laneHeaderLabel.setBounds (laneTop);
    area.removeFromTop (4);
    laneViewport.setBounds (area);

    const auto rowHeight = 52;
    laneContainer.setSize (laneViewport.getMaximumVisibleWidth(), juce::jmax (area.getHeight(), laneRows.size() * rowHeight));
    for (int index = 0; index < laneRows.size(); ++index)
        laneRows[index]->setBounds (0, index * rowHeight, laneContainer.getWidth(), rowHeight - 2);
}

void MainComponent::timerCallback()
{
    updateRuntimeStatus();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshDeviceControls();
    updateRuntimeStatus();
}

void MainComponent::refreshAllControls()
{
    autoStartToggle.setToggleState (config.autoStart, juce::dontSendNotification);
    launchAtLoginToggle.setToggleState (config.launchAtLogin, juce::dontSendNotification);
    refreshDeviceControls();
    pluginLabel.setText (engine.getPluginDiagnostic(), juce::dontSendNotification);
    pluginLabel.setColour (juce::Label::textColourId, engine.isPluginAvailable() ? green : amber);
    updateRuntimeStatus();
}

void MainComponent::refreshDeviceControls()
{
    const juce::ScopedValueSetter<bool> guard (refreshingControls, true);

    deviceChoices = engine.getDeviceChoices();
    deviceCombo.clear (juce::dontSendNotification);
    const auto current = engine.getCurrentDeviceChoice();
    for (int index = 0; index < deviceChoices.size(); ++index)
    {
        deviceCombo.addItem (deviceChoices[index].displayName, index + 1);
        if (deviceChoices[index].inputName == current.inputName
            && deviceChoices[index].outputName == current.outputName)
            deviceCombo.setSelectedId (index + 1, juce::dontSendNotification);
    }

    sampleRates = engine.getAvailableSampleRates();
    rateCombo.clear (juce::dontSendNotification);
    for (int index = 0; index < sampleRates.size(); ++index)
    {
        rateCombo.addItem (juce::String (sampleRates[index] / 1000.0, 1) + " kHz", index + 1);
        if (std::abs (sampleRates[index] - engine.getCurrentSampleRate()) < 0.1)
            rateCombo.setSelectedId (index + 1, juce::dontSendNotification);
    }

    bufferSizes = engine.getAvailableBufferSizes();
    bufferCombo.clear (juce::dontSendNotification);
    for (int index = 0; index < bufferSizes.size(); ++index)
    {
        bufferCombo.addItem (juce::String (bufferSizes[index]) + " samples", index + 1);
        if (bufferSizes[index] == engine.getCurrentBufferSize())
            bufferCombo.setSelectedId (index + 1, juce::dontSendNotification);
    }
}

void MainComponent::rebuildLaneRows()
{
    laneRows.clear();
    laneContainer.removeAllChildren();

    const auto inputNames = engine.getInputChannelNames();
    const auto outputNames = engine.getOutputChannelNames();

    for (int index = 0; index < config.lanes.size(); ++index)
    {
        auto* row = laneRows.add (new LaneRow (index, config.lanes[index], inputNames, outputNames));
        row->onChanged = [this] { applyLanes(); };
        row->onOpenEditor = [this] (int lane) { engine.openPluginEditor (lane); };
        row->onRemove = [this] (int lane)
        {
            if (config.lanes.size() <= 1)
            {
                showMessage ("At least one lane must remain in the rack.", true);
                return;
            }

            config.lanes.remove (lane);
            for (int i = 0; i < config.lanes.size(); ++i)
                config.lanes.getReference (i).id = i + 1;
            rebuildLaneRows();
            applyLanes();
        };
        laneContainer.addAndMakeVisible (row);
    }

    resized();
}

void MainComponent::applyLanes()
{
    juce::Array<LaneConfig> updated;
    for (int index = 0; index < laneRows.size(); ++index)
    {
        auto lane = laneRows[index]->getConfig();
        lane.id = index + 1;
        if (juce::isPositiveAndBelow (index, config.lanes.size()))
            lane.pluginStateBase64 = config.lanes[index].pluginStateBase64;
        updated.add (std::move (lane));
    }

    config.lanes = std::move (updated);
    const auto error = engine.setLanes (config.lanes);
    if (error.isNotEmpty())
        showMessage (error, true);
    saveConfig();
}

void MainComponent::saveConfig()
{
    const auto device = engine.getCurrentDeviceChoice();
    config.inputDeviceName = device.inputName;
    config.outputDeviceName = device.outputName;
    config.sampleRate = engine.getCurrentSampleRate();
    config.bufferSize = engine.getCurrentBufferSize();
    config.lanes = engine.captureLanesWithPluginState();

    juce::String error;
    if (! settings.save (config, error))
        showMessage (error, true);
}

void MainComponent::showMessage (const juce::String& message, bool isError)
{
    if (message.isEmpty())
        return;

    alertLabel.setText (message, juce::dontSendNotification);
    alertLabel.setColour (juce::Label::backgroundColourId,
                          isError ? juce::Colour (0xff5b2528) : juce::Colour (0xff173d2e));
    alertLabel.setColour (juce::Label::textColourId, isError ? juce::Colour (0xffffc4c5) : juce::Colour (0xffaaf0cc));
}

void MainComponent::startOrStop()
{
    if (engine.isRunning())
    {
        engine.stop();
        showMessage ("Audio stopped. Outputs are silent.", false);
    }
    else
    {
        const auto error = engine.start();
        showMessage (error.isEmpty() ? "Audio is running." : error, error.isNotEmpty());
    }

    updateRuntimeStatus();
    saveConfig();
}

void MainComponent::updateRuntimeStatus()
{
    const auto statuses = engine.getLaneStatuses();
    auto dryCount = 0;
    for (int index = 0; index < laneRows.size(); ++index)
    {
        if (juce::isPositiveAndBelow (index, statuses.size()))
        {
            laneRows[index]->setStatus (statuses[index]);
            if (statuses[index].isDryFallback)
                ++dryCount;
        }
    }

    startStopButton.setButtonText (engine.isRunning() ? "STOP AUDIO" : "START AUDIO");
    startStopButton.setColour (juce::TextButton::buttonColourId,
                               engine.isRunning() ? juce::Colour (0xff6e4d24) : juce::Colour (0xff236b4b));

    emergencyButton.setButtonText (engine.isEmergencyMuted() ? "UNMUTE OUTPUTS" : "MUTE EVERYTHING");
    emergencyButton.setColour (juce::TextButton::buttonColourId,
                               engine.isEmergencyMuted() ? juce::Colour (0xff2d6f52) : juce::Colour (0xff8f2f33));

    metricsLabel.setText ("LATENCY " + juce::String (engine.getEstimatedRoundTripMilliseconds(), 1)
                              + " ms     CPU " + juce::String (engine.getCpuUsage() * 100.0, 1)
                              + "%     XRUNS " + juce::String (engine.getXRunCount()),
                          juce::dontSendNotification);

    if (engine.isEmergencyMuted())
    {
        alertLabel.setText ("ALL OUTPUTS MUTED", juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff5b2528));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffc4c5));
    }
    else if (dryCount > 0)
    {
        alertLabel.setText ("DRY PASS ACTIVE ON " + juce::String (dryCount)
                                + (dryCount == 1 ? " LANE — " : " LANES — ")
                                + "FEEDBACK PROTECTION IS OFF",
                            juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff65421d));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffddb0));
    }
    else if (engine.isRunning())
    {
        alertLabel.setText ("ALL LANES PROCESSED", juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff173d2e));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaaf0cc));
    }
    else
    {
        alertLabel.setText ("AUDIO STOPPED — ROUTES READY", juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff27313a));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc7d2da));
    }
}
}
