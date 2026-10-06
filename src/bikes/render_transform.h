#pragma once

#include <array>
#include <cstdint>

namespace hff::bikes {

// The right, forward and up axes and the position of a CMatrix, three floats
// each.
using Transform = std::array<float, 12>;

bool CopyMatrixTransform(uintptr_t matrix, Transform& out);
bool WriteMatrixTransform(uintptr_t matrix, const Transform& transform);
// The same for the matrix of an entity.
bool CopyEntityTransform(void* entity, Transform& out);
bool WriteEntityTransform(void* entity, const Transform& transform);
// The transform `alpha` of the way from `previous` to `current`, with the axes
// kept unit length and at right angles.
Transform InterpolateTransform(const Transform& previous,
                               const Transform& current, float alpha);

} // namespace hff::bikes
