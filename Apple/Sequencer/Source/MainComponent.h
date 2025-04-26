#pragma once

#include <JuceHeader.h>
#include "TimePitchProcessor.h"

//==============================================================================
// Track
//==============================================================================
struct Track
{
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    juce::Slider volumeSlider;
    juce::ToggleButton muteButton;

    juce::Label rmsLabel;
    juce::Label peakLabel;

    juce::TextButton playPauseButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };
    juce::ToggleButton loopToggle{ "Loop" };

    float currentVolume = 1.0f;
    bool isMuted = false;
    bool isPercussive = false;
    bool wasLooping = false;
    double lastPosition = 0.0;

    juce::AudioBuffer<float> lastBuffer;

    float getRMSLevel() const
    {
        if (lastBuffer.getNumSamples() == 0)
            return 0.0f;
        return lastBuffer.getRMSLevel(0, 0, lastBuffer.getNumSamples());
    }

    float getPeakLevel() const
    {
        if (lastBuffer.getNumSamples() == 0)
            return 0.0f;
        return lastBuffer.getMagnitude(0, 0, lastBuffer.getNumSamples());
    }
};

//==============================================================================
// MainComponent
//==============================================================================
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
    juce::TextButton playButton, stopButton;
    juce::Label positionLabel;
    juce::Slider pitchSlider, tempoSlider;
    juce::ToggleButton formantCheckbox, loopToggleButton;

    juce::AudioFormatManager formatManager;
    std::vector<std::unique_ptr<Track>> tracks;
    std::vector<juce::File> trackFiles;

    TimePitchProcessor musicalProcessor;
    TimePitchProcessor drumProcessor;

    enum TransportState { Stopped, Starting, Playing, Pausing, Paused, Stopping };
    TransportState state = Stopped;

    void changeState(TransportState newState);
    void playButtonClicked();
    void stopButtonClicked();
    void mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
