#include "../Public/HydraViewerWindow.h"

#include "Document.h"
#include "../Public/HydraViewportItem.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QUrl>
#include <QVulkanInstance>

#include "ViewerVulkanInstance.h"

HydraViewerWindow::HydraViewerWindow(Document* document, QObject* parent)
    : QObject(parent), _document(document)
{
    HydraViewportItem::registerQmlType();

    // Shared process-wide QVulkanInstance (also used by the central viewer
    // window); it forces the Vulkan RHI backend app-wide. Hydra's HgiVulkan
    // still lives separately on the render thread (AOV hand-off via
    // HydraAovSource; device sharing is the M3 zero-copy step).
    // Qt 6.8: QVulkanInstance is not a QObject and is owned by the helper,
    // so we only reference it here.
    _vkInstance = ViewerVulkanInstance::ensure();
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
    if (_vkInstance)
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
    _vkInstance = nullptr; // owned by ViewerVulkanInstance
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
