#pragma once

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