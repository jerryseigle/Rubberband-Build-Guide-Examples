/*
  ==============================================================================

    TimePitchProcessor.cpp
    Created: 13 Apr 2025 1:40:39am
    Author:  Jerry Seigle

    This file implements the TimePitchProcessor class.

    It acts as a wrapper around the RubberBandStretcher for real-time
    pitch shifting and tempo manipulation. This class handles:
    - Feeding input audio into RubberBand
    - Configuring pitch, tempo, and formant options
    - Retrieving processed audio into an output buffer

  ==============================================================================
*/

#include "TimePitchProcessor.h"

//==============================================================================
// Constructor
TimePitchProcessor::TimePitchProcessor() = default;

// Destructor
TimePitchProcessor::~TimePitchProcessor() = default;

//==============================================================================
// Prepares the processor for audio playback by setting up the RubberBandStretcher
void TimePitchProcessor::prepare(double sr, int numChannels)
{
    sampleRate = sr;         // Store sample rate
    channels = numChannels;  // Store channel count

    // Allocate a temporary buffer to hold input audio data
    tempBuffer.setSize(channels, requiredSamples);

    // Create and configure the RubberBandStretcher instance
    stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
        sampleRate,
        channels,
        RubberBand::RubberBandStretcher::OptionProcessRealTime |   // Allows real-time streaming
        RubberBand::RubberBandStretcher::OptionStretchElastic |    // Smooth tempo changes
        RubberBand::RubberBandStretcher::OptionEngineFiner |       // Higher quality stretching
        RubberBand::RubberBandStretcher::OptionPitchHighQuality |  // High-quality pitch shifting
        RubberBand::RubberBandStretcher::OptionWindowLong |        // Long window = smoother sound
        RubberBand::RubberBandStretcher::OptionThreadingNever      // Ensures deterministic behavior
    );

    // Apply current pitch and tempo settings to RubberBand
    stretcher->setPitchScale(std::pow(2.0f, currentPitch / 12.0f)); // Convert semitones to pitch scale
    stretcher->setTimeRatio(currentTempo);                          // Apply tempo ratio

    // Set whether to preserve formants (vocal characteristics)
    stretcher->setFormantOption(formantEnabled
        ? RubberBand::RubberBandStretcher::OptionFormantPreserved
        : RubberBand::RubberBandStretcher::OptionFormantShifted);
}

//==============================================================================
// Sets the pitch shift in semitones
void TimePitchProcessor::setPitchSemiTones(float semitones)
{
    currentPitch = semitones; // Store new pitch

    // If RubberBand is ready, update pitch scale
    if (stretcher)
        stretcher->setPitchScale(std::pow(2.0f, currentPitch / 12.0f)); // Convert to pitch scale factor
}

//==============================================================================
// Sets the playback tempo (1.0 = normal speed)
void TimePitchProcessor::setTempoRatio(float ratio)
{
    currentTempo = ratio; // Store new tempo ratio

    // Update RubberBand with the new tempo
    if (stretcher)
        stretcher->setTimeRatio(currentTempo);
}

//==============================================================================
// Enables or disables formant preservation
void TimePitchProcessor::setFormantEnabled(bool shouldPreserveFormant)
{
    formantEnabled = shouldPreserveFormant; // Save toggle state

    // Update RubberBand with formant setting
    if (stretcher)
        stretcher->setFormantOption(shouldPreserveFormant
            ? RubberBand::RubberBandStretcher::OptionFormantPreserved
            : RubberBand::RubberBandStretcher::OptionFormantShifted);
}

//==============================================================================
// Processes an audio block using RubberBand
//
// Parameters:
// - inputProvider: a lambda that fills a buffer with fresh audio input
// - output: the final output buffer to store processed audio
void TimePitchProcessor::processBlock(std::function<void(juce::AudioBuffer<float>&)> inputProvider,
                                      juce::AudioBuffer<float>& output)
{
    if (!stretcher)
    {
        // If the stretcher is not ready, clear output and exit
        output.clear();
        return;
    }

    const int outSamples = output.getNumSamples(); // Number of samples we need to produce

    // Warm up: feed enough samples to ensure RubberBand can output smoothly
    while (stretcher->available() < outSamples * 2)
    {
        tempBuffer.clear();             // Clear the temporary input buffer
        inputProvider(tempBuffer);      // Ask caller to provide new audio

        // Collect raw pointers for each channel
        std::vector<const float*> inputPointers(channels);
        for (int ch = 0; ch < channels; ++ch)
            inputPointers[ch] = tempBuffer.getReadPointer(ch);

        // Feed the new audio into RubberBand
        stretcher->process(inputPointers.data(), requiredSamples, false);
    }

    // Prepare an array of writable pointers for the output buffer
    std::vector<float*> outputPointers(channels);
    for (int ch = 0; ch < channels; ++ch)
        outputPointers[ch] = output.getWritePointer(ch);

    // Retrieve processed samples from RubberBand into the output buffer
    stretcher->retrieve(outputPointers.data(), outSamples);
}
