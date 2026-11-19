#pragma once
#include <QObject>

class Document;
class QQmlApplicationEngine;
class QQuickWindow;
class QVulkanInstance;
class HydraViewportItem;

// Dedicated Hydra viewport window: a QQuickWindow driven by the Vulkan
// backend (via an owned QVulkanInstance) that loads Viewer.qml and hands the
// document to the HydraViewportItem inside it. Owned by MainWindow; the
// existing QQuickWidget/Default.qml central view is unaffected (the QVulkanInstance
// is per-window, the app-wide scene graph API stays as-is).
class HydraViewerWindow final : public QObject
{
    Q_OBJECT
public:
    explicit HydraViewerWindow(Document* document, QObject* parent = nullptr);
    // Destroys the QML engine (and with it the QQuickWindow) before the
    // non-QObject QVulkanInstance, which must outlive the window.
    ~HydraViewerWindow() override;

    void show();

    // Re-binds the viewport to (a possibly new) document.
    void setDocument(Document* document);

private:
    Document* _document;
    QVulkanInstance* _vkInstance = nullptr;
    QQmlApplicationEngine* _engine = nullptr;
    QQuickWindow* _window = nullptr;
    HydraViewportItem* _item = nullptr;
};
