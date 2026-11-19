// (c)2026 Juergen Riegel, mail@juergen-riegel.net

#pragma once

class QVulkanInstance;

// Process-wide QVulkanInstance shared by every Qt Quick window in the app
// (the embedded central QQuickView and the Hydra Viewer window). One VkInstance
// for the whole app keeps driver memory down and is a prerequisite for the
// later zero-copy AOV hand-off, which wants Qt and HgiVulkan on one device.
class ViewerVulkanInstance
{
public:
    // Creates the instance once and forces the Vulkan scene graph backend for
    // all QQuickWindow-based surfaces. Call before the first Quick window is
    // shown. Returns nullptr when Vulkan is unavailable (no backend override
    // is made in that case, Qt keeps its default backend choice).
    static QVulkanInstance* ensure();
};
