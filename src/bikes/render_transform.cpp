#include "bikes/render_transform.h"

#include "game/addresses.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace hff::bikes {

namespace {

void NormalizeRenderVector(Transform& transform, size_t base) {
    const float lengthSquared = transform[base] * transform[base]
                              + transform[base + 1] * transform[base + 1]
                              + transform[base + 2] * transform[base + 2];
    if (lengthSquared < 0.000001f || !std::isfinite(lengthSquared)) {
        return;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    transform[base] *= inverseLength;
    transform[base + 1] *= inverseLength;
    transform[base + 2] *= inverseLength;
}

} // namespace

bool CopyMatrixTransform(uintptr_t matrix, Transform& out) {
    __try {
        if (!matrix) {
            return false;
        }
        constexpr std::array<size_t, 4> offsets{
            kMatrixRight, kMatrixForward, kMatrixUp, kMatrixPosition
        };
        for (size_t vector = 0; vector < offsets.size(); ++vector) {
            const auto* source = reinterpret_cast<const float*>(
                matrix + offsets[vector]);
            for (size_t axis = 0; axis < 3; ++axis) {
                const float value = source[axis];
                if (!std::isfinite(value)) {
                    return false;
                }
                out[vector * 3 + axis] = value;
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WriteMatrixTransform(uintptr_t matrix, const Transform& transform) {
    __try {
        if (!matrix) {
            return false;
        }
        constexpr std::array<size_t, 4> offsets{
            kMatrixRight, kMatrixForward, kMatrixUp, kMatrixPosition
        };
        for (size_t vector = 0; vector < offsets.size(); ++vector) {
            auto* destination = reinterpret_cast<float*>(
                matrix + offsets[vector]);
            for (size_t axis = 0; axis < 3; ++axis) {
                destination[axis] = transform[vector * 3 + axis];
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool CopyEntityTransform(void* entity, Transform& out) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(entity);
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            address + kEntityMatrix);
        return CopyMatrixTransform(matrix, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool WriteEntityTransform(void* entity, const Transform& transform) {
    __try {
        const auto address = reinterpret_cast<uintptr_t>(entity);
        const auto matrix = *reinterpret_cast<const uintptr_t*>(
            address + kEntityMatrix);
        return WriteMatrixTransform(matrix, transform);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

Transform InterpolateTransform(const Transform& previous,
                               const Transform& current, float alpha) {
    Transform out{};
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = previous[i] + (current[i] - previous[i]) * alpha;
    }

    // Nlerp the basis and remove accumulated shear. A 30 Hz step is small
    // enough that this follows the short rotation arc without a quaternion.
    NormalizeRenderVector(out, 0);
    const float projection = out[3] * out[0] + out[4] * out[1]
                           + out[5] * out[2];
    out[3] -= out[0] * projection;
    out[4] -= out[1] * projection;
    out[5] -= out[2] * projection;
    NormalizeRenderVector(out, 3);
    out[6] = out[1] * out[5] - out[2] * out[4];
    out[7] = out[2] * out[3] - out[0] * out[5];
    out[8] = out[0] * out[4] - out[1] * out[3];
    NormalizeRenderVector(out, 6);
    return out;
}

} // namespace hff::bikes
