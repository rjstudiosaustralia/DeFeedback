#include "MainComponent.h"

#include "LoginItem.h"
#include "SettingsPanel.h"

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
    preview.remoteAccessCodeRequired = true;
    preview.remoteAccessCode = "12345678";
    preview.remoteBrowserToken = juce::String::repeatedString ("0123456789abcdef", 4);
    preview.remoteControlPort = 8766;
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
    juce::Rectangle<int> inputMeter;
    juce::Rectangle<int> strength;
    juce::Rectangle<int> pluginEnable;
    juce::Rectangle<int> editor;
    juce::Rectangle<int> pluginMute;
    juce::Rectangle<int> bypass;
    juce::Rectangle<int> outputMeter;
    juce::Rectangle<int> status;
    juce::Rectangle<int> remove;
};

LaneGeometry calculateLaneGeometry (juce::Rectangle<int> bounds)
{
    LaneGeometry layout;
    const auto compact = bounds.getWidth() < 1180;
    auto area = bounds.reduced (compact ? 6 : 8, 0);

    layout.number = area.removeFromLeft (compact ? 24 : 30);
    area.removeFromLeft (compact ? 4 : 6);
    layout.name = area.removeFromLeft (compact ? 82 : 112);
    area.removeFromLeft (compact ? 5 : 8);
    layout.inputMeter = area.removeFromLeft (compact ? 42 : 70);
    area.removeFromLeft (compact ? 6 : 10);

    layout.remove = area.removeFromRight (compact ? 28 : 32);
    area.removeFromRight (compact ? 4 : 6);
    layout.status = area.removeFromRight (compact ? 72 : 120);
    area.removeFromRight (compact ? 5 : 8);
    layout.outputMeter = area.removeFromRight (compact ? 42 : 70);
    area.removeFromRight (compact ? 6 : 10);
    layout.bypass = area.removeFromRight (compact ? 60 : 82);
    area.removeFromRight (compact ? 5 : 8);
    layout.pluginMute = area.removeFromRight (compact ? 60 : 82);
    area.removeFromRight (compact ? 5 : 8);
    layout.editor = area.removeFromRight (compact ? 36 : 48);
    area.removeFromRight (compact ? 5 : 8);
    layout.pluginEnable = area.removeFromRight (compact ? 60 : 74);
    area.removeFromRight (compact ? 6 : 10);
    layout.strength = area;

    return layout;
}

class PeakMeter final : public juce::Component
{
public:
    void setLevel (float newLevel)
    {
        const auto next = juce::jlimit (0.0f, 1.0f, newLevel);
        const auto colourBand = [] (float value) { return value > 0.9f ? 2 : value > 0.65f ? 1 : 0; };
        const auto changed = juce::roundToInt (std::sqrt (next) * static_cast<float> (getWidth()))
                          != juce::roundToInt (std::sqrt (level) * static_cast<float> (getWidth()))
                          || colourBand (next) != colourBand (level);
        level = next;
        if (changed) repaint();
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

        initialise (inputLevel, "INPUT LEVEL", juce::Justification::centred);
        initialise (strength, "STRENGTH");
        initialise (pluginEnable, "PLUGIN", juce::Justification::centred);
        initialise (editor, "UI", juce::Justification::centred);
        initialise (pluginMute, "PLUGIN MUTE", juce::Justification::centred);
        initialise (bypass, "BYPASS", juce::Justification::centred);
        initialise (outputLevel, "OUTPUT LEVEL", juce::Justification::centred);

        initialise (status, "STATE");
    }

    void setMode (PluginMode)
    {
        pluginMute.setVisible (false);
        pluginEnable.setVisible (false);
        editor.setVisible (false);
        strength.setText ("PLUGIN CONTROLS", juce::dontSendNotification);
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
        inputLevel.setBounds (layout.inputMeter);
        strength.setBounds (layout.strength);
        pluginEnable.setBounds (layout.pluginEnable);
        editor.setBounds (layout.editor);
        pluginMute.setBounds (layout.pluginMute);
        bypass.setBounds (layout.bypass);
        outputLevel.setBounds (layout.outputMeter);
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
    juce::Label inputLevel;
    juce::Label strength;
    juce::Label pluginEnable;
    juce::Label editor;
    juce::Label pluginMute;
    juce::Label bypass;
    juce::Label outputLevel;
    juce::Label status;
};

class LaneRow final : public juce::Component
{
public:
    LaneRow (int rowIndex,
             const LaneConfig& initial,
             PluginMode mode)
        : index (rowIndex), config (initial), pluginMode (mode)
    {
        numberLabel.setText (juce::String (rowIndex + 1), juce::dontSendNotification);
        numberLabel.setJustificationType (juce::Justification::centred);
        numberLabel.setColour (juce::Label::textColourId, mutedText);
        addAndMakeVisible (numberLabel);

        nameEditor.setText (config.name, false);
        nameEditor.setSelectAllWhenFocused (true);
        nameEditor.setTooltip ("Patch: input " + juce::String (config.inputChannel + 1)
            + " -> output " + juce::String (config.outputChannel + 1) + ". Edit in Settings.");
        nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff101316));
        nameEditor.setColour (juce::TextEditor::outlineColourId, border);
        nameEditor.onFocusLost = [this] { commit(); };
        nameEditor.onReturnKey = [this] { commit(); };
        addAndMakeVisible (nameEditor);
        pluginNameLabel.setText (pluginModeName (pluginMode), juce::dontSendNotification);
        pluginNameLabel.setFont (juce::FontOptions (10.0f));
        pluginNameLabel.setColour (juce::Label::textColourId, mutedText);
        pluginNameLabel.setTooltip (pluginModeName (pluginMode) + " - change in Settings");
        addAndMakeVisible (pluginNameLabel);

        pluginEnableToggle.setToggleState (config.pluginEnabled, juce::dontSendNotification);
        pluginEnableToggle.setButtonText (config.pluginEnabled ? "ON" : "OFF");
        pluginEnableToggle.setColour (juce::ToggleButton::textColourId, green);
        pluginEnableToggle.setTooltip ("Suspend this plugin instance and pass input audio dry, retaining its route and settings.");
        pluginEnableToggle.onClick = [this]
        {
            const auto enabled = pluginEnableToggle.getToggleState();
            config.pluginEnabled = enabled;
            pluginEnableToggle.setButtonText (enabled ? "ON" : "OFF");
            if (onPluginEnabledChanged != nullptr)
                onPluginEnabledChanged (index, enabled);
        };
        addAndMakeVisible (pluginEnableToggle);

        dryToggle.setToggleState (config.dry, juce::dontSendNotification);
        dryToggle.setColour (juce::ToggleButton::textColourId, amber);
        dryToggle.setTooltip ("Bypass the plugin and pass the input dry.");
        dryToggle.onClick = [this] { commit(); };
        addAndMakeVisible (dryToggle);

        strengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        strengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        strengthSlider.setRange (0.0, pluginMode == PluginMode::clearVoice ? 10.0 : pluginMode == PluginMode::voiceIso ? 120.0 : 100.0, 0.1);
        strengthSlider.setTextValueSuffix (pluginMode == PluginMode::clearVoice ? "" : "%");
        strengthSlider.setValue (100.0, juce::dontSendNotification);
        strengthSlider.setTooltip (pluginModeName (pluginMode) + " Strength");
        strengthSlider.onValueChange = [this]
        {
            if (onStrengthChanged != nullptr)
                onStrengthChanged (index, static_cast<float> (strengthSlider.getValue() / strengthSlider.getMaximum()));
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
        addChildComponent (pluginMuteToggle);
        pluginMuteToggle.setVisible (pluginMode == PluginMode::deFeedback);

        if (pluginMode == PluginMode::clearVoice)
        {
            dereverbSlider.setSliderStyle (juce::Slider::LinearHorizontal);
            dereverbSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
            dereverbSlider.setRange (0.0, 10.0, 0.1);
            dereverbSlider.setTooltip ("Dereverb: raise Strength first; Strength 0 bypasses all processing.");
            dereverbSlider.onValueChange = [this]
            {
                if (onClearVoiceControl != nullptr)
                    onClearVoiceControl (index, "dereverb", static_cast<float> (dereverbSlider.getValue() / 10.0));
                if (! dereverbSlider.isMouseButtonDown() && onPluginControlCommitted != nullptr)
                    onPluginControlCommitted();
            };
            dereverbSlider.onDragEnd = [this] { if (onPluginControlCommitted != nullptr) onPluginControlCommitted(); };
            cpuCombo.addItem ("CPU: LOW", 1);
            cpuCombo.addItem ("CPU: HIGH", 2);
            latencyCombo.addItem ("Latency: ZERO", 1);
            latencyCombo.addItem ("Latency: LOW", 2);
            cpuCombo.setTooltip ("LOW is recommended for feedback-critical monitoring. Stop the engine to change.");
            latencyCombo.setTooltip ("ZERO for monitoring. LOW adds plugin delay. Stop the engine to change.");
            cpuCombo.onChange = [this] { commitMode ("cpu", cpuCombo); };
            latencyCombo.onChange = [this] { commitMode ("latency", latencyCombo); };
            strengthLabel.setText ("STRENGTH", juce::dontSendNotification);
            dereverbLabel.setText ("DEREVERB", juce::dontSendNotification);
            for (auto* component : std::initializer_list<juce::Component*> {
                     &dereverbSlider, &cpuCombo, &latencyCombo, &strengthLabel, &dereverbLabel })
                addAndMakeVisible (component);
        }

        if (pluginMode == PluginMode::voiceIso)
        {
            strengthLabel.setText ("STRENGTH", juce::dontSendNotification);
            modelHint.setText ("FULL / LITE: OPEN UI", juce::dontSendNotification);
            modelHint.setFont (juce::FontOptions (12.0f));
            modelHint.setTooltip ("Stop the engine, then select Full or Lite in the plugin editor. Each lane saves its model.");
            addAndMakeVisible (strengthLabel);
            addAndMakeVisible (modelHint);
        }

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
        result.dry = dryToggle.getToggleState();
        result.pluginEnabled = pluginEnableToggle.getToggleState();
        return result;
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
        else if (! status.pluginEnabled)
        {
            displayStatus = "PLUGIN OFF - DRY";
            nextAccent = amber;
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

        if (rowAccent != nextAccent)
        {
            rowAccent = nextAccent;
            repaint();
        }
        statusLabel.setText (displayStatus, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, rowAccent);
        pluginEnableToggle.setToggleState (status.pluginEnabled, juce::dontSendNotification);
        pluginEnableToggle.setButtonText (status.pluginEnabled ? "ON" : "OFF");
        editorButton.setEnabled (status.editorAvailable);
        strengthSlider.setEnabled (status.strengthAvailable);
        pluginMuteToggle.setEnabled (status.pluginMuteAvailable);
        if (! strengthSlider.isMouseButtonDown())
            strengthSlider.setValue (status.strengthNormalized * strengthSlider.getMaximum(), juce::dontSendNotification);
        pluginMuteToggle.setToggleState (status.pluginMuted, juce::dontSendNotification);
        dereverbSlider.setEnabled (status.dereverbAvailable);
        if (! dereverbSlider.isMouseButtonDown())
            dereverbSlider.setValue (status.dereverbNormalized * 10.0, juce::dontSendNotification);
        cpuCombo.setEnabled (status.cpuModeAvailable && ! engineRunning);
        latencyCombo.setEnabled (status.latencyModeAvailable && ! engineRunning);
        cpuCombo.setSelectedId (status.highCpu ? 2 : 1, juce::dontSendNotification);
        latencyCombo.setSelectedId (status.lowLatency ? 2 : 1, juce::dontSendNotification);
        statusLabel.setTooltip (status.text + " | Plugin delay: " + juce::String (status.pluginLatencySamples) + " samples");
        inputMeter.setLevel (status.inputPeak);
        outputMeter.setLevel (status.outputPeak);
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
        auto nameArea = layout.name.withSizeKeepingCentre (layout.name.getWidth(), 52);
        nameEditor.setBounds (nameArea.removeFromTop (32));
        pluginNameLabel.setBounds (nameArea);
        inputMeter.setBounds (layout.inputMeter.withSizeKeepingCentre (layout.inputMeter.getWidth(), 10));
        strengthSlider.setBounds (layout.strength.withSizeKeepingCentre (layout.strength.getWidth(), 30));
        pluginEnableToggle.setBounds (layout.pluginEnable.withSizeKeepingCentre (layout.pluginEnable.getWidth(), 30));
        editorButton.setBounds (layout.editor.withSizeKeepingCentre (layout.editor.getWidth(), 34));
        pluginMuteToggle.setBounds (layout.pluginMute.withSizeKeepingCentre (layout.pluginMute.getWidth(), 30));
        dryToggle.setBounds (layout.bypass.withSizeKeepingCentre (layout.bypass.getWidth(), 30));
        outputMeter.setBounds (layout.outputMeter.withSizeKeepingCentre (layout.outputMeter.getWidth(), 10));
        statusLabel.setBounds (layout.status);
        removeButton.setBounds (layout.remove.withSizeKeepingCentre (layout.remove.getWidth(), 34));
        if (pluginMode == PluginMode::clearVoice)
        {
            auto controls = layout.strength.withRight (layout.pluginMute.getRight()).reduced (0, 6);
            auto top = controls.removeFromTop (30);
            strengthLabel.setBounds (top.removeFromLeft (82));
            strengthSlider.setBounds (top);
            auto middle = controls.removeFromTop (30);
            dereverbLabel.setBounds (middle.removeFromLeft (82));
            dereverbSlider.setBounds (middle);
            auto bottom = controls.removeFromTop (28);
            cpuCombo.setBounds (bottom.removeFromLeft (100));
            bottom.removeFromLeft (6);
            latencyCombo.setBounds (bottom.removeFromLeft (130));
            bottom.removeFromLeft (8);
            pluginEnableToggle.setBounds (bottom.removeFromLeft (60));
            editorButton.setBounds (bottom.removeFromLeft (36));
        }
        else if (pluginMode == PluginMode::voiceIso)
        {
            auto controls = layout.strength.withRight (layout.pluginMute.getRight()).reduced (0, 8);
            auto top = controls.removeFromTop (30);
            strengthLabel.setBounds (top.removeFromLeft (82));
            strengthSlider.setBounds (top);
            auto bottom = controls.removeFromTop (28);
            modelHint.setBounds (bottom.removeFromLeft (240));
            pluginEnableToggle.setBounds (bottom.removeFromLeft (60));
            editorButton.setBounds (bottom.removeFromLeft (36));
        }
    }

    std::function<void()> onChanged;
    std::function<void()> onPluginControlCommitted;
    std::function<void(int)> onOpenEditor;
    std::function<void(int)> onRemove;
    std::function<void(int, float)> onStrengthChanged;
    std::function<void(int, bool)> onPluginEnabledChanged;
    std::function<void(int, bool)> onPluginMuteChanged;
    std::function<void(int, const juce::String&, float)> onClearVoiceControl;

private:
    void commitMode (const juce::String& control, const juce::ComboBox& combo)
    {
        if (onClearVoiceControl != nullptr)
            onClearVoiceControl (index, control, combo.getSelectedId() == 2 ? 1.0f : 0.0f);
        if (onPluginControlCommitted != nullptr) onPluginControlCommitted();
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
    PluginMode pluginMode;
    juce::Label pluginNameLabel;
    juce::Slider dereverbSlider;
    juce::ComboBox cpuCombo, latencyCombo;
    juce::Label strengthLabel, dereverbLabel, modelHint;
    juce::ToggleButton dryToggle { "BYPASS" };
    juce::Label statusLabel;
    PeakMeter inputMeter;
    PeakMeter outputMeter;
    juce::Slider strengthSlider;
    juce::ToggleButton pluginEnableToggle;
    juce::ToggleButton pluginMuteToggle { "MUTE" };
    juce::TextButton editorButton { "UI" };
    juce::TextButton removeButton { "X" };
    juce::Colour rowAccent { mutedText };
};

MainComponent::MainComponent (bool shouldUseSafeLaunch)
    : config (loadInitialConfig (settings)), safeLaunch (shouldUseSafeLaunch)
{
    if (config.remoteAccessCode.isEmpty())
        config.remoteAccessCode = RemoteControlServer::generateAccessCode();
    if (config.remoteBrowserToken.length() < 32)
        config.remoteBrowserToken = RemoteControlServer::generateBrowserToken();

    setOpaque (true);

    titleLabel.setText ("DEFEEDBACK LIVE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (27.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (titleLabel);

   #if DEFEEDBACK_MULTICORE_PREVIEW
    const juce::String hostDescription = "Multicore preview";
   #else
    const juce::String hostDescription = "Apple Silicon live Audio Unit host";
   #endif
    subtitleLabel.setText (hostDescription + (sleepInhibitor.isActive()
                               ? "  |  SCREEN & SYSTEM AWAKE"
                               : "  |  SLEEP BLOCK FAILED"),
                           juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, sleepInhibitor.isActive() ? mutedText : red);
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
            "Enable it only on a trusted private network; never expose TCP port 8765 to the internet or shared Wi-Fi.\n\n"
            "While this app is open, it asks macOS to prevent idle system sleep, display sleep and App Nap. "
            "Closing the lid or explicitly choosing Sleep can still interrupt audio.");
    };
    addAndMakeVisible (aboutButton);
    settingsButton.onClick = [this] { showSettingsDialog(); };
    settingsButton.setTooltip ("Choose each lane's plugin and patch inputs / outputs.");
    addAndMakeVisible (settingsButton);

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
    remoteControlToggle.setTooltip ("Expose full control on the local network using the configured access protection.");
    remoteControlToggle.onClick = [this]
    {
        const auto shouldEnable = remoteControlToggle.getToggleState();
        if (shouldEnable)
        {
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

    copyRemoteButton.setTooltip ("Copy the LAN address and current access details to the clipboard.");
    copyRemoteButton.onClick = [this]
    {
        const auto urls = remoteControlServer.getDisplayUrls();
        if (urls.isEmpty())
        {
            showMessage ("Enable the LAN remote before copying its connection details.", true);
            return;
        }

        const auto access = config.remoteAccessCodeRequired
                              ? "Access code: " + config.remoteAccessCode
                              : "Access code: OFF - any device on this LAN can control the app";
        juce::SystemClipboard::copyTextToClipboard (urls[0] + "\n" + access);
        showMessage ("Remote address and access details copied.", false);
    };
    addAndMakeVisible (copyRemoteButton);

    remoteAccessButton.setTooltip ("Choose a custom or random code, or explicitly disable access-code protection.");
    remoteAccessButton.onClick = [this] { showRemoteAccessDialog(); };
    addAndMakeVisible (remoteAccessButton);

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

    if (! sleepInhibitor.isActive())
        showMessage ("Could not prevent macOS idle sleep: " + sleepInhibitor.getError(), true);

    if (config.remoteControlEnabled)
    {
        config.remoteControlEnabled = startRemoteControl();
    }
    updateRemoteControls();
    saveConfig();

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

    const auto setupPanelHeight = getWidth() < 1180 ? 260 : 196;
    auto topPanel = juce::Rectangle<int> (20, 76, getWidth() - 40, setupPanelHeight).toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (topPanel, 8.0f);
    g.setColour (border);
    g.drawRoundedRectangle (topPanel, 8.0f, 1.0f);
}

void MainComponent::resized()
{
    const auto compact = getWidth() < 1180;
    const auto setupPanelHeight = compact ? 260 : 196;
    auto area = getLocalBounds().reduced (20);
    auto heading = area.removeFromTop (46);
    aboutButton.setBounds (heading.removeFromRight (compact ? 120 : 150).reduced (4, 7));
    settingsButton.setBounds (heading.removeFromRight (100).reduced (4, 7));
    titleLabel.setBounds (heading.removeFromLeft (compact ? 250 : 360));
    subtitleLabel.setBounds (heading);
    area.removeFromTop (10);

    auto setupPanel = area.removeFromTop (setupPanelHeight).reduced (16, 12);
    auto firstLine = setupPanel.removeFromTop (54);

    auto deviceArea = firstLine.removeFromLeft (compact ? 250 : 300);
    deviceLabel.setBounds (deviceArea.removeFromTop (18));
    deviceCombo.setBounds (deviceArea.removeFromTop (30));
    firstLine.removeFromLeft (8);

    auto refreshArea = firstLine.removeFromLeft (90);
    refreshArea.removeFromTop (18);
    refreshDevicesButton.setBounds (refreshArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto rateArea = firstLine.removeFromLeft (compact ? 110 : 120);
    rateLabel.setBounds (rateArea.removeFromTop (18));
    rateCombo.setBounds (rateArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto bufferArea = firstLine.removeFromLeft (compact ? 120 : 130);
    bufferLabel.setBounds (bufferArea.removeFromTop (18));
    bufferCombo.setBounds (bufferArea.removeFromTop (30));
    firstLine.removeFromLeft (12);

    auto transportLine = compact ? setupPanel.removeFromTop (54) : firstLine;
    auto engineArea = transportLine.removeFromLeft (compact ? 160 : 140);
    engineLabel.setBounds (engineArea.removeFromTop (18));
    startStopButton.setBounds (engineArea.removeFromTop (30));
    transportLine.removeFromLeft (10);

    auto safetyArea = compact ? transportLine.removeFromLeft (280) : transportLine;
    outputSafetyLabel.setBounds (safetyArea.removeFromTop (18));
    emergencyButton.setBounds (safetyArea.removeFromTop (30));
    if (compact)
        metricsLabel.setBounds (transportLine);

    auto secondLine = setupPanel.removeFromTop (38);
    autoStartToggle.setBounds (secondLine.removeFromLeft (155));
    launchAtLoginToggle.setBounds (secondLine.removeFromLeft (145));
    resetXRunsButton.setBounds (secondLine.removeFromLeft (112).reduced (2, 5));
    if (! compact)
        metricsLabel.setBounds (secondLine);

    pluginLabel.setBounds (setupPanel.removeFromTop (24));
    auto remoteLine = setupPanel.removeFromTop (42);
    remoteControlToggle.setBounds (remoteLine.removeFromLeft (225));
    remoteAccessButton.setBounds (remoteLine.removeFromRight (90).reduced (2, 5));
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

    auto totalHeight = 0;
    for (int index = 0; index < laneRows.size(); ++index)
    {
        const auto mode = config.lanes[index].pluginMode;
        const auto rowHeight = mode == PluginMode::clearVoice ? 106 : mode == PluginMode::voiceIso ? 82 : 62;
        laneRows[index]->setBounds (0, totalHeight, laneViewport.getMaximumVisibleWidth(), rowHeight - 2);
        totalHeight += rowHeight;
    }
    laneContainer.setSize (laneViewport.getMaximumVisibleWidth(), juce::jmax (area.getHeight(), totalHeight));
    laneHeader->setBounds (headerArea.withWidth (laneContainer.getWidth()));

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
    laneHeader->setMode (config.pluginMode);
    routingHintLabel.setText (config.rackModeName().toUpperCase() + "  |  PATCHING IN SETTINGS  |  "
                                  + juce::String (config.lanes.size()) + " LANES  |  "
                                  + juce::String (deviceCapacity) + " DEVICE I/O PAIRS",
                              juce::dontSendNotification);

    for (int index = 0; index < config.lanes.size(); ++index)
    {
        auto* row = laneRows.add (new LaneRow (index, config.lanes[index], config.lanes[index].pluginMode));
        row->onChanged = [this] { applyLanes(); };
        row->onPluginControlCommitted = [this] { saveConfig(); };
        row->onOpenEditor = [this] (int lane) { engine.openPluginEditor (lane); };
        row->onStrengthChanged = [this] (int lane, float strength)
        {
            engine.setLaneStrength (lane, strength);
        };
        row->onClearVoiceControl = [this] (int lane, const juce::String& control, float value)
        {
            const auto error = engine.setLaneClearVoiceControl (lane, control, value);
            if (error.isNotEmpty()) showMessage (error, true);
        };
        row->onPluginEnabledChanged = [this] (int lane, bool enabled)
        {
            if (! juce::isPositiveAndBelow (lane, config.lanes.size()))
                return;

            config.lanes.getReference (lane).pluginEnabled = enabled;
           #if ! DEFEEDBACK_UI_PREVIEW
            engine.setLanePluginEnabled (lane, enabled);
           #endif
            showMessage (enabled
                             ? "Lane plugin enabled."
                             : "Lane plugin suspended; this lane passes audio dry.",
                         false);
            updateRuntimeStatus();
            saveConfig();
        };
        row->onPluginMuteChanged = [this] (int lane, bool muted)
        {
           #if DEFEEDBACK_UI_PREVIEW
            const auto laneId = config.lanes[lane].id;
            if (muted)
                previewMutedLaneIds.addIfNotAlreadyThere (laneId);
            else
                previewMutedLaneIds.removeAllInstancesOf (laneId);
            updateRuntimeStatus();
           #else
            engine.setLanePluginMuted (lane, muted);
           #endif
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
    updateRuntimeStatus();
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
    updated.getReference (updated.size() - 1).selectPlugin (config.pluginMode);
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
        status.pluginEnabled = config.lanes[index].pluginEnabled;
        status.text = ! status.pluginEnabled
                        ? "PLUGIN OFF - dry pass"
                        : (index == 3 ? "INVALID ROUTE" : (config.lanes[index].dry ? "BYPASSED - dry pass" : "PROCESSED"));
        status.isDryFallback = ! status.pluginEnabled || config.lanes[index].dry || index == 3;
        status.editorAvailable = status.pluginEnabled;
        status.strengthAvailable = status.pluginEnabled;
        status.pluginMuteAvailable = status.pluginEnabled && config.lanes[index].pluginMode == PluginMode::deFeedback;
        status.pluginMuted = status.pluginEnabled && ! config.lanes[index].dry && config.lanes[index].pluginMode == PluginMode::deFeedback && previewMutedLaneIds.contains (config.lanes[index].id);
        status.dereverbAvailable = status.cpuModeAvailable = status.latencyModeAvailable
            = status.pluginEnabled && config.lanes[index].pluginMode == PluginMode::clearVoice;
        status.dereverbNormalized = 0.6f;
        status.strengthNormalized = index == 0 ? 0.53f : (index == 1 ? 0.75f : (index == 2 ? 0.72f : 1.0f));
        status.inputPeak = index == 0 ? 0.55f : 0.25f;
        status.outputPeak = status.pluginMuted || index == 3 ? 0.0f : ! status.pluginEnabled ? status.inputPeak : 0.35f;
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
    auto inactiveCount = 0;
    auto errorCount = 0;
    for (int index = 0; index < laneRows.size(); ++index)
    {
        if (juce::isPositiveAndBelow (index, statuses.size()))
        {
            laneRows[index]->setStatus (statuses[index], engineRunning, masterMuted);

            if (! statuses[index].pluginEnabled)
                ++inactiveCount;
            else if (statuses[index].pluginMuted)
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
    metricsLabel.setText ("LATENCY 4.0 ms   AUDIO 54.9%   THREADS 4\nPEAK 0.900/1.333 ms   LATE 0   DEVICE N/A", juce::dontSendNotification);
   #else
    const auto deadlineMs = engine.getCurrentSampleRate() > 0.0
        ? 1000.0 * engine.getCurrentBufferSize() / engine.getCurrentSampleRate() : 0.0;
    const auto deviceErrors = engine.getDeviceXRunCount();
    metricsLabel.setText ("LATENCY " + juce::String (engine.getEstimatedRoundTripMilliseconds(), 1)
                              + " ms   AUDIO " + juce::String (engine.getCpuUsage() * 100.0, 1)
                              + "%   THREADS " + juce::String (engine.getProcessingThreadCount())
                              + "\nPEAK " + juce::String (engine.getPeakCallbackMilliseconds(), 3)
                              + "/" + juce::String (deadlineMs, 3) + " ms   LATE "
                              + juce::String (engine.getCallbackOverrunCount()) + "   DEVICE "
                              + (deviceErrors < 0 ? juce::String ("N/A") : juce::String (deviceErrors)),
                          juce::dontSendNotification);
    metricsLabel.setTooltip ("AUDIO: average callback load. PEAK: longest callback / block deadline. LATE: callbacks exceeding that deadline. DEVICE: driver-reported XRuns (N/A if unavailable). Peak and counts are since RESET XRUNS; driver and callback counts may describe the same incident.");
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
    else if (inactiveCount > 0)
    {
        alertLabel.setText (juce::String (inactiveCount)
                                + (inactiveCount == 1 ? " PLUGIN OFF" : " PLUGINS OFF")
                                + " | DRY AUDIO PASSING | PLUGIN DSP SUSPENDED",
                            juce::dontSendNotification);
        alertLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff27313a));
        alertLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc7d2da));
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
        config.remoteAccessCodeRequired,
        config.remoteBrowserToken,
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

void MainComponent::showRemoteAccessDialog()
{
    auto* dialog = new juce::AlertWindow (
        "LAN Remote Access",
        "The remote can control live audio. Use access-code protection on any shared network.\n\n"
        "Turning protection off lets every device that can reach this Mac control the engine, routing, mutes, and plugins.",
        juce::MessageBoxIconType::WarningIcon);

    juce::StringArray protectionChoices;
    protectionChoices.add ("Require an access code");
    protectionChoices.add ("NO CODE - any LAN device has full control");
    dialog->addComboBox ("protection", protectionChoices, "Protection");
    if (auto* protection = dialog->getComboBoxComponent ("protection"))
        protection->setSelectedId (config.remoteAccessCodeRequired ? 1 : 2, juce::dontSendNotification);

    dialog->addTextEditor ("code", config.remoteAccessCode, "Access code (4-64 characters)");
    if (auto* editor = dialog->getTextEditor ("code"))
        editor->setInputRestrictions (64);

    dialog->addButton ("APPLY", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("USE RANDOM CODE", 2);
    dialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<MainComponent> (this), dialog] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owner (dialog);
                if (safe == nullptr || result == 0)
                    return;

                const auto requireCode = result == 2
                                       || dialog->getComboBoxComponent ("protection")->getSelectedId() == 1;
                const auto nextCode = result == 2
                                    ? RemoteControlServer::generateAccessCode()
                                    : dialog->getTextEditorContents ("code");

                juce::String validationError;
                if (requireCode && ! RemoteControlServer::validateAccessCode (nextCode, validationError))
                {
                    safe->showMessage (validationError, true);
                    juce::MessageManager::callAsync ([safe]
                    {
                        if (safe != nullptr)
                            safe->showRemoteAccessDialog();
                    });
                    return;
                }

                const auto securityChanged = safe->config.remoteAccessCodeRequired != requireCode
                                          || safe->config.remoteAccessCode != nextCode;
                safe->config.remoteAccessCodeRequired = requireCode;
                safe->config.remoteAccessCode = nextCode;
                if (securityChanged)
                    safe->config.remoteBrowserToken = RemoteControlServer::generateBrowserToken();

                auto restarted = true;
                if (safe->config.remoteControlEnabled)
                {
                    restarted = safe->startRemoteControl();
                    safe->config.remoteControlEnabled = restarted;
                }

                safe->updateRemoteControls();
                safe->saveConfig();
                if (restarted)
                {
                    safe->showMessage (
                        requireCode
                            ? "LAN access code saved. Previously remembered browsers must authenticate again."
                            : "LAN access code disabled. Any device on this network now has full control.",
                        ! requireCode);
                }
            }),
        false);
}

void MainComponent::showSettingsDialog()
{
   #if ! DEFEEDBACK_UI_PREVIEW
    config.lanes = engine.captureLanesWithPluginState();
   #endif
    auto inputs = engine.getInputChannelNames();
    auto outputs = engine.getOutputChannelNames();
   #if DEFEEDBACK_UI_PREVIEW
    for (int i = 0; i < previewChannelCount; ++i)
    {
        inputs.add ("Input " + juce::String (i + 1));
        outputs.add ("Output " + juce::String (i + 1));
    }
   #endif
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Settings - Plugins & Patching";
    options.dialogBackgroundColour = panel;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.escapeKeyTriggersCloseButton = true;
    options.componentToCentreAround = this;
    options.content.setOwned (new SettingsPanel (config, inputs, outputs,
        [safe = juce::Component::SafePointer<MainComponent> (this)] (const AppConfig& next)
        {
            return safe != nullptr ? safe->applySettings (next) : juce::String ("Host closed.");
        }));
    options.launchAsync();
}

juce::String MainComponent::applySettings (const AppConfig& next)
{
    if (next.lanes.isEmpty()) return "At least one lane must remain in the rack.";
    if (const auto error = validateExclusiveRoutes (next.lanes); error.isNotEmpty()) return error;
    bool modeChanged = next.pluginMode != config.pluginMode
                    || next.parallelProcessing != config.parallelProcessing
                    || next.maxProcessingThreads != config.maxProcessingThreads;
    bool patchChanged = next.lanes.size() != config.lanes.size();
    for (int i = 0; i < juce::jmin (next.lanes.size(), config.lanes.size()); ++i)
    {
        modeChanged = modeChanged || next.lanes[i].pluginMode != config.lanes[i].pluginMode;
        patchChanged = patchChanged || next.lanes[i].id != config.lanes[i].id
            || next.lanes[i].inputChannel != config.lanes[i].inputChannel
            || next.lanes[i].outputChannel != config.lanes[i].outputChannel;
    }
    if (! modeChanged && ! patchChanged) return {};

    engine.setEmergencyMuted (true);
   #if DEFEEDBACK_UI_PREVIEW
    previewEngineRunning = false;
    config.pluginMode = next.pluginMode;
    config.parallelProcessing = next.parallelProcessing;
    config.maxProcessingThreads = next.maxProcessingThreads;
    config.lanes = next.lanes;
   #else
    engine.stop();
    engine.setParallelProcessing (next.parallelProcessing, next.maxProcessingThreads);
    const auto error = engine.setLanes (next.lanes);
    if (error.isEmpty())
    {
        config.pluginMode = next.pluginMode;
        config.parallelProcessing = next.parallelProcessing;
        config.maxProcessingThreads = next.maxProcessingThreads;
    }
    else engine.setParallelProcessing (config.parallelProcessing, config.maxProcessingThreads);
    config.lanes = engine.captureLanesWithPluginState();
   #endif
    refreshAllControls();
    rebuildLaneRows();
    updateRuntimeStatus();
    saveConfig();
   #if ! DEFEEDBACK_UI_PREVIEW
    if (error.isNotEmpty()) return error;
   #endif
    showMessage (config.rackModeName()
        + " selected. Engine stopped; outputs muted. Review settings, then start the engine.", false);
    return {};
}

void MainComponent::updateRemoteControls()
{
    const auto listening = remoteControlServer.isListening();
    remoteControlToggle.setToggleState (config.remoteControlEnabled && listening,
                                        juce::dontSendNotification);
    copyRemoteButton.setEnabled (listening);
    remoteAccessButton.setEnabled (true);

    if (listening)
    {
        const auto urls = remoteControlServer.getDisplayUrls();
        if (config.remoteAccessCodeRequired)
        {
            remoteStatusLabel.setText ("FULL CONTROL  |  " + urls.joinIntoString ("  /  ")
                                           + "  |  ACCESS CODE " + config.remoteAccessCode,
                                       juce::dontSendNotification);
            remoteStatusLabel.setColour (juce::Label::textColourId, green);
        }
        else
        {
            remoteStatusLabel.setText ("FULL CONTROL  |  " + urls.joinIntoString ("  /  ")
                                           + "  |  NO ACCESS CODE - ANY LAN DEVICE CAN CONTROL AUDIO",
                                       juce::dontSendNotification);
            remoteStatusLabel.setColour (juce::Label::textColourId, red);
        }
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
    root->setProperty ("parallelProcessing", config.parallelProcessing);
    root->setProperty ("maxProcessingThreads", config.maxProcessingThreads);
    root->setProperty ("callbackPeakMs", engine.getPeakCallbackMilliseconds());
    root->setProperty ("callbackOverruns", engine.getCallbackOverrunCount());
    root->setProperty ("measuredBlocks", engine.getMeasuredBlockCount());
    root->setProperty ("deviceXruns", engine.getDeviceXRunCount());
    root->setProperty ("processingThreads", engine.getProcessingThreadCount());
    root->setProperty ("audioWorkgroup", engine.hasDeviceWorkgroup());
    root->setProperty ("version", JUCE_APPLICATION_VERSION_STRING);
    root->setProperty ("pluginMode", config.rackModeId());
    root->setProperty ("pluginName", config.rackModeName());
    root->setProperty ("engineRunning", engineRunning);
    root->setProperty ("masterMuted", masterMuted);
    root->setProperty ("autoStart", config.autoStart);
    root->setProperty ("launchAtLogin", config.launchAtLogin);
    root->setProperty ("sleepPrevented", sleepInhibitor.isActive());
    root->setProperty ("accessCodeRequired", config.remoteAccessCodeRequired);

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
        laneObject->setProperty ("pluginMode", pluginModeId (lane.pluginMode));
        laneObject->setProperty ("pluginName", pluginModeName (lane.pluginMode));
        laneObject->setProperty ("id", lane.id);
        laneObject->setProperty ("name", lane.name);
        laneObject->setProperty ("inputChannel", lane.inputChannel);
        laneObject->setProperty ("outputChannel", lane.outputChannel);
        laneObject->setProperty ("dry", lane.dry);
        laneObject->setProperty ("pluginEnabled", status.pluginEnabled);
        laneObject->setProperty ("dryFallback", status.isDryFallback);
        laneObject->setProperty ("pluginMuted", status.pluginMuted);
        laneObject->setProperty ("pluginMuteAvailable", status.pluginMuteAvailable);
        laneObject->setProperty ("strength", status.strengthNormalized);
        laneObject->setProperty ("strengthAvailable", status.strengthAvailable);
        laneObject->setProperty ("dereverb", status.dereverbNormalized);
        laneObject->setProperty ("dereverbAvailable", status.dereverbAvailable);
        laneObject->setProperty ("highCpu", status.highCpu);
        laneObject->setProperty ("lowLatency", status.lowLatency);
        laneObject->setProperty ("cpuModeAvailable", status.cpuModeAvailable);
        laneObject->setProperty ("latencyModeAvailable", status.latencyModeAvailable);
        laneObject->setProperty ("pluginLatencySamples", status.pluginLatencySamples);
        laneObject->setProperty ("inputPeak", status.inputPeak);
        laneObject->setProperty ("outputPeak", status.outputPeak);

        auto displayStatus = status.text;
        if (! engineRunning)
            displayStatus = "ENGINE STOPPED";
        else if (masterMuted)
            displayStatus = "OUTPUT MUTED";
        else if (! status.pluginEnabled)
            displayStatus = "PLUGIN OFF - DRY";
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

    const auto targetIndex = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
    if (command.hasProperty ("pluginMode") && type.contains ("Lane")
        && juce::isPositiveAndBelow (targetIndex, config.lanes.size())
        && command.getProperty ("pluginMode", {}).toString() != pluginModeId (config.lanes[targetIndex].pluginMode))
    {
        showMessage ("Plugin changed on this lane. Stale remote command rejected; refresh the controls.", true);
        return;
    }

    if (type == "setParallelProcessing")
    {
        auto next = config;
        next.parallelProcessing = static_cast<bool> (command.getProperty ("enabled", false));
        const auto limit = command.getProperty ("maxThreads", config.maxProcessingThreads);
        if ((! limit.isInt() && ! limit.isInt64()) || static_cast<juce::int64> (limit) < 0 || static_cast<juce::int64> (limit) > 64)
        { showMessage ("Invalid audio thread limit.", true); return; }
        next.maxProcessingThreads = static_cast<int> (limit);
        const auto error = applySettings (next);
        if (error.isNotEmpty()) showMessage (error, true);
    }
    else if (type == "setPluginMode" || type == "setLanePluginMode")
    {
        const auto mode = command.getProperty ("value", {}).toString();
        if (mode != "defeedback" && mode != "clearvoice" && mode != "voiceiso")
        {
            showMessage ("Unknown plugin mode was rejected.", true);
            return;
        }
        auto next = config;
        const auto selected = mode == "voiceiso" ? PluginMode::voiceIso
                            : mode == "clearvoice" ? PluginMode::clearVoice : PluginMode::deFeedback;
        if (type == "setLanePluginMode")
        {
            if (! juce::isPositiveAndBelow (targetIndex, next.lanes.size()))
            {
                showMessage ("That lane no longer exists.", true);
                return;
            }
            next.lanes.getReference (targetIndex).selectPlugin (selected);
        }
        else next.switchPluginMode (selected);
        const auto error = applySettings (next);
        if (error.isNotEmpty()) showMessage (error, true);
    }
    else if (type == "setLaneClearVoiceControl")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        const auto error = engine.setLaneClearVoiceControl (index,
            command.getProperty ("control", {}).toString(),
            static_cast<float> (command.getProperty ("value", -1.0)));
        if (error.isNotEmpty()) showMessage (error, true);
        else if (static_cast<bool> (command.getProperty ("commit", true))) saveConfig();
    }
    else if (type == "refreshDevices")
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
        if (lane.inputChannel != previous.inputChannel || lane.outputChannel != previous.outputChannel)
        {
            auto next = config;
            next.lanes = updated;
            const auto error = applySettings (next);
            if (error.isNotEmpty()) showMessage (error, true);
        }
        else if (processingChanged)
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
    else if (type == "setLanePluginEnabled")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        if (! juce::isPositiveAndBelow (index, config.lanes.size()))
            return;

        const auto enabled = static_cast<bool> (command.getProperty ("enabled", true));
        config.lanes.getReference (index).pluginEnabled = enabled;
       #if ! DEFEEDBACK_UI_PREVIEW
        engine.setLanePluginEnabled (index, enabled);
       #endif
        showMessage (enabled
                         ? "Lane plugin enabled remotely."
                         : "Lane plugin suspended remotely; this lane passes audio dry.",
                     false);
        updateRuntimeStatus();
        saveConfig();
    }
    else if (type == "setLanePluginMuted")
    {
        const auto index = findLaneIndexById (static_cast<int> (command.getProperty ("id", -1)));
        if (! juce::isPositiveAndBelow (index, config.lanes.size()))
            return;
        const auto shouldMute = static_cast<bool> (command.getProperty ("muted", true));
       #if DEFEEDBACK_UI_PREVIEW
        const auto laneId = config.lanes[index].id;
        if (shouldMute)
            previewMutedLaneIds.addIfNotAlreadyThere (laneId);
        else
            previewMutedLaneIds.removeAllInstancesOf (laneId);
        updateRuntimeStatus();
       #else
        engine.setLanePluginMuted (index,
                                   shouldMute);
        updateRuntimeStatus();
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
