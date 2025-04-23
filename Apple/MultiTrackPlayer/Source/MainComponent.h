/*
  ==============================================================================

    MainComponent.h
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    This file defines the MainComponent class, which serves as the central
    UI and audio engine for the app. It handles:
    
    - Loading and managing multiple audio tracks (e.g., vocals, bass, drums)
    - Real-time pitch shifting and tempo adjustment using RubberBand
    - Volume/mute control per track
    - Playback transport logic (Play, Pause, Stop)
    - UI layout and user interaction

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TimePitchProcessor.h"

//==============================================================================
// Track
//
// Represents a single audio track, such as vocals, bass, or drums.
// Each track contains:
// - An AudioFormatReaderSource to read audio from a file
// - A TransportSource to control playback
// - Volume and mute controls for user interaction
// - State flags for volume, mute status, and whether it is percussive
//==============================================================================
struct Track
{
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource; // Reads audio data
    juce::AudioTransportSource transportSource;                  // Controls playback of audio

    juce::Slider volumeSlider;      // Slider for controlling volume of this track
    juce::ToggleButton muteButton;  // Toggle button to mute/unmute this track

    float currentVolume = 1.0f;     // Current volume level (0.0 = silent, 1.0 = full)
    bool isMuted = false;          // Whether the track is currently muted
    bool isPercussive = false;     // Whether the track is percussive (e.g., drums)
};

//==============================================================================
// MainComponent
//
// This class is the main application component, responsible for both the user
// interface and audio processing. It manages:
// - Loading audio files into tracks
// - Applying pitch and tempo changes using the RubberBand library
// - Drawing and updating the UI
// - Handling audio playback states (play, pause, stop)
//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // JUCE audio callback: called before playback starts
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;

    // JUCE audio callback: called to fill the audio output buffer
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // JUCE audio callback: called when audio playback stops
    void releaseResources() override;

    // JUCE component layout: positions UI components
    void resized() override;

    // Timer callback: updates UI (e.g., playback position) regularly
    void timerCallback() override;

private:
    //==================================================
    // Transport State Enum
    //
    // Represents the current playback state of the audio engine.
    // Used to control transitions between states (e.g., Playing, Paused).
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

    // Change to a new transport state and update UI/audio accordingly
    void changeState(TransportState newState);

    // UI event: triggered when play button is clicked
    void playButtonClicked();

    // UI event: triggered when stop button is clicked
    void stopButtonClicked();

    //============================
    // UI Components
    //============================
    juce::TextButton playButton, stopButton; // Start/stop audio playback
    juce::Label positionLabel;              // Displays playback position in minutes/seconds
    juce::Slider pitchSlider, tempoSlider;  // Pitch (semitones) and tempo (speed) controls
    juce::ToggleButton formantCheckbox;     // Preserve formant checkbox (voice characteristics)

    //============================
    // Audio System Components
    //============================
    juce::AudioFormatManager formatManager;           // Responsible for decoding audio formats (e.g. MP3, WAV)
    std::vector<std::unique_ptr<Track>> tracks;       // All loaded tracks (vocals, drums, etc.)
    std::vector<juce::File> trackFiles;               // Audio files associated with each track

    //============================
    // Time & Pitch Processors
    //============================
    TimePitchProcessor musicalProcessor;  // Processor for musical elements (e.g., vocals, bass)
    TimePitchProcessor drumProcessor;     // Processor for percussive elements (e.g., drums)

    //============================
    // Playback State
    //============================
    TransportState state = Stopped;       // Current state of playback (e.g., playing, paused)

    // Mixes selected tracks into the output buffer for real-time processing
    void mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
