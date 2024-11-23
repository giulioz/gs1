/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct VoiceState {
  float NOTE = 0;
  int GATE = 0;
  int GATEOLD = 0;
  int GATENEW = 0;
  int Mode = 0;                           // 0=Norm 1=Perc (ENVELOPE MODE)
  float AT[4] = {2000, 2000, 4400, 4400}; //(Original envelope setting)
  float DT[4] = {2, 2, 1, 1};
  float RT[4] = {100, 100, 100, 100};
  int STATE[4] = {0, 0, 0, 0};
  float EA[4] = {0, 0, 0, 0};  // 16bit
  float EAx[4] = {0, 0, 0, 0}; // 16bit
  float EAo[4] = {0, 0, 0, 0}; // oldaccu
  float RS[4] = {0, 0, 0, 0};
  float RSx[4] = {0, 0, 0, 0};
  int PAI[4];
  int PAE[4];
  int CW[4] = {0, 0, 0, 0};        // C1,C2,M1,M2
  int AMP[4] = {255, 0, 255, 255}; // C1,C2,M1,M2
  int CH1 = 0;
  int CH2 = 0;
  int M1 = 0;
  int M2 = 0;
  int M1old1, M1old2 = 0;
  int M2old1, M2old2 = 0;
  int EG0 = 0;
  int EG1 = 0;
  int EG2 = 0;
  int EG3 = 0;
  float rnd = 0;
  float KNOTE;
  float Velocity;

  int midiNote;
  bool noteOn;
  bool sustaining = false;
};

struct PatchConsts {
  float Ratio[4] = {1, 1, 1, 1};  // C1,C2,M1,M2 Ratios.
  int Detune[4] = {0, 12, 3, 12}; // C1,C2,M1,M2 detune in cents +-16

  // 44(2key) settings. (First two values in array set upper and lower scaling
  // to the rest of values.)
  float C1EC[46] = {0.0, 0.65, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9,
                    0.9, 0.9,  0.9, 1,   1,   1,   1,   1,   1,   1,   1,   1,
                    1,   1,    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                    0.9, 0.9,  0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8};
  float C2EC[46] = {0.0, 0.35, 0.2, 0.3, 0.3, 0.4, 0.5, 0.5, 0.6, 0.6, 0.7, 0.7,
                    0.8, 0.8,  0.9, 0.9, 1,   1,   1,   1,   0.9, 0.9, 0.8, 0.7,
                    0.6, 0.5,  0.4, 0.3, 0.2, 0.2, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                    0.1, 0.1,  0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1};
  float M1EC[46] = {0.0,  0.7,  0.9,  0.9,  0.6,  0.6, 0.6, 0.6, 0.7, 0.7,
                    0.7,  0.8,  0.8,  0.9,  0.9,  1,   0.9, 0.8, 0.8, 0.7,
                    0.7,  0.7,  0.7,  0.6,  0.6,  0.5, 0.5, 0.4, 0.4, 0.3,
                    0.3,  0.2,  0.2,  0.1,  0.1,  0.1, 0.1, 0.1, 0.1, 0.1,
                    0.05, 0.05, 0.05, 0.05, 0.05, 0.05};
  float M2EC[46] = {0.1, 0.55, 0.9, 0.9, 1,   1,    1,    1,   1,   1,
                    1,   1,    1,   0.9, 0.9, 0.8,  0.8,  0.7, 0.7, 0.7,
                    0.6, 0.6,  0.6, 0.5, 0.5, 0.5,  0.5,  0.5, 0.5, 0.4,
                    0.3, 0.2,  0.1, 0.1, 0.1, 0.05, 0.01, 0.0, 0.0, 0.0,
                    0.0, 0.0,  0.0, 0.0, 0.0, 0.0};
  
  float ATE[4] = {2000, 2000, 4400, 4400}; // Scaled to key in perc mode
  float DTE1Scaling = 3;
};

//==============================================================================
/**
 */
class GS1_juceAudioProcessor : public juce::AudioProcessor {
public:
  //==============================================================================
  GS1_juceAudioProcessor();
  ~GS1_juceAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
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
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  //==============================================================================

  VoiceState voiceStates[32];
  int lastVoice = 0;
  int chorusPos = 0;

  juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
      delayA;
  juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
      delayB;
  juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
      delayC;

  int currentPatch = 0;
  PatchConsts patches[2];

  void noteOn(VoiceState &voiceState, float KNOTE, float Velocity);
  int fmGenSample(VoiceState &voiceState);

private:
  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GS1_juceAudioProcessor)
};
