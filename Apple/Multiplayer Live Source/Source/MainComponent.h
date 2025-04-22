#pragma once

#include <JuceHeader.h>
#include "TimePitchProcessor.h"

struct Track
{
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    juce::Slider volumeSlider;
    juce::ToggleButton muteButton;

    float currentVolume = 1.0f;
    bool isMuted = false;
    bool isPercussive = false;
};

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

    // UI
    juce::TextButton playButton, stopButton;
    juce::Label positionLabel;
    juce::Slider pitchSlider, tempoSlider;
    juce::ToggleButton formantCheckbox;

    // Audio
    juce::AudioFormatManager formatManager;
    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<juce::File> trackFiles;

    // Real-time processors
    TimePitchProcessor musicalProcessor;
    TimePitchProcessor drumProcessor;

    // Transport state
    TransportState state = Stopped;

    // Helpers for mixing
    void mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
