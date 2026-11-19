#include "../Public/HydraViewportItem.h"

#include "HydraViewportRenderer.h"
#include "Document.h"

#include "pxr/base/tf/token.h"
#include "pxr/usdImaging/usdImagingGL/engine.h"

#include <QQmlEngine>
#include <QQuickWindow>

#include <mutex>

PXR_NAMESPACE_USING_DIRECTIVE

HydraViewportItem::HydraViewportItem(QQuickItem* parent)
    : QQuickRhiItem(parent)
{
}

QStringList HydraViewportItem::rendererIds() const
{
    QStringList ids;
    for (const TfToken& id : UsdImagingGLEngine::GetRendererPlugins())
        ids << QString::fromStdString(id.GetString());
    return ids;
}

QStringList HydraViewportItem::rendererNames() const
{
    QStringList names;
    for (const TfToken& id : UsdImagingGLEngine::GetRendererPlugins())
        names << QString::fromStdString(UsdImagingGLEngine::GetRendererDisplayName(id));
    return names;
}

void HydraViewportItem::setRendererId(const QString& rendererId)
{
    if (rendererId == _rendererId)
        return;
    _rendererId = rendererId;
    _rendererName = QString::fromStdString(
        UsdImagingGLEngine::GetRendererDisplayName(TfToken(rendererId.toStdString())));
    Q_EMIT rendererChanged();
    update();
}

void HydraViewportItem::frameView()
{
    _camera.fitToStage(_document ? _document->getStage() : UsdStageRefPtr());
    update();
}

void HydraViewportItem::orbit(double dx, double dy)
{
    _camera.orbit(dx, dy);
    update();
}

void HydraViewportItem::pan(double dx, double dy)
{
    _camera.pan(dx, dy);
    update();
}

void HydraViewportItem::dolly(double factor)
{
    _camera.dolly(factor);
    update();
}

HydraViewportItem::SyncState HydraViewportItem::captureSyncState()
{
    SyncState state;
    if (_document)
        state.stage = _document->getStage();
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    _camera.setViewportSize(std::max<qreal>(1.0, width() * dpr),
                            std::max<qreal>(1.0, height() * dpr));
    // Frame the stage whenever a new document/stage shows up (open, close).
    if (state.stage != _lastStage) {
        _lastStage = state.stage;
        _camera.fitToStage(state.stage);
    }
    state.view = _camera.viewMatrix();
    state.projection = _camera.projectionMatrix();
    return state;
}

void HydraViewportItem::setConverged(bool converged)
{
    if (_converged == converged)
        return;
    _converged = converged;
    Q_EMIT convergedChanged();
}

QQuickRhiItemRenderer* HydraViewportItem::createRenderer()
{
    return new HydraViewportRenderer(this);
}

void HydraViewportItem::registerQmlType()
{
    static std::once_flag once;
    std::call_once(once, [] {
        qmlRegisterType<HydraViewportItem>("UsdStudio", 1, 0, "HydraViewportItem");
    });
}
