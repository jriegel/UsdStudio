#include "ViewerVulkanInstance.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVulkanInstance>

QVulkanInstance* ViewerVulkanInstance::ensure()
{
    // Intentionally leaked: destroying QVulkanInstance at exit is not worth
    // the trouble (and QVulkanInstance is not a QObject, so no parent-based
    // ownership applies). Created on first use, before any Quick window shows.
    static QVulkanInstance* instance = [] {
        auto* candidate = new QVulkanInstance();
        if (candidate->create())
        {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
            return candidate;
        }
        qWarning("ViewerVulkanInstance: QVulkanInstance::create() failed; "
                 "Qt Quick windows use the default RHI backend");
        delete candidate;
        return static_cast<QVulkanInstance*>(nullptr);
    }();
    return instance;
}
