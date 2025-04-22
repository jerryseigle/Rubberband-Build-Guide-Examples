#pragma once

#include <JuceHeader.h>
#include <RubberBandStretcher.h>
#include <functional>

class TimePitchProcessor
{
public:
    TimePitchProcessor();
    ~TimePitchProcessor();

    void prepare(double sampleRate, int numChannels);
    void setPitchSemiTones(float semitones);
    void setTempoRatio(float ratio);
    void setFormantEnabled(bool shouldPreserveFormant);

    // NEW: Accepts a lambda that fills an input buffer
    void processBlock(std::function<void(juce::AudioBuffer<float>&)> inputProvider,
                      juce::AudioBuffer<float>& output);

private:
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;

    double sampleRate = 44100.0;
    int channels = 2;
    int requiredSamples = 512;

    float currentPitch = 0.0f;
    float currentTempo = 1.0f;
    bool formantEnabled = false;

    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> outputBuffer;
};
