#pragma once
#include "PluginProcessor.h"

namespace ui { class View; class Lnf; }

// Guilherme Bolinha editor: top bar (views, priority, panic, master), the five views of the
// original workstation (Mixer, Preset, Edit, Seq, Setup) and an on-screen keyboard with wheels.
class BolinhaEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener
{
public:
    explicit BolinhaEditor (BolinhaProcessor&);
    ~BolinhaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void showView (int index);
    BolinhaProcessor& processor() { return proc; }
    void setStatus (const juce::String& text);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildViews();

    BolinhaProcessor& proc;
    std::unique_ptr<ui::Lnf> lnf;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::TextButton priorityButton { "PRIORITY" }, panicButton { "PANIC" };
    juce::Slider masterVolume;
    juce::Label channelLabel, statusLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> priorityAtt;
    juce::OwnedArray<ui::View> views;
    int currentView = 0;
    juce::MidiKeyboardComponent keyboard;
    juce::Slider pitchWheel, modWheel;
    float mainMeter = 0, subMeter = 0;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BolinhaEditor)
};
