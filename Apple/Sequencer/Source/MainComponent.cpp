#include "MainComponent.h"

MainComponent::MainComponent()
    : state(Stopped)
{
    addAndMakeVisible(playButton);
    playButton.setButtonText("Play All");
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.onClick = [this] { playButtonClicked(); };

    addAndMakeVisible(stopButton);
    stopButton.setButtonText("Stop All");
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
            newTrack->isPercussive = name.containsIgnoreCase("drum") || name.containsIgnoreCase("loop");

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

            newTrack->playPauseButton.onClick = [track = newTrack.get()] {
                if (track->transportSource.isPlaying())
                {
                    track->transportSource.stop();
                    track->playPauseButton.setButtonText("Play");
                }
                else
                {
                    if (track->readerSource)
                        track->readerSource->setLooping(track->loopToggle.getToggleState());

                    track->transportSource.setLooping(track->loopToggle.getToggleState());
                    track->transportSource.start();
                    track->playPauseButton.setButtonText("Pause");
                }
            };

            newTrack->stopButton.onClick = [track = newTrack.get()] {
                track->transportSource.stop();
                track->transportSource.setPosition(0.0);
                track->playPauseButton.setButtonText("Play");
            };

            newTrack->loopToggle.onClick = [track = newTrack.get()] {
                bool shouldLoop = track->loopToggle.getToggleState();
                if (track->readerSource)
                    track->readerSource->setLooping(shouldLoop);
                track->transportSource.setLooping(shouldLoop);
            };

            newTrack->rmsLabel.setText("RMS: --", juce::dontSendNotification);
            newTrack->peakLabel.setText("Peak: --", juce::dontSendNotification);

            addAndMakeVisible(newTrack->volumeSlider);
            addAndMakeVisible(newTrack->muteButton);
            addAndMakeVisible(newTrack->rmsLabel);
            addAndMakeVisible(newTrack->peakLabel);
            addAndMakeVisible(newTrack->playPauseButton);
            addAndMakeVisible(newTrack->stopButton);
            addAndMakeVisible(newTrack->loopToggle);

            tracks.push_back(std::move(newTrack));
        }
    }

    setSize(500, 200 + 120 * (int)tracks.size());
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

        track->lastBuffer.setSize(2, buffer.getNumSamples());
        juce::AudioSourceChannelInfo info(&track->lastBuffer, 0, buffer.getNumSamples());

        track->transportSource.getNextAudioBlock(info);

        float volume = track->isMuted ? 0.0f : track->currentVolume;

        for (int ch = 0; ch < 2; ++ch)
        {
            buffer.addFrom(ch, 0, track->lastBuffer, ch, 0, buffer.getNumSamples(), volume);
        }
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
        auto row = area.removeFromTop(25);
        track->playPauseButton.setBounds(row.removeFromLeft(60));
        track->stopButton.setBounds(row.removeFromLeft(60));
        track->loopToggle.setBounds(row.removeFromLeft(80));
        track->volumeSlider.setBounds(row.removeFromLeft(getWidth() - 220));
        track->muteButton.setBounds(row);

        auto meterRow = area.removeFromTop(20);
        track->rmsLabel.setBounds(meterRow.removeFromLeft(getWidth() / 2));
        track->peakLabel.setBounds(meterRow);
        area.removeFromTop(10); // Spacer
    }
}

void MainComponent::timerCallback()
{
    if (!tracks.empty() && state == Playing)
    {
        double currentPosition = tracks.front()->transportSource.getCurrentPosition();
        double trackLength = tracks.front()->transportSource.getLengthInSeconds();

        if (loopToggleButton.getToggleState() && currentPosition > trackLength)
            currentPosition = std::fmod(currentPosition, trackLength);

        int mins = int(currentPosition) / 60;
        int secs = int(currentPosition) % 60;
        int millis = int(currentPosition * 1000) % 1000;

        positionLabel.setText("Position: " + juce::String(mins) + ":" +
                              juce::String(secs).paddedLeft('0', 2) + "." +
                              juce::String(millis).paddedLeft('0', 3),
                              juce::dontSendNotification);
    }

    for (auto& track : tracks)
    {
        float rms = track->getRMSLevel();
        float peak = track->getPeakLevel();

        track->rmsLabel.setText("RMS: " + juce::String(juce::Decibels::gainToDecibels(rms), 1),
                                juce::dontSendNotification);
        track->peakLabel.setText("Peak: " + juce::String(juce::Decibels::gainToDecibels(peak), 1),
                                 juce::dontSendNotification);
    }
}

void MainComponent::playButtonClicked()
{
    for (auto& track : tracks)
    {
        if (track->readerSource)
            track->readerSource->setLooping(track->loopToggle.getToggleState());

        track->transportSource.setLooping(track->loopToggle.getToggleState());
        track->transportSource.start();
        track->playPauseButton.setButtonText("Pause");
    }
    state = Playing;
    stopButton.setEnabled(true);
}

void MainComponent::stopButtonClicked()
{
    for (auto& track : tracks)
    {
        track->transportSource.stop();
        track->transportSource.setPosition(0.0);
        track->playPauseButton.setButtonText("Play");
    }
    state = Stopped;
    stopButton.setEnabled(false);
}
