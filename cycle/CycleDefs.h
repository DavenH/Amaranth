#pragma once

#include "JuceHeader.h"
#include <Definitions.h>
#include <Util/Util.h>

#define doUpdate(X) 	getObj(Updater).update(UpdateSources::X)

#ifdef TRACE_LOCKS
     #define lockTrace(X) LockTracer JUCE_JOIN_MACRO(trace, __LINE__) (repo, X)
#else
     #define lockTrace(X)
#endif

#define doOnce(X) { static int times = 0; if (times++ < 1) { X } }
#define do10X(X) { static int times = 0; if (times++ < 10) { X } }

#if PLUGIN_MODE
  #define onlyPlug(X) X
  #define noPlug(X)
#else
  #define onlyPlug(X)
  #define noPlug(X) X
#endif

#ifdef _X64_
  #define bitsSplit(X, Y) Y
#else
  #define bitsSplit(X, Y) X
#endif

namespace ProjectInfo
{
  const char* const  projectName    = "Cycle";
  const char* const  versionString  = "1.9.0";
  const int          versionNumber  = 0x10900;
}