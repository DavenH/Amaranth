#pragma once

#include "JuceHeader.h"

inline void suppressOpenGLDebugNotifications() {
  #if JUCE_DEBUG && ! JUCE_DISABLE_ASSERTIONS
    if (juce::gl::glDebugMessageControl != nullptr) {
        juce::gl::glDebugMessageControl(
                juce::gl::GL_DONT_CARE,
                juce::gl::GL_DONT_CARE,
                juce::gl::GL_DEBUG_SEVERITY_NOTIFICATION,
                0,
                nullptr,
                juce::gl::GL_FALSE);
    }
  #endif
}
