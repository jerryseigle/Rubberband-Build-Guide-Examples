#include "MainComponent.h"

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
    stopButton.setEnabled(false);

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

    // === AUDIO SETUP ===
    formatManager.registerBasicFormats();

    trackFiles = {
        juce::File("/Users/jerryseigle/Downloads/Vocals.mp3"),
        juce::File("/Users/jerryseigle/Downloads/Bass.mp3"),
        juce::File("/Users/jerryseigle/Downloads/Drums.mp3"),
    };

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

            addAndMakeVisible(newTrack->volumeSlider);
            addAndMakeVisible(newTrack->muteButton);

            tracks.push_back(std::move(newTrack));
        }
    }

    setSize(500, 150 + 70 * (int)tracks.size());
    setAudioChannels(0, 2);
    startTimerHz(10);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    for (auto& track : tracks)
        track->transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);

    drumProcessor.prepare(sampleRate, 2);
    musicalProcessor.prepare(sampleRate, 2);
}

void MainComponent::releaseResources()
{
    for (auto& track : tracks)
        track->transportSource.releaseResources();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    juce::AudioBuffer<float> drumMix, musicalMix;
    drumMix.setSize(2, bufferToFill.numSamples);
    musicalMix.setSize(2, bufferToFill.numSamples);

    // Mixers: feed fresh input to each processor
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

void MainComponent::mixTracksIntoBuffer(bool percussive, juce::AudioBuffer<float>& buffer)
{
    buffer.clear();
    for (auto& track : tracks)
    {
        if (track->isPercussive != percussive)
            continue;

        juce::AudioBuffer<float> temp;
        temp.setSize(2, buffer.getNumSamples());
        juce::AudioSourceChannelInfo info(&temp, 0, buffer.getNumSamples());
        track->transportSource.getNextAudioBlock(info);

        float volume = track->isMuted ? 0.0f : track->currentVolume;
        for (int ch = 0; ch < 2; ++ch)
            buffer.addFrom(ch, 0, temp, ch, 0, buffer.getNumSamples(), volume);
    }
}

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

    for (auto& track : tracks)
    {
        auto row = area.removeFromTop(50);
        track->volumeSlider.setBounds(row.removeFromLeft(getWidth() - 100));
        track->muteButton.setBounds(row);
    }
}

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
}

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
                    track->transportSource.start();
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
