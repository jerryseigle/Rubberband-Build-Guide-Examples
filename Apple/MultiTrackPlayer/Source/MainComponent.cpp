/*
  ==============================================================================

    MainComponent.cpp
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    Features:
    - Multi-track audio playback
    - Real-time pitch and tempo adjustment
    - Volume and mute control per track
    - Loop playback toggle (live)
    - RMS and Peak metering

  ==============================================================================
*/

#include "MainComponent.h" // Include the header for this component

//==============================================================================
// Constructor — sets up all UI elements and initializes audio
MainComponent::MainComponent()
    : state(Stopped) // Initial playback state
{
    // === PLAY BUTTON SETUP ===
    addAndMakeVisible(playButton); // Show play button
    playButton.setButtonText("Play"); // Button label
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green); // Green color
    playButton.onClick = [this] { playButtonClicked(); }; // Bind click event

    // === STOP BUTTON SETUP ===
    addAndMakeVisible(stopButton); // Show stop button
    stopButton.setButtonText("Stop"); // Button label
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red); // Red color
    stopButton.onClick = [this] { stopButtonClicked(); }; // Bind click event
    stopButton.setEnabled(false); // Disabled by default

    // === POSITION LABEL ===
    addAndMakeVisible(positionLabel); // Show position label
    positionLabel.setText("Position: 0:00.000", juce::dontSendNotification); // Initial text

    // === PITCH SLIDER ===
    addAndMakeVisible(pitchSlider); // Show pitch slider
    pitchSlider.setRange(-12.0, 12.0, 0.1); // Range in semitones
    pitchSlider.setTextValueSuffix(" st"); // Label suffix
    pitchSlider.setValue(0.0); // Default pitch
    pitchSlider.onValueChange = [this] {
        musicalProcessor.setPitchSemiTones(pitchSlider.getValue()); // Update pitch
    };

    // === TEMPO SLIDER ===
    addAndMakeVisible(tempoSlider); // Show tempo slider
    tempoSlider.setRange(0.5, 2.0, 0.01); // Range from half speed to double
    tempoSlider.setTextValueSuffix("x"); // Label suffix
    tempoSlider.setValue(1.0); // Default tempo
    tempoSlider.onValueChange = [this] {
        float inverted = 2.0f - tempoSlider.getValue(); // Calculate tempo
        drumProcessor.setTempoRatio(inverted); // Apply to drums
        musicalProcessor.setTempoRatio(inverted); // Apply to music
    };

    // === FORMANT PRESERVE TOGGLE ===
    addAndMakeVisible(formantCheckbox); // Show checkbox
    formantCheckbox.setButtonText("Preserve Formant"); // Label
    formantCheckbox.setToggleState(false, juce::dontSendNotification); // Default off
    formantCheckbox.onClick = [this] {
        bool preserve = formantCheckbox.getToggleState(); // Get state
        drumProcessor.setFormantEnabled(preserve); // Update processors
        musicalProcessor.setFormantEnabled(preserve);
    };

    // === LOOP TOGGLE BUTTON ===
    addAndMakeVisible(loopToggleButton); // Show loop toggle
    loopToggleButton.setButtonText("Loop Playback"); // Loop label
    loopToggleButton.setToggleState(false, juce::dontSendNotification); // Default off

    // Loop toggle behavior: clean handling using next read position
    loopToggleButton.onClick = [this] {
        bool shouldLoop = loopToggleButton.getToggleState(); // Get toggle state

        for (auto& track : tracks) // Apply to all tracks
        {
            if (track->readerSource)
            {
                if (!shouldLoop)
                {
                    // Before disabling looping, flush loop buffer offset
                    juce::int64 currentPos = track->readerSource->getNextReadPosition();
                    track->readerSource->setNextReadPosition(currentPos); // Set current read position
                }

                track->readerSource->setLooping(shouldLoop); // Enable/disable looping
            }

            track->transportSource.setLooping(shouldLoop); // Sync transport loop

            track->wasLooping = shouldLoop; // Track loop state
        }
    };

    // === AUDIO FORMAT MANAGER SETUP ===
    formatManager.registerBasicFormats(); // Support WAV, MP3, etc.

    juce::File resourceDir = juce::File::getSpecialLocation(
        juce::File::SpecialLocationType::invokedExecutableFile).getParentDirectory(); // Locate app folder

    // Load static test files
    trackFiles = {
        juce::File("/Users/jerryseigle/Downloads/Vocals.mp3"),
        juce::File("/Users/jerryseigle/Downloads/Bass.mp3"),
        juce::File("/Users/jerryseigle/Downloads/Drums.mp3"),
    };
    // (OPTIONAL) Absolute paths for testing on computer or any simulator
        /*
        trackFiles = {
            juce::File("/Users/jerryseigle/Downloads/Vocals.mp3"),
            juce::File("/Users/jerryseigle/Downloads/Bass.mp3"),
            juce::File("/Users/jerryseigle/Downloads/Drums.mp3"),
        };
        */

    // === LOAD TRACKS ===
    for (auto& file : trackFiles)
    {
        auto* reader = formatManager.createReaderFor(file); // Create reader
        if (reader != nullptr)
        {
            auto newTrack = std::make_unique<Track>(); // New track object

            newTrack->readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true); // Source
            newTrack->transportSource.setSource(newTrack->readerSource.get(), 0, nullptr, reader->sampleRate); // Connect

            juce::String name = file.getFileNameWithoutExtension(); // Track name
            newTrack->isPercussive = name.containsIgnoreCase("drum") ||
                                     name.containsIgnoreCase("loop") ||
                                     name.containsIgnoreCase("percussion"); // Detect percussion

            newTrack->volumeSlider.setRange(0.0, 1.0, 0.01); // Set volume range
            newTrack->volumeSlider.setValue(1.0); // Default volume
            newTrack->volumeSlider.onValueChange = [track = newTrack.get()] {
                track->currentVolume = (float)track->volumeSlider.getValue(); // Save volume
            };

            newTrack->muteButton.setButtonText("Mute"); // Label mute
            newTrack->muteButton.setToggleState(false, juce::dontSendNotification); // Off by default
            newTrack->muteButton.onClick = [track = newTrack.get()] {
                track->isMuted = track->muteButton.getToggleState(); // Save mute
            };

            newTrack->rmsLabel.setText("RMS: --", juce::dontSendNotification); // Init meter
            newTrack->peakLabel.setText("Peak: --", juce::dontSendNotification);

            addAndMakeVisible(newTrack->volumeSlider); // Show volume
            addAndMakeVisible(newTrack->muteButton); // Show mute
            addAndMakeVisible(newTrack->rmsLabel); // Show RMS
            addAndMakeVisible(newTrack->peakLabel); // Show peak

            tracks.push_back(std::move(newTrack)); // Add to list
        }
    }

    setSize(500, 150 + 90 * (int)tracks.size()); // Window size
    setAudioChannels(0, 2); // Stereo output
    startTimerHz(10); // Update UI every 100ms
}

//==============================================================================
// Destructor — shuts down audio on destruction
MainComponent::~MainComponent()
{
    shutdownAudio(); // Stops audio threads and frees resources
}

//==============================================================================
// Called when playback starts — allocates audio buffers and prepares processors
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    for (auto& track : tracks) // Prepare each track's transport
        track->transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

    drumProcessor.prepare(sampleRate, 2); // Prepare drum processor
    musicalProcessor.prepare(sampleRate, 2); // Prepare music processor
}

//==============================================================================
// Called when audio stops — releases any resources held
void MainComponent::releaseResources()
{
    for (auto& track : tracks) // Release transport buffers
        track->transportSource.releaseResources();
}

//==============================================================================
// Main audio rendering function — fills the output buffer
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion(); // Clear previous contents

    juce::AudioBuffer<float> drumMix, musicalMix; // Temporary mix buffers
    drumMix.setSize(2, bufferToFill.numSamples); // Stereo drums
    musicalMix.setSize(2, bufferToFill.numSamples); // Stereo music

    // === PROCESS PERCUSSION ===
    drumProcessor.processBlock([this](juce::AudioBuffer<float>& buffer) {
        mixTracksIntoBuffer(true, buffer); // Mix percussive tracks
    }, drumMix);

    // === PROCESS MUSIC ===
    musicalProcessor.processBlock([this](juce::AudioBuffer<float>& buffer) {
        mixTracksIntoBuffer(false, buffer); // Mix melodic tracks
    }, musicalMix);

    // === MERGE TO FINAL OUTPUT ===
    for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
    {
        bufferToFill.buffer->addFrom(ch, 0, drumMix, ch, 0, bufferToFill.numSamples); // Add drums
        bufferToFill.buffer->addFrom(ch, 0, musicalMix, ch, 0, bufferToFill.numSamples); // Add music
    }
}

//==============================================================================
// Mixes track audio into buffer with volume and metering
void MainComponent::mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer)
{
    buffer.clear(); // Start clean

    for (auto& track : tracks)
    {
        if (track->isPercussive != percussive) // Skip non-matching types
            continue;

        track->lastBuffer.setSize(2, buffer.getNumSamples()); // Prepare meter buffer
        juce::AudioSourceChannelInfo info(&track->lastBuffer, 0, buffer.getNumSamples()); // Setup temp info

        track->transportSource.getNextAudioBlock(info); // Read audio data

        float volume = track->isMuted ? 0.0f : track->currentVolume; // Mute or apply volume

        for (int ch = 0; ch < 2; ++ch)
        {
            buffer.addFrom(ch, 0, track->lastBuffer, ch, 0, buffer.getNumSamples(), volume); // Mix into output
        }
    }
}

//==============================================================================
// Handles layout for all UI components
void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10); // Shrink edges
    area.removeFromTop(80); // Reserve space for top padding

    playButton.setBounds(area.removeFromTop(30)); // Place play button
    stopButton.setBounds(area.removeFromTop(30)); // Place stop button
    positionLabel.setBounds(area.removeFromTop(30)); // Place position label
    pitchSlider.setBounds(area.removeFromTop(40)); // Place pitch slider
    tempoSlider.setBounds(area.removeFromTop(40)); // Place tempo slider
    formantCheckbox.setBounds(area.removeFromTop(30)); // Place formant toggle
    loopToggleButton.setBounds(area.removeFromTop(30)); // Place loop toggle

    for (auto& track : tracks)
    {
        auto row = area.removeFromTop(50); // Allocate space for controls
        track->volumeSlider.setBounds(row.removeFromLeft(getWidth() - 100)); // Place volume
        track->muteButton.setBounds(row); // Place mute

        auto meterRow = area.removeFromTop(20); // Allocate for meters
        track->rmsLabel.setBounds(meterRow.removeFromLeft(getWidth() / 2)); // Place RMS
        track->peakLabel.setBounds(meterRow); // Place peak
    }
}

//==============================================================================
// Updates UI and handles loop exit logic every 100ms
void MainComponent::timerCallback()
{
    // === UPDATE POSITION LABEL ===
    if (!tracks.empty() && state == Playing) // Only update if we're currently playing
    {
        // Get the position and total length of the first track
        double currentPosition = tracks.front()->transportSource.getCurrentPosition();
        double trackLength = tracks.front()->transportSource.getLengthInSeconds();

        // If looping is enabled and position has wrapped, reset the display position
        if (loopToggleButton.getToggleState() && currentPosition > trackLength)
            currentPosition = std::fmod(currentPosition, trackLength);

        // Convert position to minutes, seconds, milliseconds
        int mins = int(currentPosition) / 60;
        int secs = int(currentPosition) % 60;
        int millis = int(currentPosition * 1000) % 1000;

        // Display the formatted time (e.g., Position: 0:01.234)
        positionLabel.setText("Position: " + juce::String(mins) + ":" +
                              juce::String(secs).paddedLeft('0', 2) + "." +
                              juce::String(millis).paddedLeft('0', 3),
                              juce::dontSendNotification);
    }

    // === UPDATE METERS FOR EACH TRACK ===
    for (auto& track : tracks)
    {
        float rms = track->getRMSLevel();  // Measure current RMS level
        float peak = track->getPeakLevel(); // Measure current peak level

        // Update the RMS and Peak labels
        track->rmsLabel.setText("RMS: " + juce::String(juce::Decibels::gainToDecibels(rms), 1),
                                juce::dontSendNotification);
        track->peakLabel.setText("Peak: " + juce::String(juce::Decibels::gainToDecibels(peak), 1),
                                 juce::dontSendNotification);
    }
}

//==============================================================================
// Handles play button click
void MainComponent::playButtonClicked()
{
    // If we're stopped or paused, begin playing
    if (state == Stopped || state == Paused)
        changeState(Starting); // Move to starting state

    // If already playing, pause playback
    else if (state == Playing)
        changeState(Pausing); // Move to pausing state
}

//==============================================================================
// Handles stop button click
void MainComponent::stopButtonClicked()
{
    // If paused, return to zero
    if (state == Paused)
        changeState(Stopped); // Fully stop and rewind

    // Otherwise, initiate stop
    else
        changeState(Stopping); // Stop playback
}

//==============================================================================
// Handles playback state transitions
void MainComponent::changeState(TransportState newState)
{
    // Skip if there's no change
    if (state != newState)
    {
        state = newState; // Update state

        switch (state)
        {
            case Stopped:
                // Update buttons
                playButton.setButtonText("Play");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled(false);

                // Reset track position
                for (auto& track : tracks)
                    track->transportSource.setPosition(0.0);
                break;

            case Starting:
                for (auto& track : tracks)
                {
                    bool shouldLoop = loopToggleButton.getToggleState(); // Check loop state

                    // Apply looping config
                    if (track->readerSource)
                        track->readerSource->setLooping(shouldLoop);

                    track->transportSource.setLooping(shouldLoop);

                    if (shouldLoop)
                        track->wasLooping = true; // Record active looping

                    track->transportSource.start(); // Begin playback
                }

                changeState(Playing); // Transition immediately to playing
                break;

            case Playing:
                playButton.setButtonText("Pause"); // Update button
                stopButton.setButtonText("Stop");
                stopButton.setEnabled(true); // Enable stop
                break;

            case Pausing:
                for (auto& track : tracks)
                    track->transportSource.stop(); // Stop audio but keep position

                changeState(Paused); // Move to paused state
                break;

            case Paused:
                playButton.setButtonText("Resume"); // Update button
                stopButton.setButtonText("Return to Zero");
                break;

            case Stopping:
                for (auto& track : tracks)
                    track->transportSource.stop(); // Stop and reset

                changeState(Stopped); // Move to full stop
                break;
        }
    }
}
