/*
  ==============================================================================

    MainComponent.h
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TimePitchProcessor.h"

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
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Pausing,
        Paused,
        Stopping
    };

    void changeState(TransportState newState);
    void playButtonClicked();
    void stopButtonClicked();

    juce::TextButton playButton, stopButton;
    juce::Label positionLabel;
    juce::Slider pitchSlider, tempoSlider;
    juce::ToggleButton formantCheckbox;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    TimePitchProcessor timePitchProcessor;
    TransportState state = Stopped;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

