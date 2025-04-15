/*
  ==============================================================================
    TimePitchProcessor.cpp
    Created: 13 Apr 2025 1:40:39am
    Author:  Jerry Seigle

    This class handles real-time pitch and tempo adjustment using RubberBand.
    It is called from MainComponent and processes audio blocks in sync with playback.
  ==============================================================================
*/

#include "TimePitchProcessor.h"

TimePitchProcessor::TimePitchProcessor() = default;
TimePitchProcessor::~TimePitchProcessor() = default;

/**
 * Called when audio is ready to be processed.
 * Initializes RubberBandStretcher with options.
 */
void TimePitchProcessor::prepare(double sr, int numChannels)
{
    sampleRate = sr;
    channels = numChannels;

    // Temporary input and output buffers (for resizing later)
    tempBuffer.setSize(channels, 0);
    outputBuffer.setSize(channels, 0);

    // === RubberBandStretcher Setup ===
    // This object does the actual pitch and time processing.
    // We give it sample rate, number of channels, and performance/quality options.
    stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
        sampleRate,
        channels,
        RubberBand::RubberBandStretcher::OptionProcessRealTime |     // Allows streaming processing
        RubberBand::RubberBandStretcher::OptionStretchElastic |      // Flexible speed changes
        RubberBand::RubberBandStretcher::OptionEngineFiner |         // High-quality stretching engine
        RubberBand::RubberBandStretcher::OptionPitchHighQuality |    // Use better pitch shifting
        RubberBand::RubberBandStretcher::OptionWindowLong            // Long window for smoother results
    );

    // Apply initial pitch shift (in semitones)
    stretcher->setPitchScale(std::pow(2.0f, currentPitch / 12.0f));

    // Set initial tempo ratio
    stretcher->setTimeRatio(currentTempo);

    // Formant option (voice character): preserve or shift depending on checkbox
    stretcher->setFormantOption(formantEnabled
        ? RubberBand::RubberBandStretcher::OptionFormantPreserved
        : RubberBand::RubberBandStretcher::OptionFormantShifted);
}

/**
 * Called when the pitch slider changes.
 * Converts semitones to a scale ratio and applies it.
 */
void TimePitchProcessor::setPitchSemiTones(float semitones)
{
    currentPitch = semitones;
    if (stretcher)
        stretcher->setPitchScale(std::pow(2.0f, currentPitch / 12.0f));
}

/**
 * Called when the tempo slider changes.
 * Sets the new playback speed (1.0 = normal speed).
 */
void TimePitchProcessor::setTempoRatio(float ratio)
{
    currentTempo = ratio;
    if (stretcher)
        stretcher->setTimeRatio(currentTempo);
}

/**
 * Called when the formant checkbox is toggled.
 * Tells RubberBand whether or not to preserve voice characteristics.
 */
void TimePitchProcessor::setFormantEnabled(bool shouldPreserveFormant)
{
    formantEnabled = shouldPreserveFormant;
    if (stretcher)
        stretcher->setFormantOption(shouldPreserveFormant
            ? RubberBand::RubberBandStretcher::OptionFormantPreserved
            : RubberBand::RubberBandStretcher::OptionFormantShifted);
}

/**
 * This is where real-time audio processing happens.
 * Reads audio from the source, processes it, and writes the result into the buffer.
 */
void TimePitchProcessor::processBlock(juce::AudioSource* source, juce::AudioBuffer<float>& buffer)
{
    if (!stretcher || !source)
    {
        buffer.clear();
        return;
    }

    const int outSamples = buffer.getNumSamples();

    // Create an input buffer with enough samples to feed RubberBand
    juce::AudioBuffer<float> inBlock;
    inBlock.setSize(channels, requiredSamples);

    int available = stretcher->available();

    // Feed RubberBand enough samples to produce the needed output
    while (available < outSamples)
    {
        // Ask the source for the next raw audio block
        juce::AudioSourceChannelInfo info(&inBlock, 0, requiredSamples);
        source->getNextAudioBlock(info);

        // Create input array for RubberBand (float* per channel)
        std::vector<const float*> inputPointers(channels);
        for (int ch = 0; ch < channels; ++ch)
            inputPointers[ch] = inBlock.getReadPointer(ch);

        // Feed input into the stretcher
        stretcher->process(inputPointers.data(), requiredSamples, false);

        // Update how many samples are now available
        available = stretcher->available();
    }

    // Prepare output pointers
    std::vector<float*> outputPointers(channels);
    for (int ch = 0; ch < channels; ++ch)
        outputPointers[ch] = buffer.getWritePointer(ch);

    // Write processed output into the buffer
    stretcher->retrieve(outputPointers.data(), outSamples);
}

