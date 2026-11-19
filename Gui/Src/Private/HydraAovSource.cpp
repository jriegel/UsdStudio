#include "HydraAovSource.h"

#include "pxr/pxr.h"
#include "pxr/base/gf/half.h"
#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/renderBuffer.h"

// Qt 6.8: the RHI headers are part of the private QtGui API.
#include <rhi/qrhi.h>

#include <cstdint>
#include <cstring>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{

// Minimal float -> half encoder (round-to-zero, subnormals kept down to
// half's smallest). Display path only, so exact rounding is not required.
uint16_t floatToHalf(float f)
{
    uint32_t x = 0;
    std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    const int32_t exp = int32_t(x >> 23) - 127 + 15;
    if (exp >= 31)
        return uint16_t(sign | 0x7C00u);           // inf / overflow
    if (exp >= 1)
        return uint16_t(sign | (uint32_t(exp) << 10) | ((x & 0x7FFFFFu) >> 13));
    if (exp >= -9) {                               // half subnormal
        const uint32_t mant = (x & 0x7FFFFFu) | 0x800000u;
        return uint16_t(sign | (mant >> (14 - exp)));
    }
    return uint16_t(sign);
}

} // namespace

HydraAovSource::~HydraAovSource() = default;

namespace
{

class CpuAovSource final : public HydraAovSource
{
public:
    bool present(HdRenderBuffer* rb, QRhiTexture* tex, int width, int height,
                 QRhiResourceUpdateBatch* updates) override
    {
        if (!rb) return false;
        if (!tex || width <= 0 || height <= 0) return false;
        if (int(rb->GetWidth()) != width || int(rb->GetHeight()) != height)
            return false;   // swapchain/Storm buffer resize; next frame matches

        const void* base = rb->Map();
        if (!base) return false;

        const HdFormat format = rb->GetFormat();
        const bool isHalf = format == HdFormatFloat16Vec3 || format == HdFormatFloat16Vec4;
        const bool isFloat = format == HdFormatFloat32Vec3 || format == HdFormatFloat32Vec4;
        const bool isByte = format == HdFormatUNorm8Vec3 || format == HdFormatUNorm8Vec4;
        const int channels = (format == HdFormatFloat16Vec3 || format == HdFormatFloat32Vec3
                              || format == HdFormatUNorm8Vec3) ? 3 : 4;
        const size_t elemSize = isHalf ? 2u : (isFloat ? 4u : (isByte ? 1u : 0u));

        bool ok = false;
        if (elemSize) {
            _rgba.resize(size_t(width) * height * 4);
            const auto row = static_cast<const char*>(base);
            // Width-based row addressing: verified against both hdStorm and
            // hdEmbree buffers (their stride equals width * element size).
            for (int y = 0; y < height; ++y) {
                const char* p = row + size_t(y) * width * channels * elemSize;
                uint16_t* dst = _rgba.data() + (size_t(y) * width) * 4;
                for (int x = 0; x < width; ++x, p += channels * elemSize) {
                    for (int c = 0; c < 4; ++c) {
                        float v = 0.0f;
                        if (c >= channels) {
                            v = 1.0f;
                        } else if (isHalf) {
                            GfHalf hv;
                            std::memcpy(&hv, p + c * 2, 2);
                            v = float(hv);
                        } else if (isFloat) {
                            std::memcpy(&v, p + c * 4, 4);
                        } else {
                            v = float(reinterpret_cast<const uint8_t*>(p)[c]) / 255.0f;
                        }
                        dst[x * 4 + c] = floatToHalf(v);
                    }
                }
            }
            QRhiTextureSubresourceUploadDescription desc(
                                           _rgba.data(), size_t(width) * height * 8);
            updates->uploadTexture(tex,
                                           QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, desc)));
            ok = true;
        }
        rb->Unmap();
        return ok;
    }

private:
    std::vector<uint16_t> _rgba;
};

} // namespace

std::unique_ptr<HydraAovSource> MakeCpuAovSource()
{
    return std::make_unique<CpuAovSource>();
}
