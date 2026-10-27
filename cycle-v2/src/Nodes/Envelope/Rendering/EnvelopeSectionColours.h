#pragma once

#include <array>
#include <Obj/Color.h>

namespace CycleV2 {

inline std::array<Color, 3> envelopeSectionColours() {
    return { Color(0.5f, 0.71f, 1.f, 0.75f),
             Color(0.6f, 0.45f, 0.25f, 0.75f),
             Color(0.5f, 0.2f, 0.25f, 0.75f) };
}

}
