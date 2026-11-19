#ifndef USDSTUDIO_HYDRACAMERACONTROLLER_H
#define USDSTUDIO_HYDRACAMERACONTROLLER_H

#include "pxr/pxr.h"
#include "pxr/base/gf/frustum.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/usd/usd/stage.h"

PXR_NAMESPACE_USING_DIRECTIVE

// Orbit/pan/dolly trackball around a target point, expressed as the
// view/projection matrices consumed by UsdImagingGLEngine::SetCameraState.
// Pure value type: the GUI thread mutates it, the render thread only reads
// a copied pair of matrices handed over by the viewport item.
class HydraCameraController
{
public:
    void setViewportSize(double width, double height);

    // Deltas are in device pixels.
    void orbit(double dx, double dy);
    void pan(double dx, double dy);
    void dolly(double factor);

    // Frames the stage's world-space bounding box (falls back to a unit
    // cube when the stage has no geometry).
    void fitToStage(const UsdStageRefPtr& stage, double time = 0.0);

    // Horizontal field of view of the (square-window) frustum, degrees.
    void setFieldOfView(double degrees);

    GfVec3d eye() const;
    GfVec3d target() const { return _target; }
    double distance() const { return _distance; }

    GfFrustum frustum() const;
    GfMatrix4d viewMatrix() const { return frustum().ComputeViewMatrix(); }
    GfMatrix4d projectionMatrix() const { return frustum().ComputeProjectionMatrix(); }

private:
    GfVec3d _target{ 0.0, 0.0, 0.0 };
    double _yaw = 0.0;      // degrees about world +Y
    double _pitch = 20.0;   // degrees about the camera's local X
    double _distance = 10.0;
    double _fov = 90.0;     // matches the GfFrustum default window (+-1)
    double _width = 1.0;
    double _height = 1.0;
};

#endif // USDSTUDIO_HYDRACAMERACONTROLLER_H
