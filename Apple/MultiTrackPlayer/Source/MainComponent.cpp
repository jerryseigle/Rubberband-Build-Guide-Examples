/*
  ==============================================================================

    MainComponent.cpp
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    This file implements the MainComponent class logic.

    Features:
    - Multi-track audio playback
    - Real-time pitch and tempo adjustment
    - Volume and mute control per track
    - Loop playback toggle
    - RMS and Peak metering per track (live updating)

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
// Constructor — sets up UI components, audio files, processors, and timer
MainComponent::MainComponent()
    : state(Stopped)
{
    // === UI SETUP ===

    addAndMakeVisible(playButton);
    playButton.setButtonText("Play");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.onClick = [this] { playButtonClicked(); };

    addAndMakeVisible(stopButton);
    stopButton.setButtonText("Stop");
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setEnabled(false); // Disabled until playback begins

    addAndMakeVisible(positionLabel);
    positionLabel.setText("Position: 0:00.000", juce::dontSendNotification);

    addAndMakeVisible(pitchSlider);
    pitchSlider.setRange(-12.0, 12.0, 0.1);
    pitchSlider.setTextValueSuffix(" st");
    pitchSlider.setValue(0.0);
    pitchSlider.onValueChange = [this] {
        musicalProcessor.setPitchSemiTones(pitchSlider.getValue());
    };

    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(0.5, 2.0, 0.01);
    tempoSlider.setTextValueSuffix("x");
    tempoSlider.setValue(1.0);
    tempoSlider.onValueChange = [this] {
        float inverted = 2.0f - tempoSlider.getValue();
        drumProcessor.setTempoRatio(inverted);
        musicalProcessor.setTempoRatio(inverted);
    };

    addAndMakeVisible(formantCheckbox);
    formantCheckbox.setButtonText("Preserve Formant");
    formantCheckbox.setToggleState(false, juce::dontSendNotification);
    formantCheckbox.onClick = [this] {
        bool preserve = formantCheckbox.getToggleState();
        drumProcessor.setFormantEnabled(preserve);
        musicalProcessor.setFormantEnabled(preserve);
    };

    addAndMakeVisible(loopToggleButton);
    loopToggleButton.setButtonText("Loop Playback");
    loopToggleButton.setToggleState(false, juce::dontSendNotification);

    // === AUDIO SETUP ===

    formatManager.registerBasicFormats(); // MP3, WAV, etc.

    juce::File resourceDir = juce::File::getSpecialLocation(
        juce::File::SpecialLocationType::invokedExecutableFile).getParentDirectory();

    trackFiles = {
        resourceDir.getChildFile("Vocals.mp3"),
        resourceDir.getChildFile("Bass.mp3"),
        resourceDir.getChildFile("Drums.mp3"),
    };

    DBG("Resource path: " + resourceDir.getFullPathName());

    // (OPTIONAL) Absolute paths for macOS testing
    /*
    trackFiles = {
        juce::File("/Users/jerryseigle/Downloads/Vocals.mp3"),
        juce::File("/Users/jerryseigle/Downloads/Bass.mp3"),
        juce::File("/Users/jerryseigle/Downloads/Drums.mp3"),
    };
    */

    // Load and configure each track
    for (auto& file : trackFiles)
    {
        auto* reader = formatManager.createReaderFor(file);
        if (reader != nullptr)
        {
            auto newTrack = std::make_unique<Track>();

            newTrack->readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            newTrack->transportSource.setSource(newTrack->readerSource.get(), 0, nullptr, reader->sampleRate);

            juce::String name = file.getFileNameWithoutExtension();
            newTrack->isPercussive = name.containsIgnoreCase("drum") ||
                                     name.containsIgnoreCase("loop") ||
                                     name.containsIgnoreCase("percussion");

            newTrack->volumeSlider.setRange(0.0, 1.0, 0.01);
            newTrack->volumeSlider.setValue(1.0);
            newTrack->volumeSlider.onValueChange = [track = newTrack.get()] {
                track->currentVolume = (float)track->volumeSlider.getValue();
            };

            newTrack->muteButton.setButtonText("Mute");
            newTrack->muteButton.setToggleState(false, juce::dontSendNotification);
            newTrack->muteButton.onClick = [track = newTrack.get()] {
                track->isMuted = track->muteButton.getToggleState();
            };

            newTrack->rmsLabel.setText("RMS: --", juce::dontSendNotification);
            newTrack->peakLabel.setText("Peak: --", juce::dontSendNotification);

            addAndMakeVisible(newTrack->volumeSlider);
            addAndMakeVisible(newTrack->muteButton);
            addAndMakeVisible(newTrack->rmsLabel);
            addAndMakeVisible(newTrack->peakLabel);

            tracks.push_back(std::move(newTrack));
        }
    }

    setSize(500, 150 + 90 * (int)tracks.size());
    setAudioChannels(0, 2); // Stereo output
    startTimerHz(10);       // UI updates every 100ms
}

//==============================================================================
// Destructor — shuts down audio system
MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
// Prepares each track and processor before playback begins
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    for (auto& track : tracks)
        track->transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

    drumProcessor.prepare(sampleRate, 2);
    musicalProcessor.prepare(sampleRate, 2);
}

//==============================================================================
// Releases audio resources after playback stops
void MainComponent::releaseResources()
{
    for (auto& track : tracks)
        track->transportSource.releaseResources();
}

//==============================================================================
// Called repeatedly to fill the output buffer with audio
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    juce::AudioBuffer<float> drumMix, musicalMix;
    drumMix.setSize(2, bufferToFill.numSamples);
    musicalMix.setSize(2, bufferToFill.numSamples);

    drumProcessor.processBlock([this](juce::AudioBuffer<float>& buffer) {
        mixTracksIntoBuffer(true, buffer);
    }, drumMix);

    musicalProcessor.processBlock([this](juce::AudioBuffer<float>& buffer) {
        mixTracksIntoBuffer(false, buffer);
    }, musicalMix);

    for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
    {
        bufferToFill.buffer->addFrom(ch, 0, drumMix, ch, 0, bufferToFill.numSamples);
        bufferToFill.buffer->addFrom(ch, 0, musicalMix, ch, 0, bufferToFill.numSamples);
    }
}

//==============================================================================
// Mixes audio from tracks into a buffer, applies volume, and updates metering
void MainComponent::mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer)
{
    buffer.clear();

    for (auto& track : tracks)
    {
        if (track->isPercussive != percussive)
            continue;

        track->lastBuffer.setSize(2, buffer.getNumSamples());
        juce::AudioBuffer<float>& temp = track->lastBuffer;

        juce::AudioSourceChannelInfo info(&temp, 0, buffer.getNumSamples());
        track->transportSource.getNextAudioBlock(info);

        float volume = track->isMuted ? 0.0f : track->currentVolume;

        for (int ch = 0; ch < 2; ++ch)
            buffer.addFrom(ch, 0, temp, ch, 0, buffer.getNumSamples(), volume);
    }
}

//==============================================================================
// Handles layout and position of UI components
void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(80);

    playButton.setBounds(area.removeFromTop(30));
    stopButton.setBounds(area.removeFromTop(30));
    positionLabel.setBounds(area.removeFromTop(30));
    pitchSlider.setBounds(area.removeFromTop(40));
    tempoSlider.setBounds(area.removeFromTop(40));
    formantCheckbox.setBounds(area.removeFromTop(30));
    loopToggleButton.setBounds(area.removeFromTop(30));

    for (auto& track : tracks)
    {
        auto row = area.removeFromTop(50);
        track->volumeSlider.setBounds(row.removeFromLeft(getWidth() - 100));
        track->muteButton.setBounds(row);

        auto meterRow = area.removeFromTop(20);
        track->rmsLabel.setBounds(meterRow.removeFromLeft(getWidth() / 2));
        track->peakLabel.setBounds(meterRow);
    }
}

//==============================================================================
// Updates playback time and metering display every 100ms
void MainComponent::timerCallback()
{
    if (!tracks.empty() && state == Playing)
    {
        auto pos = tracks.front()->transportSource.getCurrentPosition();
        int mins = int(pos) / 60;
        int secs = int(pos) % 60;
        int millis = int(pos * 1000) % 1000;

        positionLabel.setText("Position: " + juce::String(mins) + ":" +
                              juce::String(secs).paddedLeft('0', 2) + "." +
                              juce::String(millis).paddedLeft('0', 3),
                              juce::dontSendNotification);
    }

    for (auto& track : tracks)
    {
        float rms = track->getRMSLevel();
        float peak = track->getPeakLevel();

        track->rmsLabel.setText("RMS: " + juce::String(juce::Decibels::gainToDecibels(rms), 1), juce::dontSendNotification);
        track->peakLabel.setText("Peak: " + juce::String(juce::Decibels::gainToDecibels(peak), 1), juce::dontSendNotification);
    }
}

//==============================================================================
// Handles playback state transitions and loop behavior
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
                    track->transportSource.setPosition(0.0);
                break;

            case Starting:
                for (auto& track : tracks)
                {
                    bool shouldLoop = loopToggleButton.getToggleState();

                    // 🔁 Critical: set loop on both sources
                    if (track->readerSource)
                        track->readerSource->setLooping(shouldLoop);

                    track->transportSource.setLooping(shouldLoop);
                    track->transportSource.start();
                }
                changeState(Playing);
                break;

            case Playing:
                playButton.setButtonText("Pause");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled(true);
                break;

            case Pausing:
                for (auto& track : tracks)
                    track->transportSource.stop();
                changeState(Paused);
                break;

            case Paused:
                playButton.setButtonText("Resume");
                stopButton.setButtonText("Return to Zero");
                break;

            case Stopping:
                for (auto& track : tracks)
                    track->transportSource.stop();
                changeState(Stopped);
                break;
        }
    }
}

//==============================================================================
// Handles play button click logic
void MainComponent::playButtonClicked()
{
    if (state == Stopped || state == Paused)
        changeState(Starting);
    else if (state == Playing)
        changeState(Pausing);
}

//==============================================================================
// Handles stop button click logic
void MainComponent::stopButtonClicked()
{
    if (state == Paused)
        changeState(Stopped);
    else
        changeState(Stopping);
}
