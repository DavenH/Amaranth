/*

    IMPORTANT! This file is auto-generated each time you save your
    project - if you alter its contents, your changes may be overwritten!

*/

// Work around GCC9 + JUCE Harfbuzz __has_builtin shim selecting
// libstdc++'s __make_integer_seq path, which is unavailable on this toolchain.
#if defined(__GNUC__) && ! defined(__clang__) && ! defined(__has_builtin)
 #define __has_builtin(x) 0
 #define AMARANTH_UNDEF_HAS_BUILTIN 1
#endif

#include <juce_graphics/juce_graphics_Harfbuzz.cpp>

#if defined(AMARANTH_UNDEF_HAS_BUILTIN)
 #undef AMARANTH_UNDEF_HAS_BUILTIN
 #undef __has_builtin
#endif
