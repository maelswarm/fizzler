/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <iterator>

namespace
{
    struct SliderSpec
    {
        const char* parameterID;
        const char* suffix;
    };

    // Listed in the order they appear on screen, left to right.
    const SliderSpec sliderSpecs[] =
    {
        { "Slider 1", " Phase" },
        { "Slider 2", " Dry/Wet Phase" },
        { "Slider 5", " Mode" },
        { "Slider 6", " Spray" },
        { "Slider 4", " Delay" },
        { "Slider 3", " Dry/Wet Delay" },
        { "Slider 7", " Spread" },
        { "Slider 8", " Dry/Wet Spread" }
    };

    constexpr int sliderX      = 50;
    constexpr int sliderY      = 50;
    constexpr int sliderWidth  = 50;
    constexpr int sliderHeight = 200;
    constexpr int sliderPitch  = 100;
}

//==============================================================================
PpAudioProcessorEditor::PpAudioProcessorEditor (PpAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    static_assert ((int) std::size (sliderSpecs) == numSliders,
                   "sliderSpecs must describe every slider");

    // Make sure that before the constructor has finished, you have set the
    // editor's size to whatever you need it to be.
    setSize (850, 300);

    for (int i = 0; i < numSliders; ++i)
    {
        auto& slider = sliders[(size_t) i];

        slider.setSliderStyle (juce::Slider::LinearBarVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0);
        slider.setPopupDisplayEnabled (true, false, this);
        slider.setTextValueSuffix (sliderSpecs[i].suffix);

        addAndMakeVisible (slider);

        // The attachment owns the two-way link with the parameter: it takes the
        // range and the initial value from the parameter, pushes user edits back
        // with proper begin/end gestures, and follows host automation. The old
        // hand-rolled Slider::Listener duplicated all of that, wrote straight into
        // the ValueTree behind the attachment's back, and could re-enter itself.
        attachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                audioProcessor.parameters, sliderSpecs[i].parameterID, slider));
    }

    resized();
}

PpAudioProcessorEditor::~PpAudioProcessorEditor()
{
}

//==============================================================================
void PpAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Fizzler", 0, 0, getWidth(), 30, juce::Justification::centred, 1);
}

void PpAudioProcessorEditor::resized()
{
    for (int i = 0; i < numSliders; ++i)
        sliders[(size_t) i].setBounds (sliderX + i * sliderPitch, sliderY, sliderWidth, sliderHeight);
}
