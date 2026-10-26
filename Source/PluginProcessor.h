/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <vector>

//==============================================================================
/**
*/
class PpAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PpAudioProcessor();
    ~PpAudioProcessor() override;

    juce::AudioProcessorValueTreeState parameters;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    // The delay line holds whole processBlock() blocks; "Delay" selects how many
    // blocks back to tap. 512 slots covers the full range of the parameter.
    static constexpr int maxDelayBlocks = 512;
    // One tap for the delay itself plus a pair per spray voice ("Spray" tops out at 3).
    static constexpr int maxDelayTaps = 8;
    // Number of samples the "Spread" smear averages over, in each direction.
    static constexpr int spreadWindow = 10000;

    void loadWavIntoAudioBuffer (const juce::File& wavFile, juce::AudioBuffer<float>& buffer);

    void allocateDelayLine (int numChannels, int numSamples);
    int  writeToDelayLine (const juce::AudioBuffer<float>& source);

    // Denormalised parameter values owned by `parameters`. These are safe to read
    // from the audio thread; the plain float mirrors they replaced were written on
    // the message thread and read here, which is a data race.
    std::atomic<float>* phaseParam     = nullptr;
    std::atomic<float>* phaseMixParam  = nullptr;
    std::atomic<float>* delayMixParam  = nullptr;
    std::atomic<float>* delayParam     = nullptr;
    std::atomic<float>* modeParam      = nullptr;
    std::atomic<float>* sprayParam     = nullptr;
    std::atomic<float>* spreadParam    = nullptr;
    std::atomic<float>* spreadMixParam = nullptr;

    std::vector<juce::AudioBuffer<float>> delayLine;
    int delayCapacity     = 0;   // samples per slot
    int delayWritePos     = 0;   // slot the next block goes into
    int delayBlocksFilled = 0;   // valid slots, capped at maxDelayBlocks

    // Running sum of the dry block, so the Spread averages are O(1) per sample.
    std::vector<double> prefixSum;

    juce::AudioBuffer<float> waveform;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PpAudioProcessor)
};
