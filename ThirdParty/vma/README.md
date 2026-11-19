# Vulkan Memory Allocator (vendored)

`include/vma/vk_mem_alloc.h` is the single-header **Vulkan Memory Allocator
3.2.1** from https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
(MIT license, copyright AMD and other contributors; the license text is at the
top of the header).

OpenUSD's `hgiVulkan` package includes it as `<vma/vk_mem_alloc.h>` and expects
it to come from the Vulkan SDK. Recent SDKs (1.4.363.0 here) no longer ship
VMA, so the header is vendored and `ThirdParty/BuildHgiVulkan/CMakeLists.txt`
adds `ThirdParty/vma/include` to the include path.
