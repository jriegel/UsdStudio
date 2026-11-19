#ifndef USDSTUDIO_HYDRAAOVSOURCE_H
#define USDSTUDIO_HYDRAAOVSOURCE_H

#include <memory>

#include "pxr/pxr.h"

class QRhi;
class QRhiTexture;
class QRhiResourceUpdateBatch;

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderBuffer;
PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

// How a rendered Hydra color AOV gets into a QRhiTexture the viewport can
// sample. CpuAovSource (Map + convert + upload) works for every delegate;
// a future VulkanAovSource would import Hgi's VkImage zero-copy.
class HydraAovSource
{
public:
    virtual ~HydraAovSource();

    // Converts rb's mapped samples into an RGBA16F upload of size
    // width x height and schedules the texture upload. Returns false if the
    // buffer is unusable (unmapped, unsupported format, size mismatch).
    virtual bool present(HdRenderBuffer* rb, QRhiTexture* tex,
                         int width, int height,
                         QRhiResourceUpdateBatch* updates) = 0;
};

std::unique_ptr<HydraAovSource> MakeCpuAovSource();

#endif // USDSTUDIO_HYDRAAOVSOURCE_H
