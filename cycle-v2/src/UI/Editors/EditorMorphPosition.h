#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <Obj/MorphPosition.h>

namespace CycleV2 {

class EditorMorphPosition {
public:
    struct Checkpoint {
        std::array<std::optional<float>, 3> values;
        std::array<uint64_t, 3> previewGenerations;
    };

    void reset();
    void setPreview(std::optional<MorphPosition> position);
    void setValue(int axis, float value);
    Checkpoint checkpoint() const { return { values, previewGenerations }; }
    void restoreValues(const Checkpoint& checkpoint);
    MorphPosition resolve(const MorphPosition& authored, bool preserveValues);

private:
    std::optional<MorphPosition> preview;
    std::optional<MorphPosition> lastAuthored;
    std::array<std::optional<float>, 3> values;
    std::array<uint64_t, 3> previewGenerations {};
};

}
