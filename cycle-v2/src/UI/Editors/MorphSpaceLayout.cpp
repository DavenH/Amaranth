#include <limits>

#include "UI/Editors/MorphSpaceLayout.h"

namespace CycleV2 {

using namespace juce;

namespace {

Point<float> cubeProjection(float red, float blue, float yellow) {
    return { 0.525f * red + 0.675f * yellow + 0.188f,
            0.300f * red + 0.675f * (1.f - blue) - 0.225f * yellow + 0.300f };
}

}

MorphSpaceLayout::MorphSpaceLayout(Rectangle<float> diagram, MorphDimensions activeDimensions) :
        dimensions (activeDimensions) {
    if (diagram.isEmpty() || dimensions.axisCount() == 0) {
        return;
    }
    const auto body = diagram.withTrimmedTop(22.f);
    if (dimensions.axisCount() == 3) {
        surface = body.withTrimmedLeft(38.f).withTrimmedRight(14.f)
                .withTrimmedBottom(24.f);
        std::array<Point<float>, 8> expanded;
        float left = std::numeric_limits<float>::max();
        float top = left;
        float right = std::numeric_limits<float>::lowest();
        float bottom = right;
        for (int corner = 0; corner < 8; ++corner) {
            expanded[(size_t) corner] = cubeProjection(
                    (corner & 1) != 0 ? 1.045f : -0.045f,
                    (corner & 2) != 0 ? 1.045f : -0.045f,
                    (corner & 4) != 0 ? 1.045f : -0.045f);
            const auto point = expanded[(size_t) corner];
            left = jmin(left, point.x);
            right = jmax(right, point.x);
            top = jmin(top, point.y);
            bottom = jmax(bottom, point.y);
        }
        scale = jmin(surface.getWidth() / (right - left), surface.getHeight() / (bottom - top));
        origin = surface.getCentre() - Point<float>((left + right) * 0.5f, (top + bottom) * 0.5f) * scale;
        const auto point = [&](int corner) { return origin + expanded[(size_t) corner] * scale; };
        rails = { Line<float>(point(1), point(5)), Line<float>(point(0), point(1)),
                Line<float>(point(0), point(2)) };
        placeLock(0, { 0.32f, 0.95f });
        placeLock(1, { -0.50f, 0.87f });
        placeLock(2, { -1.f, 0.f });
        labels[0] = Rectangle<float>(10.f, 12.f).withPosition(rails[0].getEnd() + Point<float>(4.f, -6.f));
        labels[1] = Rectangle<float>(10.f, 12.f).withPosition(rails[1].getEnd() + Point<float>(-5.f, 2.f));
        labels[2] = Rectangle<float>(10.f, 12.f).withPosition(rails[2].getEnd() + Point<float>(-5.f, -14.f));
    } else {
        surface = body.withTrimmedLeft(44.f).withTrimmedRight(20.f)
                .withTrimmedTop(12.f).withTrimmedBottom(44.f);
        const float side = jmin(surface.getWidth(), surface.getHeight());
        surface = surface.withSizeKeepingCentre(side, side);
        if (dimensions.axisCount() == 1) {
            rails[0] = { surface.getX() - 7.f, surface.getCentreY() + 8.f,
                    surface.getRight() + 7.f, surface.getCentreY() + 8.f };
            placeLock(0, { 0.f, 1.f });
            labels[0] = Rectangle<float>(10.f, 12.f)
                    .withPosition(rails[0].getEnd() + Point<float>(8.f, -5.f));
        } else {
            int ordinal = 0;
            for (int axis = 0; axis < 3; ++axis) {
                if (dimensions.active(MorphDimensions::axes[(size_t) axis])) {
                    planeAxes[(size_t) ordinal++] = axis;
                }
            }
            const int horizontal = planeAxes[0];
            const int vertical = planeAxes[1];
            rails[(size_t) horizontal] = { surface.getX() - 9.f, surface.getBottom() + 8.f,
                    surface.getRight() + 6.f, surface.getBottom() + 8.f };
            rails[(size_t) vertical] = { surface.getX() - 8.f, surface.getBottom() + 8.f,
                    surface.getX() - 8.f, surface.getY() - 6.f };
            placeLock(horizontal, { 0.f, 1.f });
            placeLock(vertical, { -1.f, 0.f });
            labels[(size_t) horizontal] = Rectangle<float>(10.f, 12.f)
                    .withPosition(rails[(size_t) horizontal].getEnd() + Point<float>(8.f, -5.f));
            labels[(size_t) vertical] = Rectangle<float>(10.f, 12.f)
                    .withPosition(rails[(size_t) vertical].getEnd() + Point<float>(-14.f, -5.f));
        }
    }
    for (int corner = 0; corner < 8; ++corner) {
        corners[(size_t) corner] = project((corner & 1) != 0 ? 1.f : 0.f,
                (corner & 2) != 0 ? 1.f : 0.f, (corner & 4) != 0 ? 1.f : 0.f);
    }
}

Point<float> MorphSpaceLayout::project(float red, float blue, float yellow) const {
    if (dimensions.axisCount() == 3) {
        return origin + cubeProjection(red, blue, yellow) * scale;
    }
    if (dimensions.axisCount() == 1) {
        return { surface.getX() + surface.getWidth() * yellow, surface.getCentreY() };
    }
    const float values[] { yellow, red, blue };
    return { surface.getX() + surface.getWidth() * values[planeAxes[0]],
            surface.getBottom() - surface.getHeight() * values[planeAxes[1]] };
}

Rectangle<float> MorphSpaceLayout::inputBounds() const {
    return dimensions.axisCount() == 1
            ? surface.withSizeKeepingCentre(surface.getWidth(), 24.f) : surface;
}

Point<float> MorphSpaceLayout::reducedCoordinates(Point<float> position) const {
    if (surface.isEmpty()) {
        return {};
    }
    return { jlimit(0.f, 1.f, (position.x - surface.getX()) / surface.getWidth()),
            jlimit(0.f, 1.f, (surface.getBottom() - position.y) / surface.getHeight()) };
}

std::array<int, 2> MorphSpaceLayout::cubeFaceAxes(int heldAxis) {
    return heldAxis == 0 ? std::array<int, 2> { 1, 2 }
            : heldAxis == 1 ? std::array<int, 2> { 0, 2 } : std::array<int, 2> { 0, 1 };
}

Point<float> MorphSpaceLayout::cubeFacePoint(int heldAxis, float first, float second) const {
    const auto axes = cubeFaceAxes(heldAxis);
    std::array<float, 3> values {};
    values[(size_t) heldAxis] = heldAxis == 0 ? 0.f : 1.f;
    values[(size_t) axes[0]] = first;
    values[(size_t) axes[1]] = second;
    return project(values[1], values[2], values[0]);
}

Point<float> MorphSpaceLayout::cubeFaceCoordinates(int heldAxis, Point<float> delta) const {
    const auto axes = cubeFaceAxes(heldAxis);
    constexpr int cornerBits[] { 4, 1, 2 };
    const auto first = corners[(size_t) cornerBits[axes[0]]] - corners[0];
    const auto second = corners[(size_t) cornerBits[axes[1]]] - corners[0];
    const auto inverse = AffineTransform(first.x, second.x, 0.f, first.y, second.y, 0.f).inverted();
    return delta.transformedBy(inverse);
}

int MorphSpaceLayout::cubeFaceAt(Point<float> position) const {
    if (dimensions.axisCount() != 3 || surface.isEmpty()) {
        return -1;
    }
    // The visible faces are Yellow=0 (left), Red=1 (right), Blue=1 (top).
    for (int axis = 0; axis < 3; ++axis) {
        const auto coordinate = cubeFaceCoordinates(axis, position - cubeFacePoint(axis, 0.f, 0.f));
        if (coordinate.x >= 0.f && coordinate.x <= 1.f
                && coordinate.y >= 0.f && coordinate.y <= 1.f) {
            return axis;
        }
    }
    return -1;
}

std::array<float, 3> MorphSpaceLayout::dragCubeFace(
        int heldAxis, Point<float> delta, std::array<float, 3> initial) const {
    if (dimensions.axisCount() != 3 || heldAxis < 0 || heldAxis > 2 || surface.isEmpty()) {
        return initial;
    }
    const auto axes = cubeFaceAxes(heldAxis);
    const auto change = cubeFaceCoordinates(heldAxis, delta);
    initial[(size_t) axes[0]] = jlimit(0.f, 1.f, initial[(size_t) axes[0]] + change.x);
    initial[(size_t) axes[1]] = jlimit(0.f, 1.f, initial[(size_t) axes[1]] + change.y);
    return initial;
}

void MorphSpaceLayout::placeLock(int axis, Point<float> outward) {
    locks[(size_t) axis] = Rectangle<float>(28.f, 28.f)
            .withCentre(rails[(size_t) axis].getPointAlongLineProportionally(0.5f) + outward * 22.f);
}

Rectangle<float> MorphSpaceLayout::lockBadge(Rectangle<float> target) {
    return target.isEmpty() ? target : target.withSizeKeepingCentre(20.f, 22.f);
}

}
