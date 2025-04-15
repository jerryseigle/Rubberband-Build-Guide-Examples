/*
  ==============================================================================

    TimePitchProcessor.h
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <RubberBandStretcher.h>

class TimePitchProcessor
{
public:
    TimePitchProcessor();
    ~TimePitchProcessor();

    void prepare(double sampleRate, int numChannels);
    void setPitchSemiTones(float semitones);
    void setTempoRatio(float ratio);
    void setFormantEnabled(bool shouldPreserveFormant); // ✅ New method

    void processBlock(juce::AudioSource* source, juce::AudioBuffer<float>& buffer);

private:
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;

    double sampleRate = 44100.0;
    int channels = 2;
    int requiredSamples = 512;

    float currentPitch = 0.0f;
    float currentTempo = 1.0f;
    bool formantEnabled = false; // ✅ New state flag

    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> outputBuffer;
};

