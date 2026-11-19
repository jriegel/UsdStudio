#include "../Public/HydraViewerWindow.h"

#include "Document.h"
#include "../Public/HydraViewportItem.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QUrl>
#include <QVulkanInstance>

HydraViewerWindow::HydraViewerWindow(Document* document, QObject* parent)
    : QObject(parent), _document(document)
{
    HydraViewportItem::registerQmlType();

    // A window-local QVulkanInstance makes Qt's scene graph pick the Vulkan
    // backend for this window only. Qt creates the VkInstance; Hydra's HgiVulkan
    // lives separately on the render thread (AOV hand-off via HydraAovSource).
    // Qt 6.8: QVulkanInstance is not a QObject, so it is deleted by hand in
    // the destructor (after the window is gone).
    _vkInstance = new QVulkanInstance();
    // The scene graph only uses Vulkan if we ask for it *and* initialize the
    // QVulkanInstance before the window is created. Storm's Hgi is Vulkan and
    // phase-B AOV interop wants the same API; the D3D11 fallback stays usable
    // (dummy vertex buffer is bound for the blit draw).
    if (!_vkInstance->create())
        qWarning("HydraViewer: QVulkanInstance init failed, falling back to another RHI backend");
    else
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    _engine = new QQmlApplicationEngine(this);
    // vcpkg's Qt keeps QLibraryInfo paths pointing into the vcpkg tree, which
    // does not hold the deployed QML modules; windeployqt copies them next to
    // the executable, so add that location explicitly.
    _engine->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    _engine->load(QUrl(QStringLiteral("qrc:/Viewer.qml")));
    const auto roots = _engine->rootObjects();
    _window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
    if (!_window) {
        qWarning("HydraViewer: failed to instantiate qrc:/Viewer.qml");
        return;
    }
    _window->setVulkanInstance(_vkInstance);

    if (auto* item = _window->findChild<HydraViewportItem*>()) {
        _item = item;
        _item->setDocument(_document);
    } else {
        qWarning("HydraViewer: Viewer.qml contains no HydraViewportItem");
    }
}

HydraViewerWindow::~HydraViewerWindow()
{
    // _window is owned by _engine; destroying the engine first guarantees
    // the Vulkan-backed window is gone before the QVulkanInstance dies.
    delete _engine;
    _engine = nullptr;
    _window = nullptr;
    _item = nullptr;
    delete _vkInstance;
    _vkInstance = nullptr;
}

void HydraViewerWindow::setDocument(Document* document)
{
    _document = document;
    if (_item)
        _item->setDocument(document);
}

void HydraViewerWindow::show()
{
    if (_window)
        _window->show();
}
