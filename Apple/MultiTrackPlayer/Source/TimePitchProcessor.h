/*
  ==============================================================================

    TimePitchProcessor.h
    Created: 13 Apr 2025 1:40:22am
    Author:  Jerry Seigle

    This file defines the TimePitchProcessor class.

    This class wraps the RubberBandStretcher library and provides real-time
    pitch and tempo processing for audio. It allows external components to:
    - Apply semitone pitch shifting
    - Adjust playback tempo
    - Enable or disable formant preservation
    - Feed and retrieve processed audio blocks using callbacks

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <RubberBandStretcher.h>
#include <functional> // For std::function

//==============================================================================
// TimePitchProcessor
//
// Wraps the RubberBandStretcher and provides a simplified interface for
// pitch and tempo manipulation in real-time. Accepts audio via callbacks
// and returns the processed output.
//
// This class is designed to be used in real-time audio rendering threads.
//==============================================================================
class TimePitchProcessor
{
public:
    // Constructor and destructor
    TimePitchProcessor();
    ~TimePitchProcessor();

    //==========================================================================
    // Prepares the processor for playback
    // - sampleRate: the sample rate of the audio system
    // - numChannels: number of audio channels (typically 2 for stereo)
    void prepare(double sampleRate, int numChannels);

    //==========================================================================
    // Sets the pitch shift in semitones (e.g., -5 to +7 semitones)
    // Converts semitones into pitch scale for RubberBand
    void setPitchSemiTones(float semitones);

    //==========================================================================
    // Sets the playback tempo ratio
    // - 1.0 = normal speed
    // - < 1.0 = slower
    // - > 1.0 = faster
    void setTempoRatio(float ratio);

    //==========================================================================
    // Enables or disables formant preservation
    // - When enabled, vocal character is preserved during pitch shift
    void setFormantEnabled(bool shouldPreserveFormant);

    //==========================================================================
    // Processes audio in real-time
    // - inputProvider: a lambda that fills a temporary buffer with audio
    // - output: the buffer to fill with processed audio
    //
    // The processor will:
    // - Request audio from the lambda
    // - Feed it into RubberBand
    // - Retrieve and store the processed output
    void processBlock(std::function<void(juce::AudioBuffer<float>&)> inputProvider,
                      juce::AudioBuffer<float>& output);

private:
    // Smart pointer to RubberBandStretcher instance
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;

    // Internal audio configuration
    double sampleRate = 44100.0;   // Default sample rate
    int channels = 2;              // Default to stereo
    int requiredSamples = 256;     // Number of samples per block (feeding RubberBand)

    // Internal state
    float currentPitch = 0.0f;     // Pitch in semitones
    float currentTempo = 1.0f;     // Tempo ratio (1.0 = normal)
    bool formantEnabled = false;  // Whether formants should be preserved

    // Temporary buffer to store input before sending to RubberBand
    juce::AudioBuffer<float> tempBuffer;
};
