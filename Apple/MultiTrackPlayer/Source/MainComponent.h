/*
  ==============================================================================

    MainComponent.h
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    This file defines the MainComponent class, which manages:
    - Audio playback of multiple tracks
    - Real-time pitch and tempo adjustments
    - Volume/mute controls per track
    - RMS and Peak metering for each track
    - UI layout, interaction, and looping toggle

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TimePitchProcessor.h"

//==============================================================================
// Track
//
// Represents a single audio track (e.g., vocals, bass, drums) with:
// - Volume and mute controls
// - An audio transport source for playback
// - Metering support (RMS and Peak)
//==============================================================================
struct Track
{
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource; // Reads audio from file
    juce::AudioTransportSource transportSource;                  // Handles playback control

    juce::Slider volumeSlider;      // Volume control
    juce::ToggleButton muteButton;  // Mute control

    juce::Label rmsLabel;           // UI label for RMS level
    juce::Label peakLabel;          // UI label for Peak level

    float currentVolume = 1.0f;     // Current volume (0.0 - 1.0)
    bool isMuted = false;          // Mute flag
    bool isPercussive = false;     // Used to route track to drum or music processor

    juce::AudioBuffer<float> lastBuffer; // Stores last played audio block for metering

    // Returns the RMS (root mean square) level of the last buffer
    float getRMSLevel() const
    {
        if (lastBuffer.getNumSamples() == 0)
            return 0.0f;
        return lastBuffer.getRMSLevel(0, 0, lastBuffer.getNumSamples());
    }

    // Returns the peak magnitude of the last buffer
    float getPeakLevel() const
    {
        if (lastBuffer.getNumSamples() == 0)
            return 0.0f;
        return lastBuffer.getMagnitude(0, 0, lastBuffer.getNumSamples());
    }
};

//==============================================================================
// MainComponent
//
// Manages all UI and audio processing logic for the app, including:
// - Loading and mixing tracks
// - Real-time pitch/tempo processing
// - Track-specific volume/mute control
// - RMS/Peak metering
// - Loop toggle and playback controls
//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Called by JUCE to prepare the audio system
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    // Called by JUCE to process audio blocks
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // Called when audio playback is being shut down
    void releaseResources() override;

    // Called when the UI is resized (layout logic)
    void resized() override;

    // Called by timer (used to update metering and playback time)
    void timerCallback() override;

private:
    //==================================================
    // Playback transport state machine
    //==================================================
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Pausing,
        Paused,
        Stopping
    };

    // Updates the internal state and UI for transport
    void changeState(TransportState newState);

    // Button click handlers
    void playButtonClicked();
    void stopButtonClicked();

    //==================================================
    // UI Components
    //==================================================
    juce::TextButton playButton, stopButton;     // Main playback controls
    juce::Label positionLabel;                   // Displays current time
    juce::Slider pitchSlider, tempoSlider;       // Pitch and tempo controls
    juce::ToggleButton formantCheckbox;          // Toggle to preserve formants
    juce::ToggleButton loopToggleButton;         // Toggle looping on/off

    //==================================================
    // Audio Playback
    //==================================================
    juce::AudioFormatManager formatManager;                  // Manages file decoding
    std::vector<std::unique_ptr<Track>> tracks;              // All loaded tracks
    std::vector<juce::File> trackFiles;                      // Paths to the audio files

    //==================================================
    // Audio Processing
    //==================================================
    TimePitchProcessor musicalProcessor; // Pitch/time for musical tracks
    TimePitchProcessor drumProcessor;    // Pitch/time for drum/percussive tracks

    //==================================================
    // Playback state
    //==================================================
    TransportState state = Stopped;

    // Mixes all matching tracks (percussive or melodic) into the buffer
    void mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
