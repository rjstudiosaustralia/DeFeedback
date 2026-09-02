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

#if DEFEEDBACK_UI_PREVIEW
constexpr int previewChannelCount = 16;
#endif

AppConfig loadInitialConfig (SettingsStore& settings)
{
   #if DEFEEDBACK_UI_PREVIEW
    juce::ignoreUnused (settings);
    AppConfig preview;
    preview.autoStart = false;
    preview.launchAtLogin = false;
    preview.remoteControlEnabled = true;
    preview.remoteAccessCode = "12345678";
    preview.lanes.clear();
    preview.lanes.add ({ 1, "Lead Vocal", 0, 0, false, {}, false, {} });
    preview.lanes.add ({ 2, "Guest Vocal", 1, 1, false, {}, false, {} });
    preview.lanes.add ({ 3, "Bypassed Vocal", 2, 2, true, {}, false, {} });
    preview.lanes.add ({ 4, "Route Check", 3, 3, false, {}, false, {} });
    return preview;
   #else
    return settings.load();
   #endif
}

struct LaneGeometry
{
    juce::Rectangle<int> number;
    juce::Rectangle<int> name;
    juce::Rectangle<int> input;
    juce::Rectangle<int> inputMeter;
    juce::Rectangle<int> strength;
    juce::Rectangle<int> editor;
    juce::Rectangle<int> pluginMute;
    juce::Rectangle<int> bypass;
    juce::Rectangle<int> outputMeter;
    juce::Rectangle<int> output;
    juce::Rectangle<int> status;
    juce::Rectangle<int> remove;
};

LaneGeometry calculateLaneGeometry (juce::Rectangle<int> bounds)
{
    LaneGeometry layout;
    auto area = bounds.reduced (8, 0);

    layout.number = area.removeFromLeft (30);
    area.removeFromLeft (6);
    layout.name = area.removeFromLeft (112);
    area.removeFromLeft (8);
    layout.input = area.removeFromLeft (140);
    area.removeFromLeft (8);
    layout.inputMeter = area.removeFromLeft (70);
    area.removeFromLeft (10);

    layout.remove = area.removeFromRight (32);
    area.removeFromRight (6);
    layout.status = area.removeFromRight (120);
    area.removeFromRight (8);
    layout.output = area.removeFromRight (140);
    area.removeFromRight (8);
    layout.outputMeter = area.removeFromRight (70);
    area.removeFromRight (10);
    layout.bypass = area.removeFromRight (82);
    area.removeFromRight (8);
    layout.pluginMute = area.removeFromRight (82);
    area.removeFromRight (8);
    layout.editor = area.removeFromRight (48);
    area.removeFromRight (10);
    layout.strength = area;

    return layout;
}

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

class LaneHeader final : public juce::Component
{
public:
    LaneHeader()
    {
        initialise (number, "#", juce::Justification::centred);
        initialise (name, "NAME");
        initialise (input, "INPUT");
        initialise (inputLevel, "INPUT LEVEL", juce::Justification::centred);
        initialise (strength, "STRENGTH");
        initialise (editor, "UI", juce::Justification::centred);
        initialise (pluginMute, "PLUGIN MUTE", juce::Justification::centred);
        initialise (bypass, "BYPASS", juce::Justification::centred);
        initialise (outputLevel, "OUTPUT LEVEL", juce::Justification::centred);
        initialise (output, "OUTPUT");
        initialise (status, "STATE");
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (border);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void resized() override
    {
        const auto layout = calculateLaneGeometry (getLocalBounds());
        number.setBounds (layout.number);
        name.setBounds (layout.name);
        input.setBounds (layout.input);
        inputLevel.setBounds (layout.inputMeter);
        strength.setBounds (layout.strength);
        editor.setBounds (layout.editor);
        pluginMute.setBounds (layout.pluginMute);
        bypass.setBounds (layout.bypass);
        outputLevel.setBounds (layout.outputMeter);
        output.setBounds (layout.output);
        status.setBounds (layout.status);
    }

private:
    void initialise (juce::Label& label,
                     const juce::String& labelText,
                     juce::Justification justification = juce::Justification::centredLeft)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, mutedText);
        label.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        label.setJustificationType (justification);
        addAndMakeVisible (label);
    }

    juce::Label number;
    juce::Label name;
    juce::Label input;
    juce::Label inputLevel;
    juce::Label strength;
    juce::Label editor;
    juce::Label pluginMute;
    juce::Label bypass;
    juce::Label outputLevel;
    juce::Label output;
    juce::Label status;
};

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
        dryToggle.setTooltip ("Bypass De-Feedback and pass the input dry.");
        dryToggle.onClick = [this] { commit(); };
        addAndMakeVisible (dryToggle);

        strengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        strengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        strengthSlider.setRange (0.0, 100.0, 0.1);
        strengthSlider.setTextValueSuffix ("%");
        strengthSlider.setValue (100.0, juce::dontSendNotification);
        strengthSlider.setTooltip ("De-Feedback Strength");
        strengthSlider.onValueChange = [this]
        {
            if (onStrengthChanged != nullptr)
                onStrengthChanged (index, static_cast<float> (strengthSlider.getValue() / 100.0));
            if (! strengthSlider.isMouseButtonDown() && onPluginControlCommitted != nullptr)
                onPluginControlCommitted();
        };
        strengthSlider.onDragEnd = [this]
        {
            if (onPluginControlCommitted != nullptr)
                onPluginControlCommitted();
        };
        addAndMakeVisible (strengthSlider);

        pluginMuteToggle.setColour (juce::ToggleButton::textColourId, red);
        pluginMuteToggle.setTooltip ("The De-Feedback plugin's own Mute parameter.");
        pluginMuteToggle.onClick = [this]
        {
            if (onPluginMuteChanged != nullptr)
                onPluginMuteChanged (index, pluginMuteToggle.getToggleState());
            if (onPluginControlCommitted != nullptr)
                onPluginControlCommitted();
        };
        addAndMakeVisible (pluginMuteToggle);

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
        addAndMakeVisible (inputMeter);
        addAndMakeVisible (outputMeter);
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

    void setExclusiveRouteChoices (const juce::Array<LaneConfig>& lanes)
    {
        for (int other = 0; other < lanes.size(); ++other)
        {
            if (other == index)
                continue;

            if (lanes[other].inputChannel != config.inputChannel)
                inputCombo.setItemEnabled (lanes[other].inputChannel + 1, false);

            if (lanes[other].outputChannel != config.outputChannel)
                outputCombo.setItemEnabled (lanes[other].outputChannel + 1, false);
        }
    }

    void setStatus (const LaneStatus& status, bool engineRunning, bool masterMuted)
    {
        auto displayStatus = status.text;
        auto nextAccent = green;

        if (! engineRunning)
        {
            displayStatus = "ENGINE STOPPED";
            nextAccent = mutedText;
        }
        else if (masterMuted)
        {
            displayStatus = "OUTPUT MUTED";
            nextAccent = red;
        }
        else if (status.pluginMuted)
        {
            displayStatus = "PLUGIN MUTED";
            nextAccent = red;
        }
        else if (status.text.containsIgnoreCase ("invalid")
                 || status.text.containsIgnoreCase ("duplicate"))
        {
            nextAccent = red;
        }
        else if (config.dry || status.isDryFallback)
        {
            displayStatus = config.dry ? "BYPASSED" : status.text;
            nextAccent = amber;
        }

        rowAccent = nextAccent;
        statusLabel.setText (displayStatus, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, rowAccent);
        editorButton.setEnabled (status.editorAvailable);
        strengthSlider.setEnabled (status.strengthAvailable);
        pluginMuteToggle.setEnabled (status.pluginMuteAvailable);
        if (! strengthSlider.isMouseButtonDown())
            strengthSlider.setValue (status.strengthNormalized * 100.0, juce::dontSendNotification);
        pluginMuteToggle.setToggleState (status.pluginMuted, juce::dontSendNotification);
        inputMeter.setLevel (status.inputPeak);
        outputMeter.setLevel (status.outputPeak);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().reduced (1).toFloat();
        g.setColour (rowAccent.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (rowAccent.withAlpha (0.38f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
        g.fillRoundedRectangle (bounds.withWidth (4.0f), 2.0f);
    }

    void resized() override
    {
        const auto layout = calculateLaneGeometry (getLocalBounds());
        numberLabel.setBounds (layout.number);
        nameEditor.setBounds (layout.name.withSizeKeepingCentre (layout.name.getWidth(), 34));
        inputCombo.setBounds (layout.input.withSizeKeepingCentre (layout.input.getWidth(), 34));
        inputMeter.setBounds (layout.inputMeter.withSizeKeepingCentre (layout.inputMeter.getWidth(), 10));
        strengthSlider.setBounds (layout.strength.withSizeKeepingCentre (layout.strength.getWidth(), 30));
        editorButton.setBounds (layout.editor.withSizeKeepingCentre (layout.editor.getWidth(), 34));
        pluginMuteToggle.setBounds (layout.pluginMute.withSizeKeepingCentre (layout.pluginMute.getWidth(), 30));
        dryToggle.setBounds (layout.bypass.withSizeKeepingCentre (layout.bypass.getWidth(), 30));
        outputMeter.setBounds (layout.outputMeter.withSizeKeepingCentre (layout.outputMeter.getWidth(), 10));
        outputCombo.setBounds (layout.output.withSizeKeepingCentre (layout.output.getWidth(), 34));
        statusLabel.setBounds (layout.status);
        removeButton.setBounds (layout.remove.withSizeKeepingCentre (layout.remove.getWidth(), 34));
    }

    std::function<void()> onChanged;
    std::function<void()> onPluginControlCommitted;
    std::function<void(int)> onOpenEditor;
    std::function<void(int)> onRemove;
    std::function<void(int, float)> onStrengthChanged;
    std::function<void(int, bool)> onPluginMuteChanged;

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
            combo.addItem (prefix + " " + juce::String (channel + 1) + " - " + hardwareName,
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
    juce::ToggleButton dryToggle { "BYPASS" };
    juce::Label statusLabel;
    PeakMeter inputMeter;
    PeakMeter outputMeter;
    juce::Slider strengthSlider;
    juce::ToggleButton pluginMuteToggle { "MUTE" };
    juce::TextButton editorButton { "UI" };
    juce::TextButton removeButton { "X" };
    juce::Colour rowAccent { mutedText };
};

MainComponent::MainComponent (bool shouldUseSafeLaunch)
    : config (loadInitialConfig (settings)), safeLaunch (shouldUseSafeLaunch)
{
    setOpaque (true);

    titleLabel.setText ("DEFEEDBACK LIVE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (27.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Apple Silicon live Audio Unit host", juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, mutedText);
    addAndMakeVisible (subtitleLabel);

    aboutButton.setTooltip ("Independence, licence, warranty, and live-audio safety notice.");
    aboutButton.onClick = []
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "DeFeedback Live - About / Safety",
            "DeFeedback Live " JUCE_APPLICATION_VERSION_STRING "\n"
            "Copyright (c) 2026 Ryan Somerfield / RJ Studios Australia\n"
            "Open-source software licensed under GNU AGPLv3.\n\n"
            "Independent third-party host. Not affiliated with, sponsored by, approved by, or endorsed by Alpha Labs LLC. "
            "De-Feedback is separately installed and licensed.\n\n"
            "Engineering preview supplied without warranty. Use entirely at your own risk. "
            "Live feedback and routing errors can cause dangerous sound levels, hearing injury, or equipment damage. "
            "Begin muted, verify every route, and retain an independent hardware or console mute.\n\n"
            "The optional LAN remote has full live-audio control and uses unencrypted HTTP. "
            "Enable it only on a trusted private network; never expose TCP port 8765 to the internet or shared Wi-Fi.");
    };
    addAndMakeVisible (aboutButton);

    for (auto* label : { &deviceLabel, &rateLabel, &bufferLabel, &engineLabel, &outputSafetyLabel })
    {
        label->setColour (juce::Label::textColourId, mutedText);
        label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (*label);
    }
    deviceLabel.setText ("CORE AUDIO DEVICE", juce::dontSendNotification);
    rateLabel.setText ("SAMPLE RATE", juce::dontSendNotification);
    bufferLabel.setText ("BUFFER", juce::dontSendNotification);
    engineLabel.setText ("AUDIO ENGINE", juce::dontSendNotification);
    outputSafetyLabel.setText ("OUTPUT SAFETY", juce::dontSendNotification);

    for (auto* combo : { &deviceCombo, &rateCombo, &bufferCombo })
        addAndMakeVisible (*combo);

    refreshDevicesButton.setTooltip ("Rescan Core Audio after connecting or powering an interface.");
    refreshDevicesButton.onClick = [this]
    {
        engine.refreshDeviceList();
        refreshAllControls();
        rebuildLaneRows();
        showMessage ("Core Audio device list refreshed.", false);
    };
    addAndMakeVisible (refreshDevicesButton);

    deviceCombo.onChange = [this]
    {
        if (refreshingControls || ! juce::isPositiveAndBelow (deviceCombo.getSelectedItemIndex(), deviceChoices.size()))
            return;

        const auto error = engine.selectDevice (deviceChoices.getReference (deviceCombo.getSelectedItemIndex()));
        showMessage (error.isEmpty() ? "Audio device changed." : error, error.isNotEmpty());
        refreshAllControls();
        rebuildLaneRows();
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
    startStopButton.setTooltip ("Start or stop the complete audio engine. Stopping pauses every plugin and meter.");
    startStopButton.onClick = [this] { startOrStop(); };
    addAndMakeVisible (startStopButton);

    emergencyButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8f2f33));
    emergencyButton.setTooltip ("Master output gate. Plugins and input meters keep running; every host output is zeroed.");
    emergencyButton.onClick = [this]
    {
        engine.setEmergencyMuted (! engine.isEmergencyMuted());
        updateRuntimeStatus();
    };
    addAndMakeVisible (emergencyButton);

    resetXRunsButton.setTooltip ("Set the displayed session XRun count back to zero.");
    resetXRunsButton.onClick = [this]
    {
        engine.resetXRunCount();
        showMessage ("Session XRun counter reset.", false);
        updateRuntimeStatus();
    };
    addAndMakeVisible (resetXRunsButton);

    addLaneButton.onClick = [this] { addLane(); };
    addAndMakeVisible (addLaneButton);
    addLaneButton.setTooltip ("Add the next unused input/output pair. Capacity follows the selected Core Audio device.");

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

    remoteControlToggle.setToggleState (config.remoteControlEnabled, juce::dontSendNotification);
    remoteControlToggle.setTooltip ("Expose full control on the local network. Access requires the saved eight-digit code.");
    remoteControlToggle.onClick = [this]
    {
        const auto shouldEnable = remoteControlToggle.getToggleState();
        if (shouldEnable)
        {
            if (config.remoteAccessCode.isEmpty())
                config.remoteAccessCode = RemoteControlServer::generateAccessCode();

            config.remoteControlEnabled = startRemoteControl();
            remoteControlToggle.setToggleState (config.remoteControlEnabled, juce::dontSendNotification);
        }
        else
        {
            config.remoteControlEnabled = false;
            stopRemoteControl();
            showMessage ("Full-control LAN remote disabled.", false);
        }

        updateRemoteControls();
        saveConfig();
    };
    addAndMakeVisible (remoteControlToggle);

    remoteStatusLabel.setColour (juce::Label::textColourId, mutedText);
    remoteStatusLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    remoteStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (remoteStatusLabel);

    copyRemoteButton.setTooltip ("Copy the LAN address and access code to the clipboard.");
    copyRemoteButton.onClick = [this]
    {
        const auto urls = remoteControlServer.getDisplayUrls();
        if (urls.isEmpty())
        {
            showMessage ("Enable the LAN remote before copying its connection details.", true);
            return;
        }

        juce::SystemClipboard::copyTextToClipboard (urls[0] + "\nAccess code: " + config.remoteAccessCode);
        showMessage ("Remote address and access code copied.", false);
    };
    addAndMakeVisible (copyRemoteButton);

    newRemoteCodeButton.setTooltip ("Generate a new saved access code and disconnect every browser session.");
    newRemoteCodeButton.onClick = [this]
    {
        config.remoteAccessCode = RemoteControlServer::generateAccessCode();
        auto restartSucceeded = true;
        if (config.remoteControlEnabled)
        {
            restartSucceeded = startRemoteControl();
            config.remoteControlEnabled = restartSucceeded;
        }
        updateRemoteControls();
        saveConfig();
        if (restartSucceeded)
            showMessage ("New LAN remote access code generated. Existing browser sessions were disconnected.", false);
    };
    addAndMakeVisible (newRemoteCodeButton);

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

    routingHintLabel.setText ("SIGNAL FLOW  |  INPUT -> DE-FEEDBACK -> OUTPUT  |  ONE INPUT AND ONE OUTPUT PER LANE",
                              juce::dontSendNotification);
    routingHintLabel.setColour (juce::Label::textColourId, mutedText);
    routingHintLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (routingHintLabel);

    laneHeader = std::make_unique<LaneHeader>();
    addAndMakeVisible (*laneHeader);

    laneViewport.setViewedComponent (&laneContainer, false);
    laneViewport.setScrollBarsShown (true, false);
    laneViewport.setColour (juce::ScrollBar::thumbColourId, border);
    addAndMakeVisible (laneViewport);

   #if ! DEFEEDBACK_UI_PREVIEW
    engine.addChangeListener (this);
    const auto initialiseError = engine.initialise (config);
    showMessage (initialiseError.isEmpty() ? engine.getPluginDiagnostic() : initialiseError,
                 initialiseError.isNotEmpty());
   #endif

    refreshAllControls();
    rebuildLaneRows();
    updateRuntimeStatus();

    if (config.remoteControlEnabled)
    {
        if (config.remoteAccessCode.isEmpty())
            config.remoteAccessCode = RemoteControlServer::generateAccessCode();
        config.remoteControlEnabled = startRemoteControl();
    }
    updateRemoteControls();

    if (! safeLaunch
        && config.launchAtLogin
        && getLoginItemStatus() == LoginItemStatus::disabled)
    {
        juce::String loginError;
        if (! setLaunchAtLogin (true, loginError) && loginError.isNotEmpty())
            showMessage ("Launch at login needs attention: " + loginError, true);
    }

    setSize (1360, 760);
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
    stopRemoteControl();
   #if ! DEFEEDBACK_UI_PREVIEW
    engine.removeChangeListener (this);
    saveConfig();
    engine.stop();
   #endif
    laneRows.clear();
    laneViewport.setViewedComponent (nullptr, false);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto topPanel = juce::Rectangle<int> (20, 76, getWidth() - 40, 196).toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (topPanel, 8.0f);
    g.setColour (border);
    g.drawRoundedRectangle (topPanel, 8.0f, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto heading = area.removeFromTop (46);
    aboutButton.setBounds (heading.removeFromRight (150).reduced (4, 7));
    titleLabel.setBounds (heading.removeFromLeft (360));
    subtitleLabel.setBounds (heading);
    area.removeFromTop (10);

    auto setupPanel = area.removeFromTop (196).reduced (16, 12);
    auto firstLine = setupPanel.removeFromTop (54);

    auto deviceArea = firstLine.removeFromLeft (300);
    deviceLabel.setBounds (deviceArea.removeFromTop (18));
    deviceCombo.setBounds (deviceArea.removeFromTop (30));
    firstLine.removeFromLeft (8);

    auto refreshArea = firstLine.removeFromLeft (90);
    refreshArea.removeFromTop (18);
    refreshDevicesButton.setBounds (refreshArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto rateArea = firstLine.removeFromLeft (120);
    rateLabel.setBounds (rateArea.removeFromTop (18));
    rateCombo.setBounds (rateArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto bufferArea = firstLine.removeFromLeft (130);
    bufferLabel.setBounds (bufferArea.removeFromTop (18));
    bufferCombo.setBounds (bufferArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto engineArea = firstLine.removeFromLeft (140);
    engineLabel.setBounds (engineArea.removeFromTop (18));
    startStopButton.setBounds (engineArea.removeFromTop (30));
    firstLine.removeFromLeft (10);

    auto safetyArea = firstLine;
    outputSafetyLabel.setBounds (safetyArea.removeFromTop (18));
    emergencyButton.setBounds (safetyArea.removeFromTop (30));

    auto secondLine = setupPanel.removeFromTop (38);
    autoStartToggle.setBounds (secondLine.removeFromLeft (155));
    launchAtLoginToggle.setBounds (secondLine.removeFromLeft (145));
    resetXRunsButton.setBounds (secondLine.removeFromLeft (112).reduced (2, 5));
    metricsLabel.setBounds (secondLine);

    pluginLabel.setBounds (setupPanel.removeFromTop (24));
    auto remoteLine = setupPanel.removeFromTop (42);
    remoteControlToggle.setBounds (remoteLine.removeFromLeft (225));
    newRemoteCodeButton.setBounds (remoteLine.removeFromRight (90).reduced (2, 5));
    copyRemoteButton.setBounds (remoteLine.removeFromRight (112).reduced (2, 5));
    remoteStatusLabel.setBounds (remoteLine.reduced (8, 0));
    area.removeFromTop (10);
    alertLabel.setBounds (area.removeFromTop (34));
    area.removeFromTop (8);

    auto laneActions = area.removeFromTop (34);
    addLaneButton.setBounds (laneActions.removeFromRight (120).reduced (0, 2));
    routingHintLabel.setBounds (laneActions);
    const auto headerArea = area.removeFromTop (28);
    area.removeFromTop (4);
    laneViewport.setBounds (area);

    const auto rowHeight = 62;
    laneContainer.setSize (laneViewport.getMaximumVisibleWidth(), juce::jmax (area.getHeight(), laneRows.size() * rowHeight));
    laneHeader->setBounds (headerArea.withWidth (laneContainer.getWidth()));
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
    remoteControlToggle.setToggleState (config.remoteControlEnabled, juce::dontSendNotification);
    updateRemoteControls();
    refreshDeviceControls();
   #if DEFEEDBACK_UI_PREVIEW
    pluginLabel.setText ("ISOLATED UI PREVIEW - NO CORE AUDIO DEVICE IS OPEN", juce::dontSendNotification);
    pluginLabel.setColour (juce::Label::textColourId, green);
   #else
    pluginLabel.setText (engine.getPluginDiagnostic(), juce::dontSendNotification);
    pluginLabel.setColour (juce::Label::textColourId, engine.isPluginAvailable() ? green : amber);
   #endif
    updateRuntimeStatus();
}

void MainComponent::refreshDeviceControls()
{
    const juce::ScopedValueSetter<bool> guard (refreshingControls, true);

   #if DEFEEDBACK_UI_PREVIEW
    deviceChoices.clear();
    deviceChoices.add ({ "Dante Virtual Soundcard", "Dante Virtual Soundcard", "Dante Virtual Soundcard" });
    deviceCombo.clear (juce::dontSendNotification);
    deviceCombo.addItem ("Dante Virtual Soundcard", 1);
    deviceCombo.setSelectedId (1, juce::dontSendNotification);
    rateCombo.clear (juce::dontSendNotification);
    rateCombo.addItem ("48.0 kHz", 1);
    rateCombo.setSelectedId (1, juce::dontSendNotification);
    sampleRates.clear();
    sampleRates.add (48000.0);
    bufferCombo.clear (juce::dontSendNotification);
    bufferCombo.addItem ("512 samples", 1);
    bufferCombo.setSelectedId (1, juce::dontSendNotification);
    bufferSizes.clear();
    bufferSizes.add (512);
   #else
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
   #endif
}

void MainComponent::rebuildLaneRows()
{
    laneRows.clear();
    laneContainer.removeAllChildren();

    juce::StringArray inputNames;
    juce::StringArray outputNames;
   #if DEFEEDBACK_UI_PREVIEW
    for (int channel = 0; channel < previewChannelCount; ++channel)
    {
        inputNames.add ("Input " + juce::String (channel + 1));
        outputNames.add ("Output " + juce::String (channel + 1));
    }
   #else
    inputNames = engine.getInputChannelNames();
    outputNames = engine.getOutputChannelNames();
   #endif

    const auto deviceCapacity = juce::jmin (inputNames.size(), outputNames.size());
    routingHintLabel.setText ("SIGNAL FLOW  |  INPUT -> DE-FEEDBACK -> OUTPUT  |  "
                                  + juce::String (config.lanes.size()) + " LANES  |  "
                                  + juce::String (deviceCapacity) + " DEVICE I/O PAIRS",
                              juce::dontSendNotification);

    for (int index = 0; index < config.lanes.size(); ++index)
    {
        auto* row = laneRows.add (new LaneRow (index, config.lanes[index], inputNames, outputNames));
        row->setExclusiveRouteChoices (config.lanes);
        row->onChanged = [this] { applyLanes(); };
        row->onPluginControlCommitted = [this] { saveConfig(); };
        row->onOpenEditor = [this] (int lane) { engine.openPluginEditor (lane); };
        row->onStrengthChanged = [this] (int lane, float strength)
        {
            engine.setLaneStrength (lane, strength);
        };
        row->onPluginMuteChanged = [this] (int lane, bool muted)
        {
            engine.setLanePluginMuted (lane, muted);
        };
        row->onRemove = [this] (int lane)
        {
            if (config.lanes.size() <= 1)
            {
                showMessage ("At least one lane must remain in the rack.", true);
                return;
            }

            auto updated = config.lanes;
            updated.remove (lane);
            applyLaneConfiguration (updated, "Lane removed.");
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
        if (juce::isPositiveAndBelow (index, config.lanes.size()))
        {
            lane.id = config.lanes[index].id;
            lane.pluginStateBase64 = config.lanes[index].pluginStateBase64;
            lane.editorOpen = config.lanes[index].editorOpen;
            lane.editorWindowState = config.lanes[index].editorWindowState;
        }
        updated.add (std::move (lane));
    }

    if (! applyLaneConfiguration (updated, {}))
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safe != nullptr)
                safe->rebuildLaneRows();
        });
}

bool MainComponent::applyLaneConfiguration (const juce::Array<LaneConfig>& updated,
                                            const juce::String& successMessage)
{
    if (updated.isEmpty())
    {
        showMessage ("At least one lane must remain in the rack.", true);
        return false;
    }

    if (const auto routeError = validateExclusiveRoutes (updated); routeError.isNotEmpty())
    {
        showMessage (routeError + " Selection reverted.", true);
        return false;
    }

    config.lanes = updated;
   #if DEFEEDBACK_UI_PREVIEW
    rebuildLaneRows();
    if (successMessage.isNotEmpty())
        showMessage (successMessage, false);
    updateRuntimeStatus();
    return true;
   #else
    const auto error = engine.setLanes (config.lanes);
    if (error.isNotEmpty())
    {
        showMessage (error, true);
        return false;
    }

    rebuildLaneRows();
    if (successMessage.isNotEmpty())
        showMessage (successMessage, false);
    saveConfig();
    return true;
   #endif
}

void MainComponent::addLane()
{
   #if DEFEEDBACK_UI_PREVIEW
    const auto inputChannelCount = previewChannelCount;
    const auto outputChannelCount = previewChannelCount;
   #else
    const auto inputChannelCount = engine.getInputChannelNames().size();
    const auto outputChannelCount = engine.getOutputChannelNames().size();
   #endif

    const auto findFirstUnusedChannel = [this] (int channelCount, bool input)
    {
        for (int channel = 0; channel < channelCount; ++channel)
        {
            auto used = false;
            for (const auto& lane : config.lanes)
                used = used || (input ? lane.inputChannel : lane.outputChannel) == channel;

            if (! used)
                return channel;
        }

        return -1;
    };

    const auto inputChannel = findFirstUnusedChannel (inputChannelCount, true);
    const auto outputChannel = findFirstUnusedChannel (outputChannelCount, false);
    if (inputChannel < 0 || outputChannel < 0)
    {
        showMessage (inputChannelCount == 0 || outputChannelCount == 0
                         ? "Select a Core Audio device with both input and output channels first."
                         : "Every available input/output pair already has a lane.",
                     true);
        return;
    }

    auto id = 1;
    for (const auto& lane : config.lanes)
        id = juce::jmax (id, lane.id + 1);

    auto updated = config.lanes;
    updated.add ({ id, "Vocal " + juce::String (id), inputChannel, outputChannel, false, {}, false, {} });
    applyLaneConfiguration (updated, "Lane added on the next unused input/output pair.");
}

int MainComponent::findLaneIndexById (int id) const
{
    for (int index = 0; index < config.lanes.size(); ++index)
        if (config.lanes[index].id == id)
            return index;
    return -1;
}

void MainComponent::saveConfig()
{
   #if DEFEEDBACK_UI_PREVIEW
    return;
   #else
    const auto device = engine.getCurrentDeviceChoice();
    config.inputDeviceName = device.inputName;
    config.outputDeviceName = device.outputName;
    config.sampleRate = engine.getCurrentSampleRate();
    config.bufferSize = engine.getCurrentBufferSize();
    config.lanes = engine.captureLanesWithPluginState();

    juce::String error;
    if (! settings.save (config, error))
        showMessage (error, true);
   #endif
}

void MainComponent::storeMainWindowState (const juce::String& state)
{
    config.mainWindowState = state;
    saveConfig();
}

void MainComponent::showMessage (const juce::String& message, bool isError)
{
    if (message.isEmpty())
        return;

    alertLabel.setText (message, juce::dontSendNotification);
    alertLabel.setColour (juce::Label::backgroundColourId,
                          isError ? juce::Colour (0xff5b2528) : juce::Colour (0xff173d2e));
    alertLabel.setColour (juce::Label::textColourId, isError ? juce::Colour (0xffffc4c5) : juce::Colour (0xffaaf0cc));
    remoteActionMessage = message;
    remoteActionError = isError;
    remoteActionMessageExpiresAtMs = juce::Time::getMillisecondCounterHiRes() + 5000.0;
}

void MainComponent::startOrStop()
{
   #if DEFEEDBACK_UI_PREVIEW
    previewEngineRunning = ! previewEngineRunning;
    updateRuntimeStatus();
   #else
    if (engine.isRunning())
    {
        engine.stop();
        showMessage ("Audio engine stopped. Plugins and meters are paused; outputs are silent.", false);
    }
    else
    {
        const auto error = engine.start();
        showMessage (error.isEmpty() ? "Audio engine is running." : error, error.isNotEmpty());
    }

    updateRuntimeStatus();
    saveConfig();
   #endif
}

void MainComponent::updateRuntimeStatus()
{
    juce::Array<LaneStatus> statuses;
   #if DEFEEDBACK_UI_PREVIEW
    for (int index = 0; index < laneRows.size(); ++index)
    {
        LaneStatus status;
        status.text = index == 3 ? "INVALID ROUTE" : (index == 2 ? "BYPASSED - dry pass" : "PROCESSED");
        status.isDryFallback = index >= 2;
        status.editorAvailable = true;
        status.strengthAvailable = true;
        status.pluginMuteAvailable = true;
        status.pluginMuted = index == 1;
        status.strengthNormalized = index == 0 ? 0.53f : (index == 1 ? 0.75f : (index == 2 ? 0.72f : 1.0f));
        status.inputPeak = index == 0 ? 0.55f : 0.25f;
        status.outputPeak = status.pluginMuted || index == 3 ? 0.0f : 0.35f;
        statuses.add (status);
    }
    const auto engineRunning = previewEngineRunning;
   #else
    statuses = engine.getLaneStatuses();
    const auto engineRunning = engine.isRunning();
   #endif

    const auto masterMuted = engine.isEmergencyMuted();
   #if DEFEEDBACK_UI_PREVIEW
    if (masterMuted)
        for (auto& status : statuses)
            status.outputPeak = 0.0f;
   #endif
    auto bypassCount = 0;
    auto pluginMuteCount = 0;
    auto errorCount = 0;
    for (int index = 0; index < laneRows.size(); ++index)
    {
        if (juce::isPositiveAndBelow (index, statuses.size()))
        {
            laneRows[index]->setStatus (statuses[index], engineRunning, masterMuted);

            if (statuses[index].pluginMuted)
                ++pluginMuteCount;
            else if (statuses[index].text.containsIgnoreCase ("invalid")
                     || statuses[index].text.containsIgnoreCase ("duplicate"))
                ++errorCount;
            else if (statuses[index].isDryFallback)
                ++bypassCount;
        }
    }

    startStopButton.setButtonText (engineRunning ? "STOP ENGINE" : "START ENGINE");
    startStopButton.setColour (juce::TextButton::buttonColourId,
                               engineRunning ? juce::Colour (0xff6e4d24) : juce::Colour (0xff236b4b));

    emergencyButton.setButtonText (masterMuted
                                     ? "OUTPUTS MUTED - CLICK TO UNMUTE"
                                     : "MUTE ALL OUTPUTS");
    emergencyButton.setColour (juce::TextButton::buttonColourId,
                               masterMuted ? juce::Colour (0xffb33f45) : juce::Colour (0xff672d31));

   #if DEFEEDBACK_UI_PREVIEW
    metricsLabel.setText ("LATENCY 4.0 ms     CPU 54.9%     XRUNS 0", juce::dontSendNotification);
   #else
    metricsLabel.setText ("LATENCY " + juce::String (engine.getEstimatedRoundTripMilliseconds(), 1)
                              + " ms     CPU " + juce::String (engine.getCpuUsage() * 100.0, 1)
                              + "%     XRUNS " + juce::String (engine.getXRunCount()),
                          juce::dontSendNotification);
   #endif

    if (! engineRunning)
    {
        alertLabel.setText (masterMuted
                                ? "ENGINE STOPPED | PLUGINS AND METERS PAUSED | OUTPUT MUTE ARMED"
                                : "ENGINE STOPPED | PLUGINS AND METERS ARE NOT PROCESSING",
                            juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff27313a));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc7d2da));
    }
    else if (masterMuted)
    {
        alertLabel.setText ("ALL OUTPUTS MUTED | ENGINE AND PLUGINS ARE STILL PROCESSING", juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff5b2528));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffc4c5));
    }
    else if (errorCount > 0)
    {
        alertLabel.setText ("ROUTING ERROR ON " + juce::String (errorCount)
                                + (errorCount == 1 ? " LANE" : " LANES"),
                            juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff5b2528));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffc4c5));
    }
    else if (pluginMuteCount > 0)
    {
        alertLabel.setText ("DE-FEEDBACK PLUGIN MUTE ACTIVE ON " + juce::String (pluginMuteCount)
                                + (pluginMuteCount == 1 ? " LANE" : " LANES"),
                            juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff5b2528));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffc4c5));
    }
    else if (bypassCount > 0)
    {
        alertLabel.setText ("BYPASS / DRY FALLBACK ACTIVE ON " + juce::String (bypassCount)
                                + (bypassCount == 1 ? " LANE - " : " LANES - ")
                                + "FEEDBACK PROTECTION IS OFF",
                            juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff65421d));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffddb0));
    }
    else if (engineRunning)
    {
        alertLabel.setText ("ALL LANES PROCESSING", juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff173d2e));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaaf0cc));
    }

    if (remoteControlServer.isListening())
        publishRemoteState (statuses, engineRunning, masterMuted);
}

bool MainComponent::startRemoteControl()
{
    juce::String error;
    const auto started = remoteControlServer.startServer (
        config.remoteControlPort,
        config.remoteAccessCode,
        [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::var& command)
        {
            const auto commandCopy = command;
            juce::MessageManager::callAsync ([safe, commandCopy]
            {
                if (safe != nullptr)
                    safe->handleRemoteCommand (commandCopy);
            });
        },
        error);

    if (! started)
    {
        showMessage ("LAN remote: " + error, true);
        return false;
    }

    showMessage ("Full-control LAN remote enabled. Use only on a trusted private network.", false);
    updateRuntimeStatus();
    return true;
}

void MainComponent::stopRemoteControl()
{
    remoteControlServer.stopServer();
    updateRemoteControls();
}

void MainComponent::updateRemoteControls()
{
    const auto listening = remoteControlServer.isListening();
    remoteControlToggle.setToggleState (config.remoteControlEnabled && listening,
                                        juce::dontSendNotification);
    copyRemoteButton.setEnabled (listening);
    newRemoteCodeButton.setEnabled (config.remoteControlEnabled || config.remoteAccessCode.isNotEmpty());

    if (listening)
    {
        const auto urls = remoteControlServer.getDisplayUrls();
        remoteStatusLabel.setText ("FULL CONTROL  |  " + urls.joinIntoString ("  /  ")
                                       + "  |  ACCESS CODE " + config.remoteAccessCode,
                                   juce::dontSendNotification);
        remoteStatusLabel.setColour (juce::Label::textColourId, green);
    }
    else
    {
        remoteStatusLabel.setText ("LAN REMOTE OFF  |  No network control is listening",
                                   juce::dontSendNotification);
        remoteStatusLabel.setColour (juce::Label::textColourId, mutedText);
    }
}

void MainComponent::publishRemoteState (const juce::Array<LaneStatus>& statuses,
                                        bool engineRunning,
                                        bool masterMuted)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("version", JUCE_APPLICATION_VERSION_STRING);
    root->setProperty ("engineRunning", engineRunning);
    root->setProperty ("masterMuted", masterMuted);
    root->setProperty ("autoStart", config.autoStart);
    root->setProperty ("launchAtLogin", config.launchAtLogin);

   #if DEFEEDBACK_UI_PREVIEW
    root->setProperty ("latencyMs", 4.0);
    root->setProperty ("cpuPercent", 54.9);
    root->setProperty ("xruns", 0);
    root->setProperty ("pluginDiagnostic", "Isolated UI preview - simulated De-Feedback instances");
   #else
    root->setProperty ("latencyMs", engine.getEstimatedRoundTripMilliseconds());
    root->setProperty ("cpuPercent", engine.getCpuUsage() * 100.0);
    root->setProperty ("xruns", engine.getXRunCount());
    root->setProperty ("pluginDiagnostic", engine.getPluginDiagnostic());
   #endif

    const auto hasRecentAction = remoteActionMessage.isNotEmpty()
                              && juce::Time::getMillisecondCounterHiRes() < remoteActionMessageExpiresAtMs;
    const auto runtimeMessage = alertLabel.getText();
    const auto runtimeIsCritical = masterMuted
                                || runtimeMessage.containsIgnoreCase ("routing error")
                                || runtimeMessage.containsIgnoreCase ("plugin mute")
                                || runtimeMessage.containsIgnoreCase ("bypass");
    const auto showRecentAction = hasRecentAction && (remoteActionError || ! runtimeIsCritical);
    root->setProperty ("message", showRecentAction ? remoteActionMessage : runtimeMessage);
    root->setProperty ("messageError", showRecentAction
                                         ? remoteActionError
                                         : (masterMuted
                                            || runtimeMessage.containsIgnoreCase ("error")
                                            || runtimeMessage.containsIgnoreCase ("plugin mute")));

    juce::Array<juce::var> devices;
    for (const auto& device : deviceChoices)
        devices.add (device.displayName);
    root->setProperty ("devices", devices);

    auto selectedDevice = -1;
   #if DEFEEDBACK_UI_PREVIEW
    selectedDevice = 0;
   #else
    const auto currentDevice = engine.getCurrentDeviceChoice();
    for (int index = 0; index < deviceChoices.size(); ++index)
        if (deviceChoices[index].inputName == currentDevice.inputName
            && deviceChoices[index].outputName == currentDevice.outputName)
            selectedDevice = index;
   #endif
    root->setProperty ("selectedDevice", selectedDevice);

    juce::Array<juce::var> rates;
    for (const auto rate : sampleRates)
        rates.add (rate);
    root->setProperty ("sampleRates", rates);

    juce::Array<juce::var> buffers;
    for (const auto size : bufferSizes)
        buffers.add (size);
    root->setProperty ("bufferSizes", buffers);

   #if DEFEEDBACK_UI_PREVIEW
    root->setProperty ("selectedSampleRate", 48000.0);
    root->setProperty ("selectedBufferSize", 512);
   #else
    root->setProperty ("selectedSampleRate", engine.getCurrentSampleRate());
    root->setProperty ("selectedBufferSize", engine.getCurrentBufferSize());
   #endif

    juce::StringArray inputNames;
    juce::StringArray outputNames;
   #if DEFEEDBACK_UI_PREVIEW
    for (int channel = 0; channel < previewChannelCount; ++channel)
    {
        inputNames.add ("Input " + juce::String (channel + 1));
        outputNames.add ("Output " + juce::String (channel + 1));
    }
   #else
    inputNames = engine.getInputChannelNames();
    outputNames = engine.getOutputChannelNames();
   #endif

    juce::Array<juce::var> inputs;
    for (const auto& name : inputNames)
        inputs.add (name);
    root->setProperty ("inputChannels", inputs);

    juce::Array<juce::var> outputs;
    for (const auto& name : outputNames)
        outputs.add (name);
    root->setProperty ("outputChannels", outputs);
    root->setProperty ("capacity", juce::jmin (inputNames.size(), outputNames.size()));

    juce::Array<juce::var> lanes;
    for (int index = 0; index < config.lanes.size(); ++index)
    {
        const auto& lane = config.lanes[index];
        const auto status = juce::isPositiveAndBelow (index, statuses.size())
                          ? statuses[index]
                          : LaneStatus {};
        auto laneObject = std::make_unique<juce::DynamicObject>();
        laneObject->setProperty ("id", lane.id);
        laneObject->setProperty ("name", lane.name);
        laneObject->setProperty ("inputChannel", lane.inputChannel);
        laneObject->setProperty ("outputChannel", lane.outputChannel);
        laneObject->setProperty ("dry", lane.dry);
        laneObject->setProperty ("dryFallback", status.isDryFallback);
        laneObject->setProperty ("pluginMuted", status.pluginMuted);
        laneObject->setProperty ("pluginMuteAvailable", status.pluginMuteAvailable);
        laneObject->setProperty ("strength", status.strengthNormalized);
        laneObject->setProperty ("strengthAvailable", status.strengthAvailable);
        laneObject->setProperty ("inputPeak", status.inputPeak);
        laneObject->setProperty ("outputPeak", status.outputPeak);

        auto displayStatus = status.text;
        if (! engineRunning)
            displayStatus = "ENGINE STOPPED";
        else if (masterMuted)
            displayStatus = "OUTPUT MUTED";
        else if (status.pluginMuted)
            displayStatus = "PLUGIN MUTED";
        else if (lane.dry)
            displayStatus = "BYPASSED";
        laneObject->setProperty ("status", displayStatus);
        lanes.add (juce::var (laneObject.release()));
    }
    root->setProperty ("lanes", lanes);

    remoteControlServer.publishState (juce::var (root.release()));
}

void MainComponent::handleRemoteCommand (const juce::var& command)
{
    const auto type = command.getProperty ("type", {}).toString();

    if (type == "refreshDevices")
    {
       #if ! DEFEEDBACK_UI_PREVIEW
        engine.refreshDeviceList();
       #endif
        refreshAllControls();
        rebuildLaneRows();
        showMessage ("Core Audio device list refreshed remotely.", false);
    }
    else if (type == "selectDevice")
    {
        const auto index = static_cast<int> (command.getProperty ("index", -1));
        if (! juce::isPositiveAndBelow (index, deviceChoices.size()))
        {
            showMessage ("Remote device selection is no longer available. Refresh and try again.", true);
            return;
        }
       #if ! DEFEEDBACK_UI_PREVIEW
        const auto error = engine.selectDevice (deviceChoices[index]);
        if (error.isNotEmpty())
        {
            showMessage (error, true);
            return;
        }
       #endif
        refreshAllControls();
        rebuildLaneRows();
        saveConfig();
        showMessage ("Core Audio device changed remotely.", false);
    }
    else if (type == "setSampleRate")
    {
        const auto value = static_cast<double> (command.getProperty ("value", 0.0));
        if (! sampleRates.contains (value))
        {
            showMessage ("That sample rate is not currently available.", true);
            return;
        }
       #if ! DEFEEDBACK_UI_PREVIEW
        const auto error = engine.setSampleRate (value);
        if (error.isNotEmpty())
        {
            showMessage (error, true);
            return;
        }
       #endif
        refreshAllControls();
        saveConfig();
        showMessage ("Sample rate changed remotely.", false);
    }
    else if (type == "setBufferSize")
    {
        const auto value = static_cast<int> (command.getProperty ("value", 0));
        if (! bufferSizes.contains (value))
        {
            showMessage ("That buffer size is not currently available.", true);
            return;
        }
       #if ! DEFEEDBACK_UI_PREVIEW
        const auto error = engine.setBufferSize (value);
        if (error.isNotEmpty())
        {
            showMessage (error, true);
            return;
        }
       #endif
        refreshAllControls();
        saveConfig();
        showMessage ("Buffer size changed remotely.", false);
    }
    else if (type == "setEngineRunning")
    {
        const auto shouldRun = static_cast<bool> (command.getProperty ("running", false));
       #if DEFEEDBACK_UI_PREVIEW
        if (previewEngineRunning != shouldRun)
            startOrStop();
       #else
        if (engine.isRunning() != shouldRun)
            startOrStop();
       #endif
    }
    else if (type == "setMasterMuted")
    {
        const auto shouldMute = static_cast<bool> (command.getProperty ("muted", true));
        engine.setEmergencyMuted (shouldMute);
        updateRuntimeStatus();
    }
    else if (type == "resetXRuns")
    {
       #if ! DEFEEDBACK_UI_PREVIEW
        engine.resetXRunCount();
       #endif
        showMessage ("Session XRun counter reset remotely.", false);
        updateRuntimeStatus();
    }
    else if (type == "addLane")
    {
        addLane();
    }
    else if (type == "removeLane")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        if (! juce::isPositiveAndBelow (index, config.lanes.size()))
        {
            showMessage ("That lane no longer exists.", true);
            return;
        }
        auto updated = config.lanes;
        updated.remove (index);
        applyLaneConfiguration (updated, "Lane removed remotely.");
    }
    else if (type == "updateLane")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        if (! juce::isPositiveAndBelow (index, config.lanes.size()))
        {
            showMessage ("That lane no longer exists.", true);
            return;
        }

        const auto previous = config.lanes[index];
        auto updated = config.lanes;
        auto& lane = updated.getReference (index);
        lane.name = command.getProperty ("name", lane.name).toString().trim();
        if (lane.name.isEmpty())
            lane.name = "Vocal " + juce::String (lane.id);
        lane.inputChannel = static_cast<int> (command.getProperty ("inputChannel", lane.inputChannel));
        lane.outputChannel = static_cast<int> (command.getProperty ("outputChannel", lane.outputChannel));
        lane.dry = static_cast<bool> (command.getProperty ("dry", lane.dry));

       #if DEFEEDBACK_UI_PREVIEW
        const auto inputCount = previewChannelCount;
        const auto outputCount = previewChannelCount;
       #else
        const auto inputCount = engine.getInputChannelNames().size();
        const auto outputCount = engine.getOutputChannelNames().size();
       #endif
        if (! juce::isPositiveAndBelow (lane.inputChannel, inputCount)
            || ! juce::isPositiveAndBelow (lane.outputChannel, outputCount))
        {
            showMessage ("Remote lane route is outside the current device channel range.", true);
            return;
        }

        const auto processingChanged = lane.inputChannel != previous.inputChannel
                                    || lane.outputChannel != previous.outputChannel
                                    || lane.dry != previous.dry;
        if (processingChanged)
        {
            applyLaneConfiguration (updated, "Lane routing updated remotely.");
        }
        else
        {
            config.lanes = updated;
           #if ! DEFEEDBACK_UI_PREVIEW
            engine.setLaneName (index, lane.name);
            saveConfig();
           #endif
            rebuildLaneRows();
            showMessage ("Lane name updated remotely.", false);
        }
    }
    else if (type == "setLaneStrength")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        if (! juce::isPositiveAndBelow (index, config.lanes.size()))
            return;
        const auto value = juce::jlimit (0.0f, 1.0f,
                                         static_cast<float> (command.getProperty ("value", 1.0)));
       #if ! DEFEEDBACK_UI_PREVIEW
        engine.setLaneStrength (index, value);
        if (static_cast<bool> (command.getProperty ("commit", false)))
            saveConfig();
       #else
        juce::ignoreUnused (value);
       #endif
    }
    else if (type == "setLanePluginMuted")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        if (! juce::isPositiveAndBelow (index, config.lanes.size()))
            return;
       #if ! DEFEEDBACK_UI_PREVIEW
        engine.setLanePluginMuted (index,
                                   static_cast<bool> (command.getProperty ("muted", true)));
        saveConfig();
       #endif
    }
    else if (type == "setAutoStart")
    {
        config.autoStart = static_cast<bool> (command.getProperty ("enabled", false));
        autoStartToggle.setToggleState (config.autoStart, juce::dontSendNotification);
        saveConfig();
    }
    else if (type == "setLaunchAtLogin")
    {
        const auto enabled = static_cast<bool> (command.getProperty ("enabled", false));
        juce::String error;
       #if DEFEEDBACK_UI_PREVIEW
        juce::ignoreUnused (error);
       #else
        if (! setLaunchAtLogin (enabled, error))
        {
            showMessage ("Launch at login: " + error, true);
            return;
        }
       #endif
        config.launchAtLogin = enabled;
        launchAtLoginToggle.setToggleState (enabled, juce::dontSendNotification);
        saveConfig();
    }
    else
    {
        showMessage ("Unknown remote command was rejected.", true);
    }
}
}
