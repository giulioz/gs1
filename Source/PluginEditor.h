/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>


//==============================================================================
/**
 */
class GS1_juceAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  GS1_juceAudioProcessorEditor(GS1_juceAudioProcessor &);
  ~GS1_juceAudioProcessorEditor() override;

  //==============================================================================
  void resized() override;

private:
  GS1_juceAudioProcessor &audioProcessor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GS1_juceAudioProcessorEditor)
};
