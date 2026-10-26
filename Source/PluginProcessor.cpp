/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 1", "Phase", juce::NormalisableRange<float>(0.0f, 64.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 2", "Dry/Wet Phase", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 3", "Dry/Wet Delay", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 4", "Delay", juce::NormalisableRange<float>(0.0f, 511.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 5", "Mode", juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 6", "Spray", juce::NormalisableRange<float>(1.0f, 3.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 7", "Spread", juce::NormalisableRange<float>(0.0f, 32.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Slider 8", "Dry/Wet Spread", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    return layout;
}

//==============================================================================
PpAudioProcessor::PpAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    phaseParam     = parameters.getRawParameterValue ("Slider 1");
    phaseMixParam  = parameters.getRawParameterValue ("Slider 2");
    delayMixParam  = parameters.getRawParameterValue ("Slider 3");
    delayParam     = parameters.getRawParameterValue ("Slider 4");
    modeParam      = parameters.getRawParameterValue ("Slider 5");
    sprayParam     = parameters.getRawParameterValue ("Slider 6");
    spreadParam    = parameters.getRawParameterValue ("Slider 7");
    spreadMixParam = parameters.getRawParameterValue ("Slider 8");

    jassert (phaseParam != nullptr && phaseMixParam != nullptr && delayMixParam != nullptr
              && delayParam != nullptr && modeParam != nullptr && sprayParam != nullptr
              && spreadParam != nullptr && spreadMixParam != nullptr);
}

PpAudioProcessor::~PpAudioProcessor()
{
}

//==============================================================================

void PpAudioProcessor::loadWavIntoAudioBuffer (const juce::File& wavFile, juce::AudioBuffer<float>& buffer)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (wavFile));

    if (reader != nullptr)
    {
        buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    }
}

const juce::String PpAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PpAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PpAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PpAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PpAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PpAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PpAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PpAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PpAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PpAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PpAudioProcessor::allocateDelayLine (int numChannels, int numSamples)
{
    delayCapacity = juce::jmax (1, numSamples);

    const juce::AudioBuffer<float> prototype (juce::jmax (1, numChannels), delayCapacity);
    delayLine.assign ((size_t) maxDelayBlocks, prototype);

    for (auto& block : delayLine)
        block.clear();

    delayWritePos     = 0;
    delayBlocksFilled = 0;

    prefixSum.assign ((size_t) delayCapacity + 1, 0.0);
}

int PpAudioProcessor::writeToDelayLine (const juce::AudioBuffer<float>& source)
{
    const int slot = delayWritePos;
    auto& dest = delayLine[(size_t) slot];

    const int numSamples  = juce::jmin (source.getNumSamples(),  dest.getNumSamples());
    const int numChannels = juce::jmin (source.getNumChannels(), dest.getNumChannels());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        dest.copyFrom (ch, 0, source, ch, 0, numSamples);

        // Blank the tail so a later, longer block can never read whatever the
        // previous occupant of this slot left behind.
        dest.clear (ch, numSamples, dest.getNumSamples() - numSamples);
    }

    for (int ch = numChannels; ch < dest.getNumChannels(); ++ch)
        dest.clear (ch, 0, dest.getNumSamples());

    delayWritePos     = (delayWritePos + 1) % maxDelayBlocks;
    delayBlocksFilled = juce::jmin (delayBlocksFilled + 1, maxDelayBlocks);

    return slot;
}

void PpAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate);

    // Allocating here rather than in processBlock() keeps the audio thread free of
    // allocations, and starting from a cleared history stops stale audio from a
    // previous run (or a previous channel layout) being played back.
    allocateDelayLine (juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels()),
                       samplesPerBlock);
}

void PpAudioProcessor::releaseResources()
{
    delayLine.clear();
    delayLine.shrink_to_fit();
    prefixSum.clear();
    prefixSum.shrink_to_fit();

    delayCapacity     = 0;
    delayWritePos     = 0;
    delayBlocksFilled = 0;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PpAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PpAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    // In case we have more outputs than inputs, this clears any output channels
    // that did not contain input data, because these are not guaranteed to be empty.
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (numSamples <= 0 || totalNumInputChannels <= 0)
        return;

    // Read every parameter exactly once, up front. Values can change on the message
    // thread at any moment, so re-reading one mid-block could see a new value land
    // between a bounds check and the use it guards.
    const int   phase     = (int) phaseParam->load();
    const float phaseMix  = phaseMixParam->load();
    const float delayMix  = delayMixParam->load();
    const int   delayTap  = (int) delayParam->load();
    const int   mode      = (int) modeParam->load();
    const int   spray     = (int) sprayParam->load();
    const float spreadMix = spreadMixParam->load();

    // Spread is used as a modulo divisor but its range starts at 0. Dividing by the
    // raw value crashed the host as soon as Dry/Wet Spread was raised off zero, so
    // clamp it here: 0 and 1 both mean "on every sample".
    const int spread = juce::jmax (1, (int) spreadParam->load());

    if ((int) delayLine.size() != maxDelayBlocks
         || delayCapacity < numSamples
         || delayLine.front().getNumChannels() < totalNumInputChannels)
    {
        // Only reachable if the host hands us a bigger block or a wider layout than
        // it announced in prepareToPlay. Reallocating here is not real-time safe, but
        // it is a one-off and it beats reading past the end of a buffer.
        allocateDelayLine (totalNumInputChannels, numSamples);
    }

    // Stash the dry input before anything is written back over it. This has to happen
    // even when the plug-in is fully dry, otherwise the history goes stale and raising
    // a wet control replays a burst of old audio.
    const int currentSlot = writeToDelayLine (buffer);

    if (phaseMix == 0.0f && delayMix == 0.0f && spreadMix == 0.0f)
        return;

    // Work out which slots the delay taps read from. The indices depend on neither the
    // channel nor the sample, so they only need computing once per block; the original
    // recomputed them inside the innermost loop.
    const bool delayActive = delayTap > 0 && delayTap < delayBlocksFilled;

    int tapSlots[maxDelayTaps];
    int numTaps = 0;

    if (delayActive)
    {
        const int history = delayBlocksFilled;

        auto slotForOffset = [currentSlot, history] (int blocksAgo)
        {
            // Wrap with true signed semantics. The original wrote `offset % size`
            // where size is a size_t, which promoted a negative offset to a huge
            // unsigned value and landed on an essentially arbitrary block.
            const int wrapped = ((blocksAgo % history) + history) % history;
            return (currentSlot - wrapped + maxDelayBlocks) % maxDelayBlocks;
        };

        tapSlots[numTaps++] = slotForOffset (delayTap);

        for (int j = 1; j < spray && numTaps + 1 < maxDelayTaps; ++j)
        {
            tapSlots[numTaps++] = slotForOffset (delayTap + spray / j);
            tapSlots[numTaps++] = slotForOffset (delayTap - spray / j);
        }
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // Read the untouched input from the copy just stashed in the delay line. The
        // original read through buffer.getReadPointer() while writing to that same
        // buffer, so every backwards look-up saw already-processed samples and the
        // output depended on the order the samples happened to be visited in.
        const float* dry = delayLine[(size_t) currentSlot].getReadPointer (channel);
        float* writeData = buffer.getWritePointer (channel);

        const float* taps[maxDelayTaps];

        for (int t = 0; t < numTaps; ++t)
            taps[t] = delayLine[(size_t) tapSlots[t]].getReadPointer (channel);

        if (spreadMix > 0.0f)
        {
            // Running sum of the dry block, so each Spread average below costs two
            // lookups instead of up to 10000 adds.
            prefixSum[0] = 0.0;

            for (int i = 0; i < numSamples; ++i)
                prefixSum[(size_t) i + 1] = prefixSum[(size_t) i] + (double) dry[i];
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float sampleValue;

            switch (mode)
            {
                case 1:  sampleValue = dry[(i | phase) % numSamples]; break;
                case 2:  sampleValue = dry[(i ^ phase) % numSamples]; break;
                case 0:
                default: sampleValue = dry[(i & phase) % numSamples]; break;
            }

            sampleValue = (dry[i] * (1.0f - phaseMix)) + (sampleValue * phaseMix);

            for (int t = 0; t < numTaps; ++t)
                sampleValue = (sampleValue * (1.0f - delayMix)) + (taps[t][i] * delayMix);

            if (spreadMix > 0.0f && i % spread == 0)
            {
                const int ahead  = juce::jmin (spreadWindow, numSamples - i);
                const int behind = juce::jmin (spreadWindow, i + 1);

                const auto r  = (float) ((prefixSum[(size_t) (i + ahead)] - prefixSum[(size_t) i]) / ahead);
                const auto rr = (float) ((prefixSum[(size_t) (i + 1)] - prefixSum[(size_t) (i + 1 - behind)]) / behind);

                sampleValue = (sampleValue * (1.0f - spreadMix)) + ((r + rr) * spreadMix);
            }

            writeData[i] = sampleValue;
        }
    }
}

//==============================================================================
bool PpAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PpAudioProcessor::createEditor()
{
    return new PpAudioProcessorEditor (*this);
}

//==============================================================================
void PpAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // copyState() already flushes the live parameter values into the tree, so there
    // is nothing to push by hand. The previous version fed denormalised values (up to
    // 511) into AudioProcessorParameter::setValue(), which expects 0 to 1, so every
    // save clamped the whole patch to its maximum.
    auto state = parameters.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PpAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PpAudioProcessor();
}
