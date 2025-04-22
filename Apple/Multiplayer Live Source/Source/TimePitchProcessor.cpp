#include "TimePitchProcessor.h"

TimePitchProcessor::TimePitchProcessor() = default;
TimePitchProcessor::~TimePitchProcessor() = default;

void TimePitchProcessor::prepare(double sr, int numChannels)
{
    sampleRate = sr;
    channels = numChannels;

    tempBuffer.setSize(channels, requiredSamples);
    outputBuffer.setSize(channels, requiredSamples);

    stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
        sampleRate,
        channels,
        RubberBand::RubberBandStretcher::OptionProcessRealTime |
        RubberBand::RubberBandStretcher::OptionStretchElastic |
        RubberBand::RubberBandStretcher::OptionEngineFiner |
        RubberBand::RubberBandStretcher::OptionPitchHighQuality |
        RubberBand::RubberBandStretcher::OptionWindowLong
    );
    
    stretcher->setPitchScale(std::pow(2.0f, currentPitch / 12.0f));
    stretcher->setTimeRatio(currentTempo);
    stretcher->setFormantOption(formantEnabled
        ? RubberBand::RubberBandStretcher::OptionFormantPreserved
        : RubberBand::RubberBandStretcher::OptionFormantShifted);
}

void TimePitchProcessor::setPitchSemiTones(float semitones)
{
    currentPitch = semitones;
    if (stretcher)
        stretcher->setPitchScale(std::pow(2.0f, currentPitch / 12.0f));
}

void TimePitchProcessor::setTempoRatio(float ratio)
{
    currentTempo = ratio;
    if (stretcher)
        stretcher->setTimeRatio(currentTempo);
}

void TimePitchProcessor::setFormantEnabled(bool shouldPreserveFormant)
{
    formantEnabled = shouldPreserveFormant;
    if (stretcher)
        stretcher->setFormantOption(shouldPreserveFormant
            ? RubberBand::RubberBandStretcher::OptionFormantPreserved
            : RubberBand::RubberBandStretcher::OptionFormantShifted);
}

void TimePitchProcessor::processBlock(std::function<void(juce::AudioBuffer<float>&)> inputProvider,
                                      juce::AudioBuffer<float>& output)
{
    if (!stretcher)
    {
        output.clear();
        return;
    }

    const int outSamples = output.getNumSamples();
    int available = stretcher->available();

    // Feed RubberBand until enough output is available
    while (available < outSamples)
    {
        tempBuffer.clear();
        inputProvider(tempBuffer);

        std::vector<const float*> inputPointers(channels);
        for (int ch = 0; ch < channels; ++ch)
            inputPointers[ch] = tempBuffer.getReadPointer(ch);

        stretcher->process(inputPointers.data(), requiredSamples, false);
        available = stretcher->available();
    }

    std::vector<float*> outputPointers(channels);
    for (int ch = 0; ch < channels; ++ch)
        outputPointers[ch] = output.getWritePointer(ch);

    stretcher->retrieve(outputPointers.data(), outSamples);
}
