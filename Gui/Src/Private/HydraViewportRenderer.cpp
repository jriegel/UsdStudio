#include "HydraViewportRenderer.h"

#include "../Public/HydraViewportItem.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/vt/value.h"
#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/renderBuffer.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hgi/tokens.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usdImaging/usdImagingGL/engine.h"
#include "pxr/usdImaging/usdImagingGL/renderParams.h"

#include <QFile>
#include <QMetaObject>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include "pxr/imaging/hgiVulkan/hgi.h"
// Vulkan's Xlib platform headers define macros that collide with Qt and USD.
#ifdef Bool
#undef Bool
#endif
#ifdef None
#undef None
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
constexpr const char* kVertShader = ":/usdstudio/Shaders/viewport.vert.qsb";
constexpr const char* kFragShader = ":/usdstudio/Shaders/viewport.frag.qsb";

QShader loadShader(const char* resource)
{
    QFile file(QString::fromLatin1(resource));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("HydraViewer: missing shader resource %s", resource);
        return {};
    }
    QShader shader = QShader::fromSerialized(file.readAll());
    if (!shader.isValid())
        qWarning("HydraViewer: could not deserialize shader %s", resource);
    return shader;
}
} // namespace

HydraViewportRenderer::HydraViewportRenderer(HydraViewportItem* item)
    : _item(item)
{
}

HydraViewportRenderer::~HydraViewportRenderer() = default;

void HydraViewportRenderer::initialize(QRhiCommandBuffer*)
{
    // NOTE: the scene graph calls initialize() on every sync of the item, not
    // just once. Anything created here must be guarded, or the engines (which
    // capture the raw Hgi* at construction) end up rendering against freed
    // memory.
    if (!_hgi) {
        // Hgi is independent of QRhi: the render thread owns it for its whole
        // lifetime; engines keep a raw pointer to it.
        _hgi = std::make_unique<HgiVulkan>();
        if (!_hgi->IsBackendSupported()) {
            qWarning("HydraViewer: HgiVulkan backend not supported, viewport disabled");
            _hgi.reset();
            return;
        }
    }

    QRhi* rhi = this->rhi();
    if (rhi == _rhi)
        return;

    // QRhi (re)created: drop all QRhi-side resources; ensureTexture() rebuilds
    // the texture/SRB/pipeline on the next render.
    _rhi = rhi;
    _vertexData.reset();
    _sampler.reset();
    _srb.reset();
    _pipeline.reset();
    _colorTex.reset();
    _pipelineReady = false;
    _texSize = GfVec2i(0, 0);

    // Dynamic vertex buffer for the fullscreen triangle (pos.xy + uv.xy).
    _vertexData.reset(rhi->newBuffer(QRhiBuffer::Dynamic,
                                     QRhiBuffer::VertexBuffer, 3 * 16));
    _vertexData->create();
    _sampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                   QRhiSampler::None, QRhiSampler::ClampToEdge,
                                   QRhiSampler::ClampToEdge));
    _sampler->create();
}

void HydraViewportRenderer::synchronize(QQuickRhiItem*)
{
    _state = _item->captureSyncState();

    QString rendererId = _item->currentRendererId();
    if (rendererId.isEmpty())
        rendererId = QStringLiteral("HdStormRendererPlugin");
    if (rendererId.toStdString() != _activeRendererId) {
        _activeRendererId = rendererId.toStdString();
        _stateDirty = true;
    }
}

bool HydraViewportRenderer::ensureEngine()
{
    auto it = _engines.find(_activeRendererId);
    if (it == _engines.end()) {
        UsdImagingGLEngine::Parameters params;
        // VtValue must hold exactly Hgi*: the engine fetches it with
        // GetWithDefault<Hgi*>, which does not do polymorphic casts.
        params.driver = HdDriver{HgiTokens->renderDriver,
                                 VtValue(static_cast<Hgi*>(_hgi.get()))};
        params.rendererPluginId = TfToken(_activeRendererId);
        try {
            it = _engines
                     .emplace(_activeRendererId,
                              std::make_unique<UsdImagingGLEngine>(params))
                     .first;
        } catch (const std::exception& e) {
            qWarning("HydraViewer: failed to create renderer '%s': %s",
                     _activeRendererId.c_str(), e.what());
            _engines.erase(_activeRendererId);
            _activeRendererId.clear();
            return false;
        }
        it->second->SetEnablePresentation(false);
        // Pin the AOV set explicitly: the task controller only seeds its
        // render-buffer prims lazily and GetRendererAovs() is advisory.
        it->second->SetRendererAovs({ HdAovTokens->color, HdAovTokens->primId,
                                      HdAovTokens->depth });
        _aovSource = MakeCpuAovSource();
        _stateDirty = true;
    }
    _engine = it->second.get();
    return true;
}

void HydraViewportRenderer::ensureTexture(const GfVec2i& size,
                                          QRhiRenderPassDescriptor* targetRP)
{
    QRhi* rhi = this->rhi();
    if (_texSize != size || !_colorTex) {
        _texSize = size;
        _colorTex.reset(rhi->newTexture(QRhiTexture::RGBA16F,
                                        QSize(size[0], size[1])));
        _colorTex->create();

        _srb.reset(rhi->newShaderResourceBindings());
        _srb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage, _colorTex.get(),
                _sampler.get()),
        });
        _srb->create();

        // The pipeline references the SRB and render-pass descriptor, so a
        // texture/swapchain change forces a (re)build.
        _pipeline.reset();
    }

    if (!_pipeline) {
        _pipeline.reset(rhi->newGraphicsPipeline());
        _pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        _pipeline->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(kVertShader)},
            {QRhiShaderStage::Fragment, loadShader(kFragShader)},
        });
        // Interleaved vertex data: pos.xy at 0, uv.xy at 8 (see render()).
        QRhiVertexInputLayout vin;
        vin.setBindings({
            QRhiVertexInputBinding(16),
        });
        vin.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 8),
        });
        _pipeline->setVertexInputLayout(vin);
        _pipeline->setShaderResourceBindings(_srb.get());
        _pipeline->setRenderPassDescriptor(targetRP);
        _pipelineReady = _pipeline->create();
        _usedRP = targetRP;
        if (!_pipelineReady)
            qWarning("HydraViewer: composite pipeline creation failed");
    } else if (_usedRP != targetRP) {
        // Swapchain recreated (e.g. resize): rebuild against the new pass.
        _pipeline->setRenderPassDescriptor(targetRP);
        _pipelineReady = _pipeline->create();
        _usedRP = targetRP;
    }
}

void HydraViewportRenderer::render(QRhiCommandBuffer* cb)
{
    QRhi* rhi = this->rhi();
    QRhiRenderTarget* rt = renderTarget();
    const QSize px = rt->pixelSize();
    const int w = px.width();
    const int h = px.height();
    if (w <= 0 || h <= 0)
        return;

    bool drew = false;
    // UsdImagingGLEngine populates its render index once, for the first stage
    // it ever renders; a document swap never re-binds an existing engine.
    // Recreate the engines whenever the document's stage changes.
    if (_state.stage != _boundStage) {
        _engines.clear();
        _engine = nullptr;
        _aovSource.reset();
        _renderBufferSize = GfVec2i(0, 0);
        _stateDirty = true;
        _boundStage = _state.stage;
    }
    if (_hgi && ensureEngine()) {
        const GfVec2i size{w, h};
        // Keep Storm's render buffer exactly at the swapchain pixel size: the
        // AOV readback in present() requires an exact match (window resizes
        // never touch _stateDirty otherwise).
        if (_stateDirty || size != _renderBufferSize) {
            _engine->SetRenderBufferSize(size);
            _engine->SetRenderViewport(GfVec4d(0, 0, w, h));
            _renderBufferSize = size;
            _stateDirty = false;
        }
        _engine->SetCameraState(_state.view, _state.projection);

        if (_state.stage) {
            UsdImagingGLRenderParams params;
            // Scene lighting needs lights in the stage (or a lighting rig);
            // verified: with enableLighting and no lights Storm renders pure
            // black. Until the viewer injects a default rig, unlit shading
            // gives the predictable image.
            params.enableLighting = false;
            params.drawMode = UsdImagingGLDrawMode::DRAW_GEOM_SMOOTH;
            _engine->Render(_state.stage->GetPseudoRoot(), params);
        }

        const bool converged = _engine->IsConverged();
        if (converged != _lastConverged) {
            _lastConverged = converged;
            QMetaObject::invokeMethod(_item, "setConverged",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, converged));
        }

        if (_state.stage) {
            ensureTexture(size, rt->renderPassDescriptor());
            QRhiResourceUpdateBatch* ru = rhi->nextResourceUpdateBatch();
            HdRenderBuffer* aov =
                _engine->GetAovRenderBuffer(HdAovTokens->color);
            _aovSource->present(aov, _colorTex.get(), w, h, ru);

            if (_pipelineReady) {
                // Fullscreen clip-space triangle with uv, as vertex data
                // (uv v=0 is the texture top).
                static const float kVerts[12] = {
                    -1.0f, -1.0f, 0.0f, 1.0f,
                     3.0f, -1.0f, 2.0f, 1.0f,
                    -1.0f,  3.0f, 0.0f, -1.0f,
                };
                ru->updateDynamicBuffer(_vertexData.get(), 0,
                                        sizeof(kVerts), kVerts);

                cb->beginPass(rt, QColor::fromRgbF(0.12, 0.12, 0.14, 1.0),
                              {1.0f, 0}, ru);
                cb->setGraphicsPipeline(_pipeline.get());
                cb->setShaderResources(_srb.get());
                const QRhiCommandBuffer::VertexInput vb{ _vertexData.get(), 0 };
                cb->setVertexInput(0, 1, &vb);
                cb->setViewport(QRhiViewport(0, 0, w, h));
                cb->draw(3);
                cb->endPass();
                drew = true;
            }
        }
    }

    if (!drew) {
        QRhiResourceUpdateBatch* ru = rhi->nextResourceUpdateBatch();
        cb->beginPass(rt, QColor::fromRgbF(0.12, 0.12, 0.14, 1.0), {1.0f, 0},
                      ru);
        cb->endPass();
    }

    // Continuous redraw: Storm streams samples over frames (convergence), and
    // QQuickRhiItem update() requests from input handlers were not reliably
    // scheduling a frame, so keep the loop running. update() must run on the
    // GUI thread (we are in the render thread here).
    // TODO(M2): switch to damage-based repaint with the ObjectsChanged hook.
    QMetaObject::invokeMethod(_item, [_item = _item] { _item->update(); },
                              Qt::QueuedConnection);
}
