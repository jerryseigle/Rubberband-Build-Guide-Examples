/*
  ==============================================================================
    MainComponent.cpp
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    This component sets up a JUCE app that loads an audio file and uses
    the RubberBand library to apply real-time pitch and tempo manipulation.
    A checkbox allows you to enable or disable formant preservation.
  ==============================================================================
*/

#include "MainComponent.h"

MainComponent::MainComponent()
    : state(Stopped)
{
    // === UI SETUP ===

    // Play button
    addAndMakeVisible(&playButton);
    playButton.setButtonText("Play");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setEnabled(false); // Enable after audio loads

    // Stop button
    addAndMakeVisible(stopButton);
    stopButton.setButtonText("Stop");
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setEnabled(false);

    // Playback position label
    addAndMakeVisible(positionLabel);
    positionLabel.setText("Position: 0:00.000", juce::dontSendNotification);

    // Pitch Slider - controls semitone shift
    addAndMakeVisible(pitchSlider);
    pitchSlider.setRange(-12.0, 12.0, 0.1); // +/- 12 semitones
    pitchSlider.setTextValueSuffix(" st");
    pitchSlider.setValue(0.0);
    pitchSlider.onValueChange = [this] {
        timePitchProcessor.setPitchSemiTones(pitchSlider.getValue());
    };

    // Tempo Slider - inverted because RubberBand works inversely
    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(0.5, 2.0, 0.01); // 0.5x to 2.0x
    tempoSlider.setTextValueSuffix("x");
    tempoSlider.setValue(1.0); // 1.0 = normal speed
    tempoSlider.onValueChange = [this] {
        float inverted = 2.0f - tempoSlider.getValue(); // invert to match natural slider direction
        timePitchProcessor.setTempoRatio(inverted);
    };

    // Formant Checkbox - toggles voice preservation
    addAndMakeVisible(formantCheckbox);
    formantCheckbox.setButtonText("Preserve Formant");
    formantCheckbox.setToggleState(false, juce::dontSendNotification); // default OFF
    formantCheckbox.onClick = [this] {
        timePitchProcessor.setFormantEnabled(formantCheckbox.getToggleState());
    };

    // === AUDIO SETUP ===

    // Register audio formats (MP3, WAV, etc.)
    formatManager.registerBasicFormats();

    // Load the MP3 file from the app’s bundle (update to the path of your audio file)
    juce::File audioFile("/the-absolute-path-to-audio-file/SampleAudio.mp3");


    if (audioFile.existsAsFile())
    {
        auto* reader = formatManager.createReaderFor(audioFile);
        if (reader != nullptr)
        {
            readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            playButton.setEnabled(true);
        }
    }

    setSize(400, 350);         // Set window size
    setAudioChannels(0, 2);    // Stereo output
    startTimerHz(10);          // Timer for UI update
}

MainComponent::~MainComponent()
{
    shutdownAudio(); // Clean up
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    if (readerSource)
        readerSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    // Prepares the RubberBand processor with sample rate and channel info
    timePitchProcessor.prepare(sampleRate, 2);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // If we’re not playing, clear the output buffer
    if (!readerSource || state != Playing)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    // Process audio block with RubberBand pitch & tempo changes
    timePitchProcessor.processBlock(readerSource.get(), *bufferToFill.buffer);
}

void MainComponent::releaseResources()
{
    if (readerSource)
        readerSource->releaseResources();
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(80);
    playButton.setBounds(area.removeFromTop(40));
    stopButton.setBounds(area.removeFromTop(40));
    positionLabel.setBounds(area.removeFromTop(40));
    pitchSlider.setBounds(area.removeFromTop(40));
    tempoSlider.setBounds(area.removeFromTop(40));
    formantCheckbox.setBounds(area.removeFromTop(40));
}

void MainComponent::timerCallback()
{
    if (readerSource && state == Playing)
    {
        auto pos = readerSource->getNextReadPosition() / 44100.0;
        int mins = int(pos) / 60;
        int secs = int(pos) % 60;
        int millis = int(pos * 1000) % 1000;

        positionLabel.setText("Position: " + juce::String(mins) + ":" +
                              juce::String(secs).paddedLeft('0', 2) + "." +
                              juce::String(millis).paddedLeft('0', 3),
                              juce::dontSendNotification);
    }
}

// === AUDIO STATE MACHINE ===
// This handles changing button states and playback behavior

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
                if (readerSource)
                    readerSource->setNextReadPosition(0);
                break;

            case Starting:
                changeState(Playing); // We transition directly to playing
                break;

            case Playing:
                playButton.setButtonText("Pause");
                stopButton.setButtonText("Stop");
                stopButton.setEnabled(true);
                break;

            case Pausing:
                changeState(Paused);
                break;

            case Paused:
                playButton.setButtonText("Resume");
                stopButton.setButtonText("Return to Zero");
                break;

            case Stopping:
                changeState(Stopped);
                break;
        }
    }
}

void MainComponent::playButtonClicked()
{
    if (state == Stopped || state == Paused)
        changeState(Starting);
    else if (state == Playing)
        changeState(Pausing);
}

void MainComponent::stopButtonClicked()
{
    if (state == Paused)
        changeState(Stopped);
    else
        changeState(Stopping);
}

