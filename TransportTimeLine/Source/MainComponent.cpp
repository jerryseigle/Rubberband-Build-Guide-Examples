#include "MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible(transportLabel);
    transportLabel.setText("Bar: 1  Beat: 1  Time: 0:00.000", juce::dontSendNotification);

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("BPM: 120", juce::dontSendNotification);

    addAndMakeVisible(timeSignatureBox);
    timeSignatureBox.setText("4/4");
    timeSignatureBox.onTextChange = [this] {
        auto text = timeSignatureBox.getText();
        if (text.containsChar('/'))
        {
            auto parts = juce::StringArray::fromTokens(text, "/", "");
            if (parts.size() == 2)
            {
                int top = parts[0].getIntValue();
                int bottom = parts[1].getIntValue();
                if (top > 0 && bottom > 0)
                    beatsPerBar = top;
            }
        }
    };

    addAndMakeVisible(tempoSlider);
    tempoSlider.setRange(0.5, 2.0, 0.01); // ✅ Correct RubberBand range
    tempoSlider.setValue(1.0);             // ✅ Neutral (normal tempo)
    tempoSlider.setTextValueSuffix(" ratio");
    tempoSlider.onValueChange = [this] {
        bpmLabel.setText("BPM: " + juce::String(getAdjustedTempo(), 1), juce::dontSendNotification);
    };

    addAndMakeVisible(waitForBarToggle);
    waitForBarToggle.setButtonText("Quantize to Bar");

    addAndMakeVisible(startTransportButton);
    startTransportButton.setButtonText("Start Timeline");
    startTransportButton.onClick = [this] {
        transportRunning = true;
        lastCallbackTime = juce::Time::getMillisecondCounterHiRes();
    };

    addAndMakeVisible(stopTransportButton);
    stopTransportButton.setButtonText("Stop Timeline");
    stopTransportButton.onClick = [this] {
        transportRunning = false;
        timelineSeconds = 0.0;
        transportLabel.setText("Bar: 1  Beat: 1  Time: 0:00.000", juce::dontSendNotification);
    };

    setSize(450, 300);
    setAudioChannels(0, 2);
    startTimerHz(20);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
}

void MainComponent::releaseResources()
{
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(80);
    transportLabel.setBounds(area.removeFromTop(30));
    bpmLabel.setBounds(area.removeFromTop(30));
    timeSignatureBox.setBounds(area.removeFromTop(30));
    tempoSlider.setBounds(area.removeFromTop(40));
    waitForBarToggle.setBounds(area.removeFromTop(30));
    startTransportButton.setBounds(area.removeFromTop(30));
    stopTransportButton.setBounds(area.removeFromTop(30));
}

void MainComponent::timerCallback()
{
    if (!transportRunning)
        return;

    double now = juce::Time::getMillisecondCounterHiRes();
    if (lastCallbackTime > 0.0)
    {
        double realDelta = (now - lastCallbackTime) / 1000.0;

        double timeRatio = tempoSlider.getValue();
        if (timeRatio <= 0.0)
            timeRatio = 1.0; // Safe fallback

        double adjustedDelta = realDelta * timeRatio; // ✅ Correct: multiply
        timelineSeconds += adjustedDelta;
    }
    lastCallbackTime = now;

    transportLabel.setText(getFormattedTimeline(timelineSeconds, originalTempo), juce::dontSendNotification);
    bpmLabel.setText("BPM: " + juce::String(getAdjustedTempo(), 1), juce::dontSendNotification);
}

double MainComponent::getAdjustedTempo() const
{
    double timeRatio = tempoSlider.getValue();
    if (timeRatio <= 0.0)
        timeRatio = 1.0;

    return originalTempo * timeRatio; // ✅ Correct: multiply
}

juce::String MainComponent::getFormattedTimeline(double seconds, double bpm, int beatsPerBarOverride) const
{
    double secondsPerBeat = 60.0 / bpm;
    double secondsPerBar = secondsPerBeat * beatsPerBar;

    int bar = (int)(seconds / secondsPerBar) + 1;
    int beat = ((int)(seconds / secondsPerBeat) % beatsPerBar) + 1;
    int millis = ((int)(seconds * 1000.0)) % 1000;

    return "Bar: " + juce::String(bar)
         + "  Beat: " + juce::String(beat)
         + "  Time: " + juce::String((int)seconds / 60) + ":"
         + juce::String((int)seconds % 60).paddedLeft('0', 2) + "."
         + juce::String(millis).paddedLeft('0', 3);
}
