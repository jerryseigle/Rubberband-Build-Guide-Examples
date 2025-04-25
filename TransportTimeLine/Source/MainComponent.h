#pragma once

#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void resized() override;
    void timerCallback() override;

private:
    // UI Components
    juce::Label transportLabel;
    juce::Label bpmLabel;
    juce::TextEditor timeSignatureBox;
    juce::Slider tempoSlider;
    juce::ToggleButton waitForBarToggle;
    juce::TextButton startTransportButton;
    juce::TextButton stopTransportButton;

    // Transport Timeline
    bool transportRunning = false;
    double originalTempo = 120.0;
    int beatsPerBar = 4;
    double currentSampleRate = 44100.0;
    double timelineSeconds = 0.0;
    double lastCallbackTime = -1.0;

    // Utility Methods
    double getAdjustedTempo() const;
    juce::String getFormattedTimeline(double seconds, double bpm, int beatsPerBarOverride = 4) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
