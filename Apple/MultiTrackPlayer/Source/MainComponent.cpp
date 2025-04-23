/*
  ==============================================================================

    MainComponent.cpp
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    This file contains the implementation of the MainComponent class.

    Responsibilities:
    - Build the UI (buttons, sliders, labels for each track)
    - Load multiple audio tracks from local files
    - Route tracks through pitch/time processors
    - Control transport logic (play, stop, pause)
    - Provide real-time feedback to the user

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
// Constructor: Initializes UI components, loads audio files, and sets up playback
MainComponent::MainComponent()
    : state(Stopped)
{
    // === UI Setup ===

    // Configure the Play button
    addAndMakeVisible(playButton);
    playButton.setButtonText("Play");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.onClick = [this] { playButtonClicked(); };

    // Configure the Stop button
    addAndMakeVisible(stopButton);
    stopButton.setButtonText("Stop");
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setEnabled(false); // Disable until playback begins

    // Position label shows current playback time
    addAndMakeVisible(positionLabel);
    positionLabel.setText("Position: 0:00.000", juce::dontSendNotification);

    // Pitch control slider: shifts pitch in semitones
    addAndMakeVisible(pitchSlider);
    pitchSlider.setRange(-12.0, 12.0, 0.1);
    pitchSlider.setTextValueSuffix(" st");
    pitchSlider.setValue(0.0);
    pitchSlider.onValueChange = [this] {
        musicalProcessor.setPitchSemiTones(pitchSlider.getValue());
    };

    // Tempo control slider: adjusts playback speed
    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(0.5, 2.0, 0.01);
    tempoSlider.setTextValueSuffix("x");
    tempoSlider.setValue(1.0);
    tempoSlider.onValueChange = [this] {
        float inverted = 2.0f - tempoSlider.getValue(); // Inverted slider logic
        drumProcessor.setTempoRatio(inverted);          // Update drum processor
        musicalProcessor.setTempoRatio(inverted);       // Update musical processor
    };

    // Checkbox to enable or disable formant preservation
    addAndMakeVisible(formantCheckbox);
    formantCheckbox.setButtonText("Preserve Formant");
    formantCheckbox.setToggleState(false, juce::dontSendNotification);
    formantCheckbox.onClick = [this] {
        bool preserve = formantCheckbox.getToggleState();
        drumProcessor.setFormantEnabled(preserve);
        musicalProcessor.setFormantEnabled(preserve);
    };

    // === Audio Setup ===

    formatManager.registerBasicFormats(); // Enable support for WAV, MP3, etc.

    // Locate the directory where this binary is running from
    juce::File resourceDir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::invokedExecutableFile)
        .getParentDirectory();

    // Define relative audio file paths
    trackFiles = {
        resourceDir.getChildFile("Vocals.mp3"),
        resourceDir.getChildFile("Bass.mp3"),
        resourceDir.getChildFile("Drums.mp3"),
    };

    DBG("Resource path: " + resourceDir.getFullPathName());

    // === Load and configure each track ===
    for (auto& file : trackFiles)
    {
        auto* reader = formatManager.createReaderFor(file); // Open file for reading
        if (reader != nullptr)
        {
            auto newTrack = std::make_unique<Track>();

            // Link the file reader to a transport source
            newTrack->readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            newTrack->transportSource.setSource(newTrack->readerSource.get(), 0, nullptr, reader->sampleRate);

            // Determine if track is percussive based on filename
            juce::String name = file.getFileNameWithoutExtension();
            newTrack->isPercussive = name.containsIgnoreCase("drum") ||
                                     name.containsIgnoreCase("loop") ||
                                     name.containsIgnoreCase("percussion");

            // Volume slider setup
            newTrack->volumeSlider.setRange(0.0, 1.0, 0.01);
            newTrack->volumeSlider.setValue(1.0);
            newTrack->volumeSlider.onValueChange = [track = newTrack.get()] {
                track->currentVolume = (float)track->volumeSlider.getValue();
            };

            // Mute toggle button setup
            newTrack->muteButton.setButtonText("Mute");
            newTrack->muteButton.setToggleState(false, juce::dontSendNotification);
            newTrack->muteButton.onClick = [track = newTrack.get()] {
                track->isMuted = track->muteButton.getToggleState();
            };

            // Add track UI elements to screen
            addAndMakeVisible(newTrack->volumeSlider);
            addAndMakeVisible(newTrack->muteButton);

            // Store track in list
            tracks.push_back(std::move(newTrack));
        }
    }

    // Resize window based on track count
    setSize(500, 150 + 70 * (int)tracks.size());

    // Set up stereo output (no inputs)
    setAudioChannels(0, 2);

    // Start a UI update timer that ticks 10 times per second
    startTimerHz(10);
}

//==============================================================================
// Destructor: safely shuts down audio engine and releases resources
MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
// Called before playback begins — prepares audio sources and processors
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    for (auto& track : tracks)
        track->transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

    drumProcessor.prepare(sampleRate, 2);     // 2 = stereo
    musicalProcessor.prepare(sampleRate, 2);
}

//==============================================================================
// Called when audio playback is stopping — releases audio resources
void MainComponent::releaseResources()
{
    for (auto& track : tracks)
        track->transportSource.releaseResources();
}

//==============================================================================
// This function is called repeatedly to supply audio to the output buffer
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion(); // Start with silence

    // Temporary buffers for each processor's output
    juce::AudioBuffer<float> drumMix;
    juce::AudioBuffer<float> musicalMix;
    drumMix.setSize(2, bufferToFill.numSamples);
    musicalMix.setSize(2, bufferToFill.numSamples);

    // Fill drum processor with percussive tracks
    drumProcessor.processBlock([this](juce::AudioBuffer<float>& buffer) {
        mixTracksIntoBuffer(true, buffer);
    }, drumMix);

    // Fill musical processor with melodic tracks
    musicalProcessor.processBlock([this](juce::AudioBuffer<float>& buffer) {
        mixTracksIntoBuffer(false, buffer);
    }, musicalMix);

    // Combine drum and musical output into final output
    for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
    {
        bufferToFill.buffer->addFrom(ch, 0, drumMix, ch, 0, bufferToFill.numSamples);
        bufferToFill.buffer->addFrom(ch, 0, musicalMix, ch, 0, bufferToFill.numSamples);
    }
}

//==============================================================================
// Mixes active tracks into the output buffer
// - percussive: true to mix only drums, false for melodic tracks
// - buffer: destination buffer to mix into
void MainComponent::mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer)
{
    buffer.clear(); // Clear to silence

    for (auto& track : tracks)
    {
        // Skip tracks that don’t match the percussive flag
        if (track->isPercussive != percussive)
            continue;

        // Create a temporary buffer for this track’s audio
        juce::AudioBuffer<float> temp;
        temp.setSize(2, buffer.getNumSamples());

        // Fill the temp buffer with the track's audio
        juce::AudioSourceChannelInfo info(&temp, 0, buffer.getNumSamples());
        track->transportSource.getNextAudioBlock(info);

        // Set volume: 0 if muted, otherwise use current slider value
        float volume = track->isMuted ? 0.0f : track->currentVolume;

        // Add the track's audio to the output buffer with proper volume
        for (int ch = 0; ch < 2; ++ch)
            buffer.addFrom(ch, 0, temp, ch, 0, buffer.getNumSamples(), volume);
    }
}

//==============================================================================
// Lays out UI components each time the window is resized
void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10); // Add padding around the edges
    area.removeFromTop(80);                   // Space before controls

    // Layout fixed UI elements
    playButton.setBounds(area.removeFromTop(30));
    stopButton.setBounds(area.removeFromTop(30));
    positionLabel.setBounds(area.removeFromTop(30));
    pitchSlider.setBounds(area.removeFromTop(40));
    tempoSlider.setBounds(area.removeFromTop(40));
    formantCheckbox.setBounds(area.removeFromTop(30));

    // Layout volume and mute controls for each track
    for (auto& track : tracks)
    {
        auto row = area.removeFromTop(50);
        track->volumeSlider.setBounds(row.removeFromLeft(getWidth() - 100));
        track->muteButton.setBounds(row);
    }
}

//==============================================================================
// Updates the playback position label every 1/10th of a second
void MainComponent::timerCallback()
{
    if (!tracks.empty() && state == Playing)
    {
        auto pos = tracks.front()->transportSource.getCurrentPosition();

        int mins = int(pos) / 60;
        int secs = int(pos) % 60;
        int millis = int(pos * 1000) % 1000;

        // Update label with formatted time string
        positionLabel.setText("Position: " + juce::String(mins) + ":" +
                              juce::String(secs).paddedLeft('0', 2) + "." +
                              juce::String(millis).paddedLeft('0', 3),
                              juce::dontSendNotification);
    }
}

//==============================================================================
// Changes the state of the playback engine and updates the UI accordingly
void MainComponent::changeState(TransportState newState)
{
    if (state != newState)
    {
        state = newState;

        switch (state)
        {
            case Stopped:
                playButton.setButtonText("Play");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled(false);
                for (auto& track : tracks)
                    track->transportSource.setPosition(0.0); // Reset to beginning
                break;

            case Starting:
                for (auto& track : tracks)
                    track->transportSource.start(); // Start playback
                changeState(Playing);
                break;

            case Playing:
                playButton.setButtonText("Pause");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled(true);
                break;

            case Pausing:
                for (auto& track : tracks)
                    track->transportSource.stop(); // Pause playback
                changeState(Paused);
                break;

            case Paused:
                playButton.setButtonText("Resume");
                stopButton.setButtonText("Return to Zero");
                break;

            case Stopping:
                for (auto& track : tracks)
                    track->transportSource.stop(); // Stop playback
                changeState(Stopped);
                break;
        }
    }
}

//==============================================================================
// Called when the Play button is clicked — toggles playback state
void MainComponent::playButtonClicked()
{
    if (state == Stopped || state == Paused)
        changeState(Starting);
    else if (state == Playing)
        changeState(Pausing);
}

//==============================================================================
// Called when the Stop button is clicked — stops or resets playback
void MainComponent::stopButtonClicked()
{
    if (state == Paused)
        changeState(Stopped);
    else
        changeState(Stopping);
}
