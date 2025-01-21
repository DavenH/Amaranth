#pragma once
#include "../src/Array/Buffer.h"

#ifdef DEBUG_RENDERING
  #define DEBUG_CLEAR() SignalDebugger::instance().clearPlots()
  #define DEBUG_VIEW(data, label) SignalDebugger::instance().plotSignal(data, label)
  #define DEBUG_HEATMAP(label) SignalDebugger::instance().plotHeatmap(label)
  #define DEBUG_ADD_TO_HEATMAP(label, data) SignalDebugger::instance().addBufferToGrid(label, data)
  #define DEBUG_PERIODS(sample, label) SignalDebugger::instance().plotPeriodHeatmap(sample, label)
#else
  #define DEBUG_VIEW(data, label) ((void)0)
  #define DEBUG_CLEAR() ((void)0)
  #define DEBUG_HEATMAP(label) ((void)0)
  #define DEBUG_ADD_TO_HEATMAP(label, data) ((void)0)
  #define DEBUG_PERIODS(sample, label) ((void)0)
#endif

float real(const Complex32& c);
float imag(const Complex32& c);
float mag(const Complex32& c);
Complex32 makeComplex(float r, float i);

#if (defined(JUCE_WINDOWS) && (JUCE_WINDOWS == 1))
#define platformSplit(X, Y, Z) X
#elif (defined(JUCE_MAC) && (JUCE_MAC == 1))
#define platformSplit(X, Y, Z) Y
#elif (defined(JUCE_LINUX) && (JUCE_LINUX == 1))
#define platformSplit(X, Y, Z) Z
#else
#error "Include JuceHeader.h"
// #define platformSplit(X, Y, Z) ""
#endif

void print(const Buffer<Complex32>& buffer);
void print(const Buffer<Float32>& buffer);