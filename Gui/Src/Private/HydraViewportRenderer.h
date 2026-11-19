#pragma once
// The render-thread half of HydraViewportItem: owns the HgiVulkan instance, the
// UsdImagingGL engines (one per renderer plugin) and the QRhi composite pass.
// Hgi (and the engines built on it) are created and destroyed on the render
// thread - Hgi has strict thread affinity. Qt's QRhi owns its own Vulkan
// device; the AOV hand-off goes through HydraAovSource (CPU upload in phase A,
// zero-copy external-memory interop in phase B).

#include "HydraAovSource.h"
#include "../Public/HydraViewportItem.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/pxr.h"

#include <QtQuick/qquickrhiitem.h>

#include <map>
#include <memory>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class HgiVulkan;
class HdRenderBuffer;
class UsdImagingGLEngine;
PXR_NAMESPACE_CLOSE_SCOPE

class QRhi;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
class QRhiTexture;
class QRhiSampler;
class QRhiBuffer;
class QRhiResourceUpdateBatch;
class QRhiRenderPassDescriptor;

class HydraViewportRenderer final : public QQuickRhiItemRenderer
{
public:
    explicit HydraViewportRenderer(HydraViewportItem* item);
    ~HydraViewportRenderer() override;

    // Qt 6.8.2 QQuickRhiItemRenderer API (cb/item still passed in).
    void initialize(QRhiCommandBuffer* cb) override;
    void synchronize(QQuickRhiItem* item) override;
    void render(QRhiCommandBuffer* cb) override;

private:
    bool ensureEngine();
    void ensureTexture(const GfVec2i& size, QRhiRenderPassDescriptor* targetRP);

    HydraViewportItem* _item;

    // GUI-thread snapshot (synchronize) consumed on the render thread.
    HydraViewportItem::SyncState _state;
    std::string _activeRendererId;
    bool _stateDirty = true;
    GfVec2i _renderBufferSize{0, 0};   // Storm buffer size last requested

    std::unique_ptr<HgiVulkan> _hgi;
    // QRhi the current blit resources were created against. initialize() is
    // called on *every* sync (not just once), so all one-time setup must be
    // guarded on this changing.
    QRhi* _rhi = nullptr;
    std::map<std::string, std::unique_ptr<UsdImagingGLEngine>> _engines;
    UsdImagingGLEngine* _engine = nullptr;
    // Stage the cached engines were populated with. UsdImagingGLEngine binds
    // to the first stage it renders (its internal _isPopulated gate) and never
    // repopulates on a stage swap, so the engines are recreated whenever the
    // document's stage changes.
    UsdStagePtr _boundStage;
    std::unique_ptr<HydraAovSource> _aovSource;

    // QRhi composite pass.
    std::unique_ptr<QRhiGraphicsPipeline> _pipeline;
    std::unique_ptr<QRhiShaderResourceBindings> _srb;
    std::unique_ptr<QRhiBuffer> _vertexData; // fullscreen-triangle verts
    std::unique_ptr<QRhiTexture> _colorTex;
    std::unique_ptr<QRhiSampler> _sampler;
    GfVec2i _texSize{0, 0};
    QRhiRenderPassDescriptor* _usedRP = nullptr;
    bool _pipelineReady = false;
    bool _lastConverged = false;
};
