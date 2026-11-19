#include "../Public/HydraCameraController.h"

#include "pxr/base/gf/bbox3d.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/tf/token.h"
#include "pxr/usd/usd/timeCode.h"
#include "pxr/usd/usdGeom/bboxCache.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPitchLimit = 89.0;

// GfRotation has no vector-rotate member; go through the matrix.
GfVec3d xform(const GfRotation& rot, const GfVec3d& v)
{
    GfMatrix4d m(1.0);
    m.SetRotate(rot);
    return m.TransformDir(v);
}
}

void HydraCameraController::setViewportSize(double width, double height)
{
    _width = std::max(1.0, width);
    _height = std::max(1.0, height);
}

void HydraCameraController::orbit(double dx, double dy)
{
    _yaw += dx * 0.25;
    _pitch = std::clamp(_pitch + dy * 0.25, -kPitchLimit, kPitchLimit);
}

void HydraCameraController::pan(double dx, double dy)
{
    // Move the target in the camera's local XY plane, scaled with distance
    // so the grab-the-scene feel is resolution and zoom independent.
    const double unitsPerPixel = 2.0 * std::tan(_fov * M_PI / 360.0) * _distance / _height;
    const GfRotation rot(GfVec3d(1, 0, 0), _pitch);
    const GfRotation yaw(GfVec3d(0, 1, 0), _yaw);
    const GfVec3d right = xform(yaw, GfVec3d(1, 0, 0));
    const GfVec3d up = xform(yaw, xform(rot, GfVec3d(0, 1, 0)));
    _target += right * (-dx * unitsPerPixel) + up * (dy * unitsPerPixel);
}

void HydraCameraController::dolly(double factor)
{
    _distance = std::clamp(_distance * factor, 1e-3, 1e6);
}

void HydraCameraController::fitToStage(const UsdStageRefPtr& stage, double time)
{
    GfVec3d center(0.0, 0.0, 0.0);
    double radius = 1.0;

    if (stage) {
        UsdGeomBBoxCache cache(UsdTimeCode(time),
            TfTokenVector{ TfToken("default") }, /*useExtentsHint=*/true);
        const GfBBox3d box = cache.ComputeWorldBound(stage->GetPseudoRoot());
        if (!box.GetRange().IsEmpty()) {
            const GfVec3d min = box.GetRange().GetMin();
            const GfVec3d max = box.GetRange().GetMax();
            center = 0.5 * (min + max);
            radius = 0.5 * (max - min).GetLength();
            if (radius < 1e-4)
                radius = 1.0;
        }
    }

    _target = center;
    const double halfFov = _fov * M_PI / 360.0;
    _distance = radius / std::sin(0.5 * halfFov) * 1.1;
}

void HydraCameraController::setFieldOfView(double degrees)
{
    _fov = std::clamp(degrees, 10.0, 170.0);
}

GfVec3d HydraCameraController::eye() const
{
    // Start from the default frustum orientation (eye on +Z, looking -Z),
    // pitch about the local X axis first, then yaw about world +Y.
    const GfRotation pitchRot(GfVec3d(1, 0, 0), _pitch);
    const GfRotation yawRot(GfVec3d(0, 1, 0), _yaw);
    return _target + xform(yawRot, xform(pitchRot, GfVec3d(0, 0, 1))) * _distance;
}

GfFrustum HydraCameraController::frustum() const
{
    GfFrustum f;
    f.SetProjectionType(GfFrustum::Perspective);
    f.SetPosition(eye());
    f.SetRotation(GfRotation(GfVec3d(1, 0, 0), _pitch) * GfRotation(GfVec3d(0, 1, 0), _yaw));

    // GfFrustum's perspective window is specified at the fixed reference
    // plane z=1 (fov = 2*atan(halfTan)), NOT at the view distance - scaling
    // it by _distance would squeeze the field of view to ~180 degrees of
    // nothing for far views. Scale X by the viewport aspect to keep pixels
    // square.
    const double halfTan = std::tan(_fov * M_PI / 360.0);
    const double hw = halfTan * (_width / _height);
    const double hh = halfTan;
    f.SetWindow(GfRange2d(GfVec2d(-hw, -hh), GfVec2d(hw, hh)));
    f.SetViewDistance(_distance);

    // Near clips just in front of the closest geometry we can orbit to.
    const double nearPlane = std::max(1e-3, _distance * 0.001);
    f.SetNearFar(GfRange1d(nearPlane, _distance * 100.0));
    return f;
}
