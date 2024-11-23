/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// Constants
static int SampleRate = 34687; // 34687Hz Samplerate (IMPORTANT!)
// Note! Sinewave phase resolution has to be 10bit for quantization error to
// give right sound!

static int DTE[] = {2, 2, 1, 1};
static int RTE[] = {100, 100, 100, 100};
static int IL[] = {0, 0, 0, 0};
static int SL[] = {0, 0, 0, 0}; // 8bit sustain level
static int FMmode[] = {0, 0};   // FM mode CH1 and CH2

static double PI = 3.1415927;
static double HALF_PI = 1.5707964;

// Tables
static int Sin[4096];
static int logsinTable[256];
static int expTable[256];
static int expTable2[4096];

static double map(double x, double in_min, double in_max, double out_min,
                  double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static int lookupSin(int val) {
  bool signsin = (bool)(val & 512);
  bool mirrorsin = (bool)(val & 256);
  val &= 255;
  int result = logsinTable[mirrorsin ? val ^ 255 : val ^ 0];
  if (signsin) {
    result |= 0x8000;
  }
  return result;
}

static int lookupExp(int val) {
  bool signexp = (bool)(val & 0x8000);
  int t = (expTable[(val & 255) ^ 255] | (32768)) << 1;
  int result = t >> ((val & 0x7F00) >> 8);
  if (signexp) {
    result = -result - 1;
  }
  return result >> 4;
}

void GS1_juceAudioProcessor::noteOn(VoiceState &voiceState, float KNOTE,
                                    float Velocity) {
  PatchConsts patch = patches[currentPatch];

  voiceState.noteOn = true;
  voiceState.midiNote = KNOTE;

  voiceState.KNOTE = KNOTE;
  voiceState.Velocity = Velocity;
  for (int i = 0; i < 4; i++) {
    voiceState.STATE[i] = 0;
    voiceState.EA[i] = 0;  // 16bit
    voiceState.EAx[i] = 0; // 16bit
    voiceState.EAo[i] = 0; // oldaccu
    voiceState.RS[i] = 0;
    voiceState.RSx[i] = 0;
    voiceState.GATE = 0;
    voiceState.GATEOLD = 0;
    voiceState.Mode = 0;
  }
  for (int i = 0; i < 4; i++) {
    voiceState.PAI[i] = 0;
    voiceState.PAE[i] = 0;
  }
  voiceState.KNOTE = voiceState.KNOTE - 1;
  voiceState.NOTE = 27.50 * pow(2, (voiceState.KNOTE / (12))); // FROM A1
  voiceState.rnd = 0;

  // Calculate phase accumulator control words.
  voiceState.CW[0] = int(
      pow(2, 28) / (SampleRate / (27.50 *
                                  pow(2, (voiceState.KNOTE + voiceState.rnd +
                                          (patch.Detune[0] * 0.01)) /
                                             (12)) *
                                  patch.Ratio[0])));
  voiceState.CW[1] = int(
      pow(2, 28) / (SampleRate / (27.50 *
                                  pow(2, (voiceState.KNOTE + voiceState.rnd +
                                          (patch.Detune[1] * 0.01)) /
                                             (12)) *
                                  patch.Ratio[1])));
  voiceState.CW[2] = int(
      pow(2, 28) / (SampleRate / (27.50 *
                                  pow(2, (voiceState.KNOTE + voiceState.rnd +
                                          (patch.Detune[2] * 0.01)) /
                                             (12)) *
                                  patch.Ratio[2])));
  voiceState.CW[3] = int(
      pow(2, 28) / (SampleRate / (27.50 *
                                  pow(2, (voiceState.KNOTE + voiceState.rnd +
                                          (patch.Detune[3] * 0.01)) /
                                             (12)) *
                                  patch.Ratio[3])));

  // in perc mode recalc envelope times depending on note.
  voiceState.AT[0] = patch.ATE[0] * map((voiceState.KNOTE + 1), 1, 88, 1, 4);
  voiceState.AT[1] = patch.ATE[1] * map((voiceState.KNOTE + 1), 1, 88, 1, 4);
  voiceState.AT[2] = patch.ATE[2] * map((voiceState.KNOTE + 1), 1, 88, 1, 4);
  voiceState.AT[3] = patch.ATE[3] * map((voiceState.KNOTE + 1), 1, 88, 1, 4);
  voiceState.DT[0] = DTE[0] * map((voiceState.KNOTE + 1), 1, 88, 0.5, 3);
  voiceState.DT[1] =
      DTE[1] * map((voiceState.KNOTE + 1), 1, 88, 0.5, patch.DTE1Scaling);
  voiceState.DT[2] = DTE[2] * map((voiceState.KNOTE + 1), 1, 88, 0.5, 3);
  voiceState.DT[3] = DTE[3] * map((voiceState.KNOTE + 1), 1, 88, 0.5, 3);
  voiceState.RT[0] = RTE[0] * map((voiceState.KNOTE + 1), 1, 88, 1, 2);
  voiceState.RT[1] = RTE[1] * map((voiceState.KNOTE + 1), 1, 88, 1, 2);
  voiceState.RT[2] = RTE[2] * map((voiceState.KNOTE + 1), 1, 88, 1, 2);
  voiceState.RT[3] = RTE[3] * map((voiceState.KNOTE + 1), 1, 88, 1, 2);

  // calc operator volume scaler depending on note index in scaler array.
  voiceState.EG2 =
      floor(map(pow(map(1, 0, 1, patch.M1EC[0],
                        map(patch.M1EC[int(floor(voiceState.KNOTE / 2) + 2)], 0,
                            1, patch.M1EC[0], patch.M1EC[1])),
                    0.1),
                0, 1, 1, 0) *
            4095);
  voiceState.EG1 =
      floor(map(pow(map(1, 0, 1, patch.C2EC[0],
                        map(patch.C2EC[int(floor(voiceState.KNOTE / 2) + 2)], 0,
                            1, patch.C2EC[0], patch.C2EC[1])),
                    0.1),
                0, 1, 1, 0) *
            4095);
  voiceState.EG3 =
      floor(map(pow(map(1, 0, 1, patch.M2EC[0],
                        map(patch.M2EC[int(floor(voiceState.KNOTE / 2) + 2)], 0,
                            1, patch.M2EC[0], patch.M2EC[1])),
                    0.1),
                0, 1, 1, 0) *
            4095);
  voiceState.EG0 =
      floor(map(pow(map(1, 0, 1, patch.C1EC[0],
                        map(patch.C1EC[int(floor(voiceState.KNOTE / 2) + 2)], 0,
                            1, patch.C1EC[0], patch.C1EC[1])),
                    0.1),
                0, 1, 1, 0) *
            4095);

  voiceState.CH1 = 0;
  voiceState.CH2 = 0;
  voiceState.M1 = 0;
  voiceState.M2 = 0;
  voiceState.M1old1 = 0;
  voiceState.M1old2 = 0;
  voiceState.M2old1 = 0;
  voiceState.M2old2 = 0;

  voiceState.GATENEW = 1;
}

int GS1_juceAudioProcessor::fmGenSample(VoiceState &voiceState) {
  for (int e = 0; e < 4; e++) {
    if (voiceState.Mode == 0) {
      if (voiceState.GATEOLD == 0 && voiceState.GATE == 1) {
        voiceState.STATE[e] = 1;
        voiceState.EA[e] = IL[e] << 12;
      }
      if (voiceState.GATE == 1 && voiceState.STATE[e] == 1 &&
          voiceState.EA[e] < 0xFFFFF) {
        voiceState.EA[e] = voiceState.EA[e] + voiceState.AT[e];
        voiceState.EAx[e] = voiceState.EA[e];
        if (voiceState.EA[e] > 0xFFFFF) {
          voiceState.EA[e] = 0xFFFFF;
        }
      }
      if (voiceState.GATE == 1 && voiceState.STATE[e] == 1 &&
          voiceState.EA[e] >= 0xFFFFF) {
        voiceState.STATE[e] = 2;
      }
      if (voiceState.GATE == 1 && voiceState.STATE[e] == 2 &&
          voiceState.EA[e] > SL[e] << 12) {
        voiceState.EA[e] = voiceState.EA[e] - voiceState.DT[e];
        if (voiceState.EA[e] < SL[e] << 12) {
          voiceState.EA[e] = SL[e] << 12;
        }
        voiceState.EAx[e] =
            int(map(expTable2[int(
                        map(voiceState.EA[e], SL[e] << 12, 0xFFFFF, 0, 4095))],
                    0, 4095, SL[e] << 12, 0xFFFFF));
      }
      if (voiceState.GATE == 0 && voiceState.GATEOLD == 1) {
        voiceState.RS[e] = voiceState.EA[e];
        voiceState.RSx[e] = voiceState.EAx[e];
        voiceState.RT[e] = voiceState.RT[e];
      }
      if (voiceState.GATE == 0 && voiceState.EA[e] > 0) {
        voiceState.EA[e] = voiceState.EA[e] - voiceState.RT[e];
        voiceState.STATE[e] = 0;
        if (voiceState.EA[e] <= 0) {
          voiceState.EA[e] = 0;
        }
        voiceState.EAx[e] =
            int(map(expTable2[int(map(voiceState.EA[e], 0,
                                      floor(voiceState.RS[e]), 0, 4095))],
                    0, 4095, 0, voiceState.RSx[e]));
      }
      if (voiceState.GATE == 0 &&
          voiceState.EAo[e] < (int(voiceState.EA[e]) & 0xFFFFF)) {
        voiceState.EA[e] = 0;
      }
      voiceState.EAo[e] = voiceState.EA[e];
    }
  }
  voiceState.GATEOLD = voiceState.GATE;
  voiceState.GATE = voiceState.GATENEW;

  // Operator volume is calculated from scaled volume and velocity.
  voiceState.AMP[2] = ((((((int)floor(voiceState.EAx[2]) >> 8) ^ 4095) & 4095) +
                        int(voiceState.EG2))
                       << 0) +
                      (int(voiceState.Velocity) << 3);
  voiceState.AMP[1] = ((((((int)floor(voiceState.EAx[1]) >> 8) ^ 4095) & 4095) +
                        int(voiceState.EG1))
                       << 0) +
                      (int(voiceState.Velocity) << 3);
  voiceState.AMP[3] = ((((((int)floor(voiceState.EAx[3]) >> 8) ^ 4095) & 4095) +
                        int(voiceState.EG3))
                       << 0) +
                      (int(voiceState.Velocity) << 3);
  voiceState.AMP[0] = ((((((int)floor(voiceState.EAx[0]) >> 8) ^ 4095) & 4095) +
                        int(voiceState.EG0))
                       << 0) +
                      (int(voiceState.Velocity) << 3);
  for (int check = 0; check < 4;
       check++) { // Make sure amplitude does not go out of range.
    if (voiceState.AMP[check] >= 4095) {
      voiceState.AMP[check] = 4095;
    }
  }
  // Update all Phase accumulators..(28bit)
  for (int n = 0; n < 4; n++) {
    voiceState.PAI[n] = voiceState.PAI[n] & 0xFFFFFFF;
    // Shift down externally used accu value.
    voiceState.PAE[n] = voiceState.PAI[n] >> 18;
    voiceState.PAI[n] = voiceState.PAI[n] + voiceState.CW[n];
  }

  // Following section is routing and operator stack config. 4 modes: norm,
  // pi/2: half intensity modulator self feedback, pi full mod self feedback and
  // cross from other stack. CHANNEL1
  if (FMmode[0] == 0) { // NORM
    if (voiceState.AMP[2] <= 4094) {
      voiceState.M1 =
          ((lookupExp(lookupSin(voiceState.PAE[2]) + voiceState.AMP[2]) +
            8192) >>
           2) &
          1023;
    } else {
      voiceState.M1 = 4095;
    }
    if (voiceState.AMP[0] <= 4094) {
      voiceState.CH1 =
          ((lookupExp(lookupSin(voiceState.PAE[0] + voiceState.M1) +
                      voiceState.AMP[0]))); // 14bit output signed+-
    } else {
      voiceState.CH1 = 0;
    }
  }
  if (FMmode[0] == 1) { // PI/2
    if (voiceState.AMP[2] <= 4094) {
      voiceState.M1 =
          (lookupExp(
               lookupSin(voiceState.PAE[2] +
                         (((voiceState.M1old1 + voiceState.M1old2) / 2) >> 3)) +
               voiceState.AMP[2]) +
           8192) >>
          4;
    } else {
      voiceState.M1 = 4095;
    }
    voiceState.M1old2 = voiceState.M1old1;
    voiceState.M1old1 = voiceState.M1;
    if (voiceState.AMP[0] <= 4094) {
      voiceState.CH1 =
          ((lookupExp(lookupSin(voiceState.PAE[0] + voiceState.M1) +
                      voiceState.AMP[0]))); // 14bit output signed+-
    } else {
      voiceState.CH1 = 0;
    }
  }
  if (FMmode[0] == 2) { // PI
    if (voiceState.AMP[2] <= 4094) {
      voiceState.M1 =
          (lookupExp(
               lookupSin(voiceState.PAE[2] +
                         (((voiceState.M1old1 + voiceState.M1old2) / 2) >> 2)) +
               voiceState.AMP[2]) +
           8192) >>
          4;
    } else {
      voiceState.M1 = 4095;
    }
    voiceState.M1old2 = voiceState.M1old1;
    voiceState.M1old1 = voiceState.M1;
    if (voiceState.AMP[0] <= 4094) {
      voiceState.CH1 =
          ((lookupExp(lookupSin(voiceState.PAE[0] + voiceState.M1) +
                      voiceState.AMP[0]))); // 14bit output signed+-
    } else {
      voiceState.CH1 = 0;
    }
  }
  if (FMmode[0] == 3) { // CROSS
    if (voiceState.AMP[2] <= 4094) {
      voiceState.M1 = ((lookupExp(lookupSin(voiceState.PAE[2] + voiceState.M2) +
                                  voiceState.AMP[2]) +
                        8192) >>
                       2) &
                      1023; // USE OPPOSITE MOD!
    } else {
      voiceState.M1 = 4095;
    }
    if (voiceState.AMP[0] <= 4094) {
      voiceState.CH1 = ((lookupExp(
          lookupSin(voiceState.PAE[0] + voiceState.M1) + voiceState.AMP[0])));
    } else {
      voiceState.CH1 = 0;
    }
  }
  // CHANNEL2
  if (FMmode[1] == 0) { // NORM
    if (voiceState.AMP[3] <= 4094) {
      voiceState.M2 =
          ((lookupExp(lookupSin(voiceState.PAE[3]) + voiceState.AMP[3]) +
            8192) >>
           2) &
          1023;
    } else {
      voiceState.M2 = 4095;
    }
    if (voiceState.AMP[1] <= 4094) {
      voiceState.CH2 =
          ((lookupExp(lookupSin(voiceState.PAE[1] + voiceState.M2) +
                      voiceState.AMP[1]))); // 14bit output signed+-
    } else {
      voiceState.CH2 = 0;
    }
  }
  if (FMmode[1] == 1) { // PI/2
    if (voiceState.AMP[1] <= 4094) {
      voiceState.M2 =
          (lookupExp(
               lookupSin(voiceState.PAE[3] +
                         (((voiceState.M2old1 + voiceState.M2old2) / 2) >> 3)) +
               voiceState.AMP[3]) +
           8192) >>
          4;
    } else {
      voiceState.M2 = 4095;
    }
    voiceState.M2old2 = voiceState.M2old1;
    voiceState.M2old1 = voiceState.M2;
    if (voiceState.AMP[1] <= 4094) {
      voiceState.CH2 =
          ((lookupExp(lookupSin(voiceState.PAE[1] + voiceState.M1) +
                      voiceState.AMP[1]))); // 14bit output signed+-
    } else {
      voiceState.CH2 = 0;
    }
  }
  if (FMmode[1] == 2) { // PI
    if (voiceState.AMP[3] <= 4094) {
      voiceState.M2 =
          (lookupExp(
               lookupSin(voiceState.PAE[3] +
                         (((voiceState.M2old1 + voiceState.M2old2) / 2) >> 2)) +
               voiceState.AMP[3]) +
           8192) >>
          4;
    } else {
      voiceState.M2 = 4095;
    }
    voiceState.M2old2 = voiceState.M2old1;
    voiceState.M2old1 = voiceState.M2;
    if (voiceState.AMP[1] <= 4094) {
      voiceState.CH2 =
          ((lookupExp(lookupSin(voiceState.PAE[1] + voiceState.M1) +
                      voiceState.AMP[1]))); // 14bit output signed+-
    } else {
      voiceState.CH2 = 0;
    }
  }
  if (FMmode[1] == 3) { // CROSSMOD
    if (voiceState.AMP[3] <= 4094) {
      voiceState.M2 = ((lookupExp(lookupSin(voiceState.PAE[3] + voiceState.M1) +
                                  voiceState.AMP[3]) +
                        8192) >>
                       2) &
                      1023; // USE OPPOSITE MOD!
    } else {
      voiceState.M2 = 4095;
    }
    if (voiceState.AMP[1] <= 4094) {
      voiceState.CH2 = ((lookupExp(
          lookupSin(voiceState.PAE[1] + voiceState.M2) + voiceState.AMP[1])));
    } else {
      voiceState.CH2 = 0;
    }
  }
  return voiceState.CH1 +
         voiceState.CH2; // Mix two stacks output and send to wave array
}

float cubicInterpolation(float v0, float v1, float v2, float v3, float t) {
  float a0, a1, a2, a3;

  a0 = v3 - v2 - v0 + v1;
  a1 = v0 - v1 - a0;
  a2 = v2 - v0;
  a3 = v1;

  return (a0 * (t * t * t)) + (a1 * (t * t)) + (a2 * t) + a3;
}

//==============================================================================
GS1_juceAudioProcessor::GS1_juceAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
  for (int i = 0; i < 256; ++i) {
    logsinTable[i] =
        (round(-(log(sin(ceil(i + 0.5) * PI / 256 / 2)) / log(2)) * 256.0));
    expTable[i] = (round((pow(2, i / 256.0) - 1) * 32768));
  }
  for (int i = 0; i < 4096; i++) {
    Sin[i] = int(map(sin(map(i, 0, 4095, 0, HALF_PI)), 0, 1, 0, 5782));
  }
  for (int i = 0; i < 4096; i++) {
    expTable2[i] = i;
  }

  float Ratio[] = {1, 7, 1, 15}; // C1,C2,M1,M2 Ratios.
  int Detune[] = {0, 0, 5, 0};   // C1,C2,M1,M2 detune in cents +-16
  float C1EC[] = {
      0.0, 1,   0.5, 0.5, 0.5, 0.5, 0.6, 0.6, 0.7, 0.7, 0.8, 0.8,
      0.9, 0.9, 0.9, 1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   0.9, 0.9, 0.9,
      0.8, 0.8, 0.7, 0.7, 0.6, 0.6, 0.5, 0.5, 0.4, 0.4}; // First lower and
                                                         // upper limit(max amp
                                                         // for velocity) then
                                                         // 44(2key) settings.
  float C2EC[] = {0.0, 0.06, 0.6,  0.6,  0.6,  0.6, 0.6, 0.6, 0.7, 0.7,
                  0.7, 0.7,  0.7,  0.7,  0.6,  0.6, 0.5, 0.5, 0.4, 0.4,
                  0.4, 0.4,  0.4,  0.4,  0.4,  0.4, 0.4, 0.3, 0.3, 0.3,
                  0.3, 0.3,  0.2,  0.2,  0.2,  0.2, 0.1, 0.1, 0.1, 0.1,
                  0.1, 0.05, 0.05, 0.05, 0.05, 0.05};
  float M1EC[] = {0.0,  0.27, 1,    1,    1,    1,    1,    1,    0.9,  0.9,
                  0.8,  0.8,  0.7,  0.7,  0.6,  0.6,  0.6,  0.6,  0.6,  0.6,
                  0.6,  0.5,  0.5,  0.5,  0.4,  0.4,  0.3,  0.3,  0.3,  0.2,
                  0.2,  0.1,  0.1,  0.1,  0.05, 0.05, 0.05, 0.01, 0.01, 0.01,
                  0.01, 0.01, 0.01, 0.01, 0.01, 0.01};
  float M2EC[] = {0.0, 0.3, 1,   1,   1,   1,   1,   0.9,  0.9, 0.8, 0.8, 0.7,
                  0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.0, 0.0, 0.0, 0.0,
                  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,
                  0,   0,   0,   0,   0,   0,   0,   0,    0,   0};
  float ATE[] = {2000, 2000, 1800, 3000};
  memcpy(patches[1].Ratio, Ratio, sizeof(Ratio));
  memcpy(patches[1].Detune, Detune, sizeof(Detune));
  memcpy(patches[1].C1EC, C1EC, sizeof(C1EC));
  memcpy(patches[1].C2EC, C2EC, sizeof(C2EC));
  memcpy(patches[1].M1EC, M1EC, sizeof(M1EC));
  memcpy(patches[1].M2EC, M2EC, sizeof(M2EC));
  memcpy(patches[1].ATE, ATE, sizeof(ATE));
  patches[1].DTE1Scaling = 15;
}

GS1_juceAudioProcessor::~GS1_juceAudioProcessor() {}

//==============================================================================
const juce::String GS1_juceAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool GS1_juceAudioProcessor::acceptsMidi() const { return true; }

bool GS1_juceAudioProcessor::producesMidi() const { return false; }

bool GS1_juceAudioProcessor::isMidiEffect() const { return false; }

double GS1_juceAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int GS1_juceAudioProcessor::getNumPrograms() { return 2; }

int GS1_juceAudioProcessor::getCurrentProgram() { return currentPatch; }

void GS1_juceAudioProcessor::setCurrentProgram(int index) {
  currentPatch = index;
}

const juce::String GS1_juceAudioProcessor::getProgramName(int index) {
  if (index == 0) {
    return "EP11";
  } else if (index == 1) {
    return "EP22";
  }
  return {};
}

void GS1_juceAudioProcessor::changeProgramName(int index,
                                               const juce::String &newName) {}

//==============================================================================
void GS1_juceAudioProcessor::prepareToPlay(double sampleRate,
                                           int samplesPerBlock) {
  juce::dsp::ProcessSpec spec;
  spec.maximumBlockSize = samplesPerBlock;
  spec.sampleRate = sampleRate;
  spec.numChannels = 1;

  delayA = juce::dsp::DelayLine<float,
                                juce::dsp::DelayLineInterpolationTypes::Linear>{
      1024};
  delayB = juce::dsp::DelayLine<float,
                                juce::dsp::DelayLineInterpolationTypes::Linear>{
      1024};
  delayC = juce::dsp::DelayLine<float,
                                juce::dsp::DelayLineInterpolationTypes::Linear>{
      1024};
  delayA.prepare(spec);
  delayB.prepare(spec);
  delayC.prepare(spec);
  delayA.reset();
  delayB.reset();
  delayC.reset();
}

void GS1_juceAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

bool GS1_juceAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  return true;
}

void GS1_juceAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                          juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  for (size_t i = 0; i < buffer.getNumSamples(); i++) {
    for (const auto metadata : midiMessages) {
      auto message = metadata.getMessage();
      if (metadata.samplePosition != i) {
        continue;
      }

      if (message.isNoteOn()) {
        noteOn(voiceStates[lastVoice], message.getNoteNumber() - 24,
               127 - message.getVelocity());
        lastVoice = (lastVoice + 1) % 32;
      } else if (message.isNoteOff()) {
        for (size_t nVoice = 0; nVoice < 32; nVoice++) {
          if (voiceStates[nVoice].midiNote == message.getNoteNumber() - 24) {
            if (!voiceStates[nVoice].sustaining) {
              voiceStates[nVoice].GATENEW = 0;
            }
            voiceStates[nVoice].noteOn = false;
          }
        }
      } else if (message.isSustainPedalOff()) {
        for (size_t nVoice = 0; nVoice < 32; nVoice++) {
          if (!voiceStates[nVoice].noteOn) {
            voiceStates[nVoice].GATENEW = 0;
            voiceStates[nVoice].noteOn = false;
            voiceStates[nVoice].sustaining = false;
          }
        }
      } else if (message.isSustainPedalOn()) {
        for (size_t nVoice = 0; nVoice < 32; nVoice++) {
          voiceStates[nVoice].sustaining = true;
        }
      }
    }

    int sumSample = 0;
    for (size_t nVoice = 0; nVoice < 32; nVoice++) {
      sumSample += fmGenSample(voiceStates[nVoice]);
    }
    float sample = map(sumSample, -262144 / 6, 262112 / 6, -1, 1);
    delayA.pushSample(0, sample);
    delayB.pushSample(0, sample);
    delayC.pushSample(0, sample);
  }

  float *channelDataL = buffer.getWritePointer(0);
  float *channelDataR = buffer.getWritePointer(1);

  // bypass
  // for (int i = 0; i < buffer.getNumSamples(); i++) {
  //   channelDataL[i] = map(outSamples[i], -262144/6, 262112/6, -1, 1);
  //   channelDataR[i] = map(outSamples[i], -262144/6, 262112/6, -1, 1);
  // }

  // chorus
  for (int i = 0; i < buffer.getNumSamples(); i++) {
    chorusPos++;

    float A1 = sin(map((chorusPos & 65535), 0, 65535, 0, 6.28));
    float B1 = sin(map((chorusPos & 65535), 0, 6553.5, 0, 6.28));
    // Slightly random to make more analog effect feel
    float lup1 = map(((A1 * 2.7) + B1) / 3.7, -1, 1, 0, 61);
    float A2 =
        sin(map(((chorusPos & 65535) + 21845) & 65535, 0, 65535, 0, 6.28));
    float B2 =
        sin(map(((chorusPos & 65535) + 21845) & 65535, 0, 6553.5, 0, 6.28));
    float lup2 = map(((A2 * 2.7) + B2) / 3.7, -1, 1, 0, 60);
    float A3 =
        sin(map(((chorusPos & 65535) + 43690) & 65535, 0, 65535, 0, 6.28));
    float B3 =
        sin(map(((chorusPos & 65535) + 43690) & 65535, 0, 6553.5, 0, 6.28));
    float lup3 = map(((A3 * 2.7) + B3) / 3.7, -1, 1, 0, 63);

    delayA.setDelay(lup1);
    delayB.setDelay(lup2);
    delayC.setDelay(lup3);

    float sampA = delayA.popSample(0);
    channelDataL[i] = (sampA / 2) + delayC.popSample(0);
    channelDataR[i] = (sampA / 2) + delayB.popSample(0);
  }
}

//==============================================================================
bool GS1_juceAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *GS1_juceAudioProcessor::createEditor() {
  return new GS1_juceAudioProcessorEditor(*this);
}

//==============================================================================
void GS1_juceAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {}

void GS1_juceAudioProcessor::setStateInformation(const void *data,
                                                 int sizeInBytes) {}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new GS1_juceAudioProcessor();
}
