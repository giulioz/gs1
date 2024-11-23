/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"


//==============================================================================
GS1_juceAudioProcessorEditor::GS1_juceAudioProcessorEditor(
    GS1_juceAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  setSize(100, 100);
}

GS1_juceAudioProcessorEditor::~GS1_juceAudioProcessorEditor() {}

void GS1_juceAudioProcessorEditor::resized() {}
