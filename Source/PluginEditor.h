/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

//==============================================================================
/**
*/
class PpAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit PpAudioProcessorEditor (PpAudioProcessor&);
    ~PpAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int numSliders = 8;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PpAudioProcessor& audioProcessor;

    std::array<juce::Slider, numSliders> sliders;

    // Declared after the sliders on purpose: members are destroyed in reverse
    // declaration order, so each attachment is torn down (and de-registers itself
    // as a listener) while the slider it points at is still alive.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PpAudioProcessorEditor)
};
