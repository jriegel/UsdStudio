#ifndef USDSTUDIO_HYDRAVIEWPORTITEM_H
#define USDSTUDIO_HYDRAVIEWPORTITEM_H

#include <QQuickRhiItem>

#include "pxr/pxr.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/usd/usd/stage.h"

#include "HydraCameraController.h"

class Document;

// QQuickRhiItem that renders a UsdStudio Document through Hydra
// (UsdImagingGLEngine over HgiVulkan) and composites the color AOV with QRhi.
// All Hydra/Hgi work lives on the render thread in HydraViewportRenderer;
// this class owns the GUI-thread-side state (camera, renderer selection).
class HydraViewportItem : public QQuickRhiItem
{
    Q_OBJECT
    Q_PROPERTY(QStringList rendererIds READ rendererIds CONSTANT)
    Q_PROPERTY(QStringList rendererNames READ rendererNames CONSTANT)
    Q_PROPERTY(QString rendererName READ rendererName NOTIFY rendererChanged)
    Q_PROPERTY(bool converged READ converged NOTIFY convergedChanged)

public:
    struct SyncState
    {
        pxr::UsdStageRefPtr stage;
        pxr::GfMatrix4d view;
        pxr::GfMatrix4d projection;
    };

    explicit HydraViewportItem(QQuickItem* parent = nullptr);

    // Renderer plugin ids/names from the Hydra plugin registry (GUI thread).
    QStringList rendererIds() const;
    QStringList rendererNames() const;
    QString rendererName() const { return _rendererName; }
    Q_INVOKABLE QString currentRendererId() const { return _rendererId; }
    bool converged() const { return _converged; }

    Q_INVOKABLE void setRendererId(const QString& rendererId);
    Q_INVOKABLE void frameView();
    Q_INVOKABLE void orbit(double dx, double dy);
    Q_INVOKABLE void pan(double dx, double dy);
    Q_INVOKABLE void dolly(double factor);

    // Wired from C++ (HydraViewerWindow) after QML instantiation.
    void setDocument(Document* document) { _document = document; update(); }
    Document* document() const { return _document; }

    // Camera stage hand-over for the renderer, called on the GUI thread
    // during the window's sync phase.
    SyncState captureSyncState();

    static void registerQmlType();

Q_SIGNALS:
    void rendererChanged();
    void convergedChanged();

protected:
    QQuickRhiItemRenderer* createRenderer() override;

private:
    // Invoked (queued) from the render thread.
    Q_INVOKABLE void setConverged(bool converged);

    Document* _document = nullptr;
    HydraCameraController _camera;
    pxr::UsdStageRefPtr _lastStage;   // for auto-framing when the stage changes
    QString _rendererId;      // empty = registry default (Storm)
    QString _rendererName;
    bool _converged = false;
};

#endif // USDSTUDIO_HYDRAVIEWPORTITEM_H
