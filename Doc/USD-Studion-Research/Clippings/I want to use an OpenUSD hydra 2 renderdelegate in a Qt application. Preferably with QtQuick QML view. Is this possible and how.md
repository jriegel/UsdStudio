---
title: "I want to use an OpenUSD hydra 2 renderdelegate in a Qt application. Preferably with QtQuick QML view. Is this possible and how?"
source: "https://www.google.com/search?q=I+want+to+use+an+OpenUSD+hydra+2+renderdelegate+in+a+Qt+application.+Preferably+with+QtQuick+QML+view.+Is+this+possible+and+how%3F&client=firefox-b-d&hs=VVgB&sca_esv=7bfc6db5b28830ae&sxsrf=AKL5yDg3pbLkhn6mU54kivS6S1rdk9eODw%3A1794918999940&fbs=ACAhTvx0CAPB6TRWsNnItqaR7Zg8Xd2Ksf0a2MFLySnnxd_WlVRasi6GVbVOL6KOxNlTDr_On02fFysRBr3GMGZoI0EfApULSd8df_1cwljz-50IEvOSQeOuqcfmlvQzcDn6DMQvnf7NRlEj_nBGUNcDWxgUlJVoqxVBQ6ZiRCdmzcZPJlWzKOImh4s0bHnVL6evIRTRshl-&vsint=&aep=1&ntc=1&cs=0&sa=X&ved=2ahUKEwjB85Crgo-YAxV3gP0HHTwGHxgQ2J8OegQICxAD&biw=2273&bih=1196&dpr=1.67&mstk=AUtExfBiz8txzEf5PWg__Lq69BgBupb8zoXO-Gbzi0P0cl2VCy0m7m-0Ron5ItUypgXTA8_BzHtS4Dz0iyQdzj4x8ZrUnNNz52YxI2PV_4DqBerllWeczcEOBxALZ1oGU1oGTufcjBzTI-h4UGy918IUIx0CZZB3pov_ftPu3ovww0K9XSTJ9Q7-zW1wMO3EUTaPKs9l2B4aSaXJglgKtVtwMVBcRfQiTzP3X_lBZ6s5fFMzhQW6bOX2ScUSuAuFbkb59bbK0I0RBVUopVE6GM7sS3mH6ulnsrY_V9Ja2-aw4DfOOngd7Svh5wVfIw9NHtLO-hFfKB4SKF3U0Q&csuir=1&mtid=hkr8atifK7qR9u8Pw4KLkAI&udm=50"
author:
published: 2026-09-11
created: 2026-11-17
description:
tags:
  - "clippings"
---
## Unterhaltung im KI-Modus: I want to use an OpenUSD hydra 2 renderdelegate in a Qt application. Preferably with QtQuick QML view. Is this possible and how?

I want to use an OpenUSD hydra 2 renderdelegate in a Qt application. Preferably with QtQuick QML view. Is this possible and how?

### Antwort des KI-Modus für „I want to use an OpenUSD hydra 2 renderdelegate in a Qt application. Preferably with QtQuick QML view. Is this possible and how?“

==**Yes, it is possible to integrate an OpenUSD Hydra 2.0 render delegate into a Qt Quick (QML) application**==**.** Because Qt 6 abstracts graphics backends using **Qt RHI (Rendering Hardware Interface)** while Hydra uses **Hgi (Hydra Graphics Interface)**, the standard approach is sharing the underlying GPU device and passing the rendered texture (AOV) directly into QML without CPU readbacks.

forum.aousd.org +1

---

1\. Choose Your Qt Quick Integration Architecture

Depending on whether you want QML UI elements layered *over* your 3D viewport or just need a fast, dedicated viewport window, choose one of the following patterns:

- **Embedded QML Viewport (`QQuickRhiItem`).** This is the recommended approach in Qt 6 for rendering custom 3D content inside a QML layout. It provides a `QQuickRhiItemRenderer` where you can import Hydra's output texture and draw it as a QML item, allowing standard QML controls, overlays, and transparency to composite cleanly on top.
	Felgo +1
- **Underlay Rendering (`QQuickWindow::beforeRendering`).** You can intercept Qt's render loop via the `beforeRendering` signal to execute Hydra rendering directly onto the window before QML draws its UI. This avoids an intermediate texture copy, though managing viewport clipping and aspect ratios inside complex QML layouts requires manual coordination.
	Qt Documentation +1
- **Separate Native Window (`WindowContainer`).** If your Hydra delegate manages its own swapchain (such as a custom Vulkan or Metal viewport) and you do not need QML elements overlapping the 3D canvas, you can embed a native `QWindow` inside QML.
	Qt Forum

---

2\. Set Up the Hydra 2.0 Scene Index Pipeline

Hydra 2.0 replaces the legacy `HdSceneDelegate` and `HdRenderIndex` with a composable **Scene Index** graph. Even if you use `UsdImagingGLEngine` as a high-level wrapper, enabling Hydra 2.0 ensures your render delegate receives modern scene index notifications (`PrimsAdded`, `PrimsDirtied`):

Universal Scene Description +1

1. **Enable Hydra 2.0.** Ensure `USDIMAGINGGL_ENGINE_ENABLE_SCENE_INDEX` is enabled (which is the default in modern OpenUSD releases) or construct your scene index chain manually via `UsdImagingCreateSceneIndices`.
	Universal Scene Description
2. **Initialize Hgi and the Render Delegate.** Instantiate your chosen `Hgi` backend (e.g., `HgiVulkan`, `HgiMetal`, or `HgiGL`) and pass it to `HdDriver` when initializing `UsdImagingGLEngine` or your `HdRenderIndex`.
	Universal Scene Description +1
3. **Render to an Offscreen AOV.** Configure your Hydra render pass or `HdxTaskController` to output the final color buffer into an `HdRenderBuffer` (such as the `color` AOV) rather than directly to an OS window swapchain.
	forum.aousd.org

---

3\. Share the GPU Context & Hand Off Textures (Zero-Copy)

To prevent severe performance bottlenecks, avoid reading pixels back to the CPU (`Map()` / `glReadPixels`). Instead, share the native graphics device between Hydra and Qt Quick:

forum.aousd.org +1

- **Align Graphics APIs.** Force Qt Quick to use the exact same graphics backend as your Hydra delegate via `QQuickWindow::setGraphicsApi()` (e.g., `QSGRendererInterface::Vulkan` or `Metal`).
- **Share the Device Handle.** Either let `Hgi` create the native device (e.g., `VkDevice` or `MTLDevice`) and pass it to Qt via `QQuickGraphicsDevice::fromDeviceObjects()`, or extract Qt's `QRhiNativeHandles` during `QQuickRhiItemRenderer::initialize()` and pass them into a custom `Hgi` instance.
- **Import the Hydra AOV into `QRhi`.** Retrieve the native texture handle from Hydra's color `HdRenderBuffer::GetResource()` (an `HgiTexture*` opaque handle). Wrap that native image (such as a `VkImage` or `id<MTLTexture>`) into a `QRhiTexture` using `QRhiTexture::createFrom()`.
- **Blit or Draw in QML.** Inside `QQuickRhiItemRenderer::render()`, bind the imported `QRhiTexture` to a fullscreen quad shader (or issue a `QRhiResourceUpdateBatch::copyTexture()`) to display the frame in QML.
	www.idedoc.com +2

---

Key Technical Gotchas to Watch For

- **Thread Synchronization.** Qt Quick's threaded render loop runs on a dedicated `QSGRenderThread`, whereas USD stage edits usually happen on the main GUI thread. Keep all Hydra `Render()` calls and `Hgi` resource updates strictly inside `QQuickRhiItemRenderer::render()` or `synchronize()` to prevent race conditions.
- **Color Space & Premultiplied Alpha.** Hydra typically renders linear HDR color (`Rgba16Float`), while Qt Quick composites in sRGB (`Rgba8Unorm`) and expects **premultiplied alpha**. Run a tone-mapping and gamma-correction pass (either via Hydra's `HdxColorCorrectionTask` or in your QML blit shader) before displaying the texture.
- **Memory Barriers / Layout Transitions.** If using Vulkan or DirectX 12, ensure `Hgi` transitions the shared AOV image to `SHADER_READ_ONLY_OPTIMAL` (and signals a semaphore if using separate queues) before `QRhi` samples it.
	forum.aousd.org +4

If you'd like to dive into concrete code examples, let me know:

- Which **graphics API** you are targeting (Vulkan, Metal, OpenGL, or DirectX)
- Whether you are using **HdStorm** or a **custom Hydra 2.0 render delegate**

I can provide a C++ setup snippet showing how to wire up `QQuickRhiItem` and `Hgi` for your stack.

KI-Antworten können Fehler enthalten. [Weitere Informationen](https://support.google.com/websearch?p=aimode)

In Zwischenablage kopiertFehler beim Kopieren in die Zwischenablage. Versuche es später noch einmal.

## Geteilt

0 Dateien

- Qt Forum
	OpenUSD Hydra + Qt Quick/QRhi/Vulkan Integration...
	September 11, 2026 — How should we keep the image layout tracking consistent between QRhi and Hgi when sharing the same VkImage?
	![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIgAAACICAMAAAALZFNgAAAAZlBMVEX///8AAADKysri4uLp6elra2uLi4tZWVn4+PhwcHDW1taenp42NjbS0tLs7Oz09PQcHBxUVFR9fX29vb1ISEiRkZFhYWGEhIQXFxcQEBCYmJguLi4+Pj7c3NzDw8NPT0+tra0mJiYdMqq2AAADIUlEQVR4nO2Z12KsIBBAQSyIBde26orl/3/yjgXLlsQkd9eXOQ+J4gJHYUCQEARBEARBEARBEARBEAT5DumtGCd6sJpS6tMJezQLTxHJKL0oFdSpPYu4F8c7QwTqFzb1usp0R5HGgUQeiJB5ghihNEiSJB8SYSk8lCrMRpHMJoqFNBZRV5ttwHJCPb/5nAjLDapFLFe2pLHJtXMCZpGUePZnRFRM7cQKJpEkJ9KVDjFcEid5YOYgUmWfEcmZkK2vmqmzWm2bSZeQ3HFIUjsxoU76iTgagqXwg5jeII7HcDFNTiT8ZxyOJSf1cPB+MloUZVn08Ic+b4H4ExoQqRs+UyOCIAiCIMgeVnlbKnGWiEH3dCeKlNHKqSJ5yFbkiSLWWXXvQJF7XoiwxrOzrmH6XAphTunNejLAhZh+pISCBRlk85Ixgaumsr3GJAd5KhK60RTM0XXuvMG4DBRuP6zCBKXLqovp/Bdaq+ttypWRsGqnEm7dH0RUMRTR92NBoRbpVD7vKt2JuOOBsx2N0s3xwd2VZyJwX44BKz/ZQIG5FqlLuNfcZq9FIisxOR82WkDlakjOGbinx0aEJyIJpe28/JRQOZtFoCJPjakvRHq9FxhD2lwAh1s5NmsYuqAVKGfZIurotDQPtq39QsTXuxdKP0egOto2IOJfY82Qh/s0WtbjopzqCeYtxy9FdISYEW035R/bb9pPesPdS+iny+XwNrXxj0RCf+0Y6vGJvxQp/Js/0Q87NSb0r/V6TaPwpyISQlk3E5TmHBWxiNTwqeh8vV7T8scifNNfQORyXGSH2qX8SiT/LyJsl7WmxVkiIcTRcgIh1D8TWTrgG0Xuo2Yc3M4Q4S0tzc3lsRV2IqxfI+GNIsMHi25z7D2IyHQzeL1RxFjm3OF+qXwQIdasB072G0XGGT1rgqC56o9JdyIwLdI4MZruMgzH7xMxN+8WFn8isnn5aL8f0FpyhKCMHve4eVfPLzh65hRlsd0g59l0PRamzp9H6fpEouU7gln2m3H6NzClvnzfDJU65+MfgiAIgiAIgiAIgiAIgiAIgiAI8nf+AR9uLNTAEz9MAAAAAElFTkSuQmCC)
- Universal Scene Description
	Hydra 2.0 Getting Started Guide
	Hydra 2.0 replaces the following legacy Hydra 1.0 classes with scene indices and scene index observers and components built around...
	![](data:image/png;base64,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)
- forum.aousd.org
	HdStorm + Vulkan + Qt QML - Hydra - Alliance for OpenUSD
	12.09.2026 — We're building a USD/Hydra viewer that integrates HdStorm with Qt QML/QtQuick via Vulkan. HdStorm currently support HDR framebuffe...
	![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMwAAACICAMAAACcNfS6AAAAh1BMVEX///8gkdCe3PI0xvOa2/K35PVkq9qP2PIOjc/y9/sAic2ZxuXd8/wZj89EotgnxPNgzvPS5fN80/Fv0fIxl9IpqeAgm9eG2fdouOGk4fQAhMvl8PiAuODc6/bK4PH4/f/I7fu/2u6mzOh1st1Xpdjr+f6Ov+Oz0utLy/RGnNR4vOPR6/ev5fqKipCgAAAGD0lEQVR4nO2b4XaqOhCFRUEDkoLKvb2KGlQEPfr+z3fFag+ZQTAQSFyL71+1xWyzM5mZpINBT09PT09PT09PT48G7Mhe9RBkYScOXa1t1cOQgh0TwzBIvFE9EAnsDGpkUCdVPZTGTMmPlhvOh1vNPji/WjKrhaoH1IBZQIw81NipHlJdWJqflsfCSZjqYdWCrQnUkqk5fOLCCSlBUu4Lx9iqHpowqYGn5Tk5+w+zWpHF/k7OQfXwRNicnddSMjXkc9KBNIDLBf5MjU/JPKdw6VPnC7qO0o+wWhiv0LSkg9kZTo5z1t9qW7RcnHh2e30zhq+TQPfMc4/85Ix/AjFLYD5A6FTxaEuxD9Bi+ZWO44KjcZEzi9Fog1nu/U2MrHbWNR1IKbYYn/OzW0XAQ4iWVrPXK7gmCtoY6TkAEA0zzw2yGAmKipfNDLLVrmLbwcVNP7asxGGX0rXqQdUkPOAthNsQLfNjkn5Y6WfLJb+omel5vn6rvAi2h1GMknV+ItyFNxp5XqRqgALYBzQtqzSv5XK6acmYKBvju2xQqULO+U1/sPQeWkaef1Q1yvcoyCu5PfA4f0rJ1JwsZQN9A1TpU2fKLRc/pyVbOEPuz1mSTAGJqkCBCy4ScHmj5XFaMjkLzmqb84oA6GygghQ1xgiXV9rfUEqmZnTJP8Ne491WQebJpshiK671evQLtGRqrtxz9kgNOXS9w4aoOiZ8U9waFWq5RzVusAVb7rlbq21x0TjOF41s8krKPapF+WfdkiGohnbZHUDNJGPFR+RFiZbMalxUY1OcQ3R2WGDjZhLfaIlgFEPSQFRDBcSt3u6mgNgWtMDyJmdXpMW0TvAlYDXciOrkXCpFLXHnwH2L0GKed9v4j0jNiEsHGCq6KU3alhLiZhJf6btoWh4pmQnNVm21r3athtv75MzZYYlWx/dzKUdIzckVerZkcKXv8JU+2vS9nJlcuI2CDRQXrLS9ayr2GkUx3tcXvMy5734wRB6ccyEYZ0iw8yaLgtZ3xabvfcMKBkc1n8vVqiKlLLaoDHPO+W/ttulDrvgpKGMDUc0eo96IIT0dYElB6OQsYk2GALfwSZOK+ZvCJq8h+0ZUOEbZIK0baSJsNU52Qd4n9bBAbmbLKqwWxnBflpkOTNG1Eb6ZVMkRBIIhthrXaCuqleRYraAaFAz/FxOunwtOB7jf2FVUsXUpqPQNIYsxyxwOzYh/ERUJwGobbLWg+bkUvplExkIWc4dmFtrMK7DaFU3OhHNSVedHHJagHJmIhf1o+QuwWuSPgBo+qqVBeU9OlBBZzKDjPex1lfHnvxx/uLf2yT8I7v0Dun5DgvoxFC/D+/cjAtcTK3uv4DcKPzytabU12ovVQwU3hQdhjJa+FhBDPEbjwk8XqGAIKmjva4TgjagQJeJ6IXIXfxaUX+RTz/s3ol7fFdUHQt9KD3GtpyfOG5cm8M0jXXEq0wF8J0xfaLnV7DU6lhODCvLG35d93Cp+nXmyJB43Iv4SBH7ev4iKD3zd62B2My6mMC54wgm2q/yw/DNbOse5V5SiwAqUwQav510KP61djtcaWm5qrKoKlD9m64RHdVxDzRKocVE97Xd7/FzLYk810GqDb1hPn+BvtMlxWV9LJscCMQle7fA6tJq7bCIlUzMEVjue4OTMpVyJiha+7y/miO9f5if/BQsIfuXOfOFfresTy7IiC6kZRc213IJLNY8gKpMRBN5TEQeFfZV4i0ZRzYWnD2oBV6LEQCcpqmlgtbLLPKr4exIvRMVlHlXU2kC1s9gTcPbxDktNpWTgE/lSuBu8+gHvSpSiWUTGCFgN3RfTEHBP5aXFNNn0YXIGAMdsL2AXt5xJNwwrhuFKSKPdhtn++8ATXemwqFEVJoRpCsSsGtgNquM6aqIWtbhml1ructqy2s1iHWvJ6ul2rMbq9cUa08Y/EHVusSeo2dGcSI2SHzWSG7SKLPZQY0YSW5q1W6/S5MAWbn0URDGkRlJU63ajfIWcDfSoflp+kGC1iy5aJEQ1pVEMYTbZQF3xM8p2mdQvC1xLQzT/Z++enp6enp6enp6eng75H9B4yilL66MxAAAAAElFTkSuQmCC)
- basysKom GmbH
	basysKom GmbH | Hello, RHI - How to get started with Qt RHI
	November 24, 2023 — Qt RHI (Rendering Hardware Interface) is a low-level, cross-platform graphics API public since Qt 6.6, targeting OpenGL ES, Metal,
	![](https://encrypted-tbn2.gstatic.com/images?q=tbn:ANd9GcT-oLMEvOAGS6ONW7kpg9-xI4M58weijDhbQKjNuDV8OX4ne5-yZiGEAEjXJgXB6BszZXhXD6Bpzz65kqfUoPHNSdsoAJYCXTKjqIOR4IU)
- Felgo
	QQuickRhiItemRenderer Class | Qt Quick | Qt Documentation (Pro)
	Reimplementations of initialize() and render() can query those objects via depthStencilBuffer() and renderTarget(). rendering or 3...
- Qt Documentation
	Scene Graph
	This is what allows to conveniently issue a QRhiCommandBuffer::resourceUpdate() When creating a graphics pipeline, if we wanted to...
	![](https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcSNB2WpGsfDpNlUrFl_dpi5w5f9nM2X-WOKkFAJDL196atyaUC9XTtAd2j8ZgXYNwPLpplW0C7PR1wpoTsN9ALPXCQrlgrcEXFIGKm0fKw)
- GitLab
	Qt Quick Scene Graph | Qt Quick 5.12.8 - GitLab
	The rendering of the scene graph happens internally in the QQuickWindow class, add custom scene graph content. The integration poi...
	![](https://encrypted-tbn2.gstatic.com/images?q=tbn:ANd9GcSaxRLgbHfkdIQBC9PCM_DjOScpt3K-XuruMoDg9gtk_r3urMATEykez6qIEqsR_IRwyB5BJo-zoylFFX8iVjNtHRFRbBstv7_YpLX0J6w)
- Universal Scene Description
	Universal Scene Description: Hydra 2.0 Getting Started Guide
	Represents scenes via HdSceneIndexBase and HdSceneIndexPrim; provides PrimsAdded, PrimsRemoved, PrimsDirtied, and PrimsRenamed not...
	![](https://encrypted-tbn1.gstatic.com/images?q=tbn:ANd9GcQ1hG4rt1hroLSIzeZEwkO0J2xk6MxRZzZCsKCicH3H-YLeF7kaksQWbh1KdT7wYVKCJPSvpngp7hbqP4Hh3Mpph5Xe_eBQR1GxSsoSVAc)
- www.idedoc.com
	Scene Graph
	Qt Quick expects premultiplied alpha. See also QQuickRhiItem, Scene Graph - RHI Under QML, and Scene Graph - Custom QSGRenderNode.
	![](https://encrypted-tbn1.gstatic.com/images?q=tbn:ANd9GcSSiG2YKzS5HrJHs0mnfx5gLWvy1WDS9JKxgHardq-mutMfmE-VxU_qU4CJOqbUuxcR4Snq4S85QPIdFC7D6viW3ln1OY4g68OdA90420I)

Alle anzeigen

The graphic API I dont care, which works best for QT on windows and Linux and probably Apple. I want to go the way of QQuickRhiItem and AOV. For the first test I want to use HdStorm (as default) but I want it to be open for all kind of Render Delegates from third party (e.g. Blender).

### Antwort des KI-Modus für „The graphic API I dont care, which works best for QT on windows and Linux and probably Apple. I want to go the way of QQuickRhiItem and AOV. For the first test I want to use HdStorm (as default) but I want it to be open for all kind of Render Delegates from third party (e.g. Blender).“

To support Windows, Linux, and macOS while keeping your application open to any third-party render delegate, use **Vulkan on Windows and Linux** and **Metal on macOS**. Design your viewport around a **renderer-agnostic AOV handoff** rather than tying your code directly to `HdStorm` or `Hgi`.

This architecture works across platforms and accommodates arbitrary third-party delegates like [Blender Cycles](https://projects.staging.blender.org/bartvdbraak/blender/commit/04bb5f999501d97ab142eb2bce8f3a696bb06fa5?files=build_files), Arnold, or RenderMan.

---

1\. Best Graphics API Strategy Across Platforms

Qt 6 defaults to Direct3D 11/12 on Windows, OpenGL/Vulkan on Linux, and Metal on macOS. Because OpenUSD's `Hgi` supports **Vulkan, Metal, and OpenGL** (with no Direct3D backend), you should align Qt and OpenUSD on native modern APIs:

Universal Scene Description +1

<table><tbody><tr><th colspan="undefined">Operating System</th><th colspan="undefined">Qt RHI Backend</th><th colspan="undefined">OpenUSD <code>Hgi</code> Backend</th><th colspan="undefined">Why This Pairing Works Best</th></tr><tr><td colspan="undefined"><strong>Windows & Linux</strong></td><td colspan="undefined"><code>QSGRendererInterface::Vulkan</code></td><td colspan="undefined"><code>HgiVulkan</code></td><td colspan="undefined">Avoids legacy OpenGL context-sharing headaches and driver bugs; both Qt and OpenUSD <a href="https://www.khronos.org/blog/vulkan-support-added-to-openusd-and-pixars-hydra-storm-renderer">HgiVulkan</a> have mature Vulkan support.</td></tr><tr><td colspan="undefined"><strong>macOS</strong></td><td colspan="undefined"><code>QSGRendererInterface::Metal</code></td><td colspan="undefined"><code>HgiMetal</code></td><td colspan="undefined">Apple deprecated OpenGL; Metal is the native default for Qt 6 on macOS and is well-supported in OpenUSD.</td></tr></tbody></table>

- **Force the Backend Early.** Call `QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan)` (or `Metal` on macOS) in `main.cpp` before instantiating your `QQuickView` or `QQmlApplicationEngine`.
- **Let `Hgi` Own the Device.** Create your `Hgi` instance (`Hgi::CreatePlatformDefaultHgi()`) first so it initializes the native `VkDevice` or `MTLDevice` with all extensions required by Hydra. Pass that device to Qt Quick via `QQuickWindow::setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(...))`.
	Qt Forum +1

---

2\. Supporting Third-Party Render Delegates (Beyond `HdStorm`)

Third-party render delegates fall into two categories: **GPU delegates** (like `HdStorm` or MaterialX/Vulkan delegates) that use `Hgi`, and **CPU/External delegates** (like Cycles, Embree, or RenderMan) that render into system memory or external GPU buffers without touching `Hgi`.

To support **all** of them cleanly inside `QQuickRhiItem`, structure your `UsdImagingGLEngine` integration to inspect the output AOV dynamically:

1. **Discover Delegates via `HdRendererPluginRegistry`.** Do not hardcode `HdStorm`. Query `HdRendererPluginRegistry::GetInstance().GetPluginDescs()` to populate a renderer selector in your QML UI, and switch delegates at runtime via `engine->SetRendererPlugin(pluginId)`.
2. **Pass `HdDriver` unconditionally.** Always pass your `Hgi` instance inside an `HdDriver` (`{HdTokens->drivers, VtValue(HdDriver{HgiTokens->renderDriver, VtValue(hgi.get())})}`) when constructing `UsdImagingGLEngine`. GPU delegates like `HdStorm` will use it, while CPU delegates like Cycles or Embree will safely ignore it.
3. **Implement a Dual-Path AOV Importer.** After calling `engine->Render(...)`, retrieve the color `HdRenderBuffer` via `engine->GetAovBuffer(HdAovTokens->color)` and check how its memory is backed:
	- **Fast Path (Zero-Copy GPU Texture):** Inspect `renderBuffer->GetResource(false)`. If it holds a valid `HgiTextureHandle`, extract the native `VkImage` or `id<MTLTexture>` and wrap it in a `QRhiTexture` using `QRhiTexture::createFrom()`.
		- **Fallback Path (CPU / External Delegate):** If `GetResource(false)` is empty (which happens with CPU path tracers like Cycles or Embree), call `renderBuffer->Map()` to get the raw `void*` pixel pointer, upload it into a standard `QRhiTexture` via `QRhiResourceUpdateBatch::uploadTexture()`, and call `renderBuffer->Unmap()`.
		forum.aousd.org

---

3\. `QQuickRhiItem` Implementation Flow

Your custom `QQuickRhiItemRenderer` subclass acts as the bridge between `UsdImagingGLEngine` and QML's scene graph:

- **`initialize(QRhiCommandBuffer *cb)`:** Create a simple fullscreen quad vertex buffer, a sampler (`QRhiSampler`), and a graphics pipeline (`QRhiGraphicsPipeline`) with a fragment shader that converts linear HDR (`Rgba16Float`) to sRGB and outputs premultiplied alpha.
- **`synchronize(QQuickRhiItem *item)`:** Safely transfer camera matrices, viewport dimensions, time codes, and stage mutations from your main-thread `QQuickRhiItem` properties into your renderer state.
- **`render(QRhiCommandBuffer *cb)`:**
	1. Call `engine->SetRenderViewport(...)` and `engine->Render(rootPrim, renderParams)` to advance Hydra and update the AOV.
		2. Resolve the color AOV into your `QRhiTexture` using the **Fast Path** or **Fallback Path** above.
		3. Begin a `QRhiRenderPass` on `renderTarget()`, bind your fullscreen quad pipeline and the AOV `QRhiTexture`, and draw the quad.

---

Important Synchronization Details

- **Vulkan Queue & Layout Tracking:** When sharing a `VkDevice` between `HgiVulkan` and `QRhi`, ensure both use the same graphics queue family and index (`QRhiVulkanNativeHandles`). Because `Hgi` and `QRhi` track `VkImageLayout` independently, ensure `Hgi` finishes its command submission and leaves the AOV image in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` before `QRhi` samples it.
	Qt Documentation +1
- **Progressive Rendering (Cycles / Arnold):** Path-tracing delegates refine the image over multiple passes even when the camera is stationary. In `render()`, check `engine->IsConverged()`. If it returns `false`, call `update()` on your `QQuickRhiItem` so QML continues scheduling frames until the render converges.

Would you like to see a minimal **C++ implementation skeleton** covering:

- Setting up `Hgi` device sharing with `QQuickGraphicsDevice` on **Vulkan and Metal**
- The **dual-path AOV extraction** (`GetResource` vs. `Map`) inside `QQuickRhiItemRenderer::render()`?

KI-Antworten können Fehler enthalten. [Weitere Informationen](https://support.google.com/websearch?p=aimode)

In Zwischenablage kopiertFehler beim Kopieren in die Zwischenablage. Versuche es später noch einmal.

## Geteilt

0 Dateien

- Qt Forum
	OpenUSD Hydra + Qt Quick/QRhi/Vulkan Integration, Architectural...
	11.09.2026 — How should we keep the image layout tracking consistent between QRhi and Hgi when sharing the same VkImage? How can we publish a...
	![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIgAAACICAMAAAALZFNgAAAAZlBMVEX///8AAADKysri4uLp6elra2uLi4tZWVn4+PhwcHDW1taenp42NjbS0tLs7Oz09PQcHBxUVFR9fX29vb1ISEiRkZFhYWGEhIQXFxcQEBCYmJguLi4+Pj7c3NzDw8NPT0+tra0mJiYdMqq2AAADIUlEQVR4nO2Z12KsIBBAQSyIBde26orl/3/yjgXLlsQkd9eXOQ+J4gJHYUCQEARBEARBEARBEARBEAT5DumtGCd6sJpS6tMJezQLTxHJKL0oFdSpPYu4F8c7QwTqFzb1usp0R5HGgUQeiJB5ghihNEiSJB8SYSk8lCrMRpHMJoqFNBZRV5ttwHJCPb/5nAjLDapFLFe2pLHJtXMCZpGUePZnRFRM7cQKJpEkJ9KVDjFcEid5YOYgUmWfEcmZkK2vmqmzWm2bSZeQ3HFIUjsxoU76iTgagqXwg5jeII7HcDFNTiT8ZxyOJSf1cPB+MloUZVn08Ic+b4H4ExoQqRs+UyOCIAiCIMgeVnlbKnGWiEH3dCeKlNHKqSJ5yFbkiSLWWXXvQJF7XoiwxrOzrmH6XAphTunNejLAhZh+pISCBRlk85Ixgaumsr3GJAd5KhK60RTM0XXuvMG4DBRuP6zCBKXLqovp/Bdaq+ttypWRsGqnEm7dH0RUMRTR92NBoRbpVD7vKt2JuOOBsx2N0s3xwd2VZyJwX44BKz/ZQIG5FqlLuNfcZq9FIisxOR82WkDlakjOGbinx0aEJyIJpe28/JRQOZtFoCJPjakvRHq9FxhD2lwAh1s5NmsYuqAVKGfZIurotDQPtq39QsTXuxdKP0egOto2IOJfY82Qh/s0WtbjopzqCeYtxy9FdISYEW035R/bb9pPesPdS+iny+XwNrXxj0RCf+0Y6vGJvxQp/Js/0Q87NSb0r/V6TaPwpyISQlk3E5TmHBWxiNTwqeh8vV7T8scifNNfQORyXGSH2qX8SiT/LyJsl7WmxVkiIcTRcgIh1D8TWTrgG0Xuo2Yc3M4Q4S0tzc3lsRV2IqxfI+GNIsMHi25z7D2IyHQzeL1RxFjm3OF+qXwQIdasB072G0XGGT1rgqC56o9JdyIwLdI4MZruMgzH7xMxN+8WFn8isnn5aL8f0FpyhKCMHve4eVfPLzh65hRlsd0g59l0PRamzp9H6fpEouU7gln2m3H6NzClvnzfDJU65+MfgiAIgiAIgiAIgiAIgiAIgiAI8nf+AR9uLNTAEz9MAAAAAElFTkSuQmCC)
- forum.aousd.org
	HdStorm + Vulkan + Qt QML - Hydra - Alliance for OpenUSD
	12.09.2026 — HgiVulkan creates the unique VkDevice/VkInstance/VkPhysicalDevice. borrowed as GpuTexture via HgiTexture\* opaque handle. handle gi...
	![](data:image/png;base64,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)
- Qt Documentation
	Grafiken - Qt Documentation
	Das Qt Rendering Hardware Interface (RHI) Die unterstützten Grafik-APIs sind: OpenGL (Version 2.1. Vulkan (Version 1.0 und höher);
	![](data:image/png;base64,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)
- The Khronos Group
	Adding Vulkan to Pixars Hydra Storm Renderer
	Build on OpenUSD o Extend Hydra Abstraction Layer o Enhance HdStorm renderer with capabilities for. Autodesk. Community partnershi...
	![](data:image/jpeg;base64,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)
- GitHub
	GitHub - ArcheGraphics/HydraViewer: openusd hydra with qt and metal · GitHub
	Project: HydraViewer (GitHub - ArcheGraphics/HydraViewer). Purpose: OpenUSD Hydra Storm renderer integration with Qt6 and Apple Me...
	![](data:image/png;base64,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)
- The Khronos Group
	Vulkan Support Added to OpenUSD and Pixar’s Hydra Storm Renderer
	July 25, 2024 — API: HgiVulkan backend added in OpenUSD 24.08, providing experimental Khronos Vulkan support for Pixar's Hydra Storm renderer and...
	![](data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wCEAAkGBwgHBgkIBwgKCgkLDRYPDQwMDRsUFRAWIB0iIiAdHx8kKDQsJCYxJx8fLT0tMTU3Ojo6Iys/RD84QzQ5OjcBCgoKDQwNGg8PGjclHyU3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3N//AABEIAIgAiAMBIgACEQEDEQH/xAAbAAACAwEBAQAAAAAAAAAAAAAABAIFBgMBB//EAEgQAAIBAwICBAoHBQUHBQAAAAECAwAEERIhBTEGEyJBFBdRVWFxgZKT0RYyUlSRodIHFSNCsTNDcoLwJERic7LB8TVFY4Oi/8QAGAEBAQEBAQAAAAAAAAAAAAAAAAECAwT/xAAhEQEAAwABBAMBAQAAAAAAAAAAAQIRIQMSIjETQVFxBP/aAAwDAQACEQMRAD8AvPHBD5tPxT+mjxwQ+bT8U/prEfRCf79F7h+de/Q6b7/H8M/Opo23jgh82n4p/TR44IfNp+Kf01ih0Mm+/R+4fnXv0KnPLiEQ/wDrPzppjaeOCHzafin9NHjgh82n4p/TWNHQmfzjF8M/OvfoRcecYvhn500bHxwQ+bT8U/po8cEPm0/FP6axq9Cbgkj94xZH/wAZ+dS+g1x5wi+GfnTTGw8cEPm0/FP6aPHBD5tPxT+mseOg1x5wi+GfnQeg9x5xi+GfnTRsPHBD5tPxT+mjxwQ+bT8U/prH/Qe484RfDPzrz6Dz+cYvhn500bHxwQ+bT8U/po8cEPm0/FP6axv0JuPOMXwz868PQqcf+4RfDPzpo2fjgh82n4p/TR44IfNp+Kf01ih0Mn+/R+4fnQehs/3+P3D86aNr44IfNp+Kf00Vh26ISoMm/jx/y2+dFNXGlsLlbuxS4kCwMSchcleePZXXLKA3NTyK7j8a63XDxa2jR20X8MHIVd8ZOaz3EZ7i2u7IwuygseWRqORkHy1yrfWl6JNtzUhKPLVVw/ivhpkWQNDIh5hjpbP9KbeSWM9stjyhq3wh0Sjy1LrgO+kBct9ts/4qu+ANwySKQ8XuWTJ0hQ7Db/LQJPKFYMCMrsRnuroJgeR29dO8Zh4VDZ6+EXrSBSAyNKW2J9O9Ucd02COsbY7bmgf630171lI+FN3O34mjwpvtv+JoHTL6aiZR5aTN0x/vG/Gom5Yc3b8TQNtMBzYZ9dQ6wE7mlTcPzLk+2g3LY3b86KZMvdmomUeWkZr1IlJllCj/AIjVZccd07W0ef8Aifl+FWITV8ZhgnIwO/NFY26vLi72uJWZfsnYUVcTWvubmew6PxXNvI3W9YEOslgRk8wfZVYnSVJjGeJWEcmliysh3BHfg1bF7Gbo6PDiwt1l7ZXmpzty9lUzWfR6Q4h4q0bdwkXy+wV565LW76deD2No4u7mC4mMSkausjxoPPBNPXznwUSwspUnGoYNecMt7a34HxeO2vYrlWiJOgjK9gjcVVcFjX90TYABMo3A3qTOVmWs2VnBrulmc8o0GBHgdrfy921Wq8B4p1mY7OYwZDCXrlBK4zqxj17VX8MXTHdgEkYXn6nr6faf+lR+i3H/AE1vp32ulq8sPZ8Itb+GK4tr2d4pELq2ACfQRiu6dHI2MR8KmGoeRflRwhTwrgFv4TNHIViEzEHAKkkj/Xortc3cq2U0tuqF43ZApIHazj2V6fj8d1wmZiXJuj0Yi1i5n2cJyX7QHkqEnR9Br/2yUFTjdRvtnyUyLq5uNFtFbTJBMVzd61GknTllAOeZNU863xvoIuH8RWS1th/FLnTq7mO/1hg47/ZXCJleTsvR9UMoF5IdCavqj0/Kl7vhFnaOq3HESmoasso5Zx/U86vL25jQ3bRlXVLQv2DttqqgPV9JLUT3EcsReMIIot9s6uZ9VarW99mGZ6kVnJdvo+vYHhLdrP8AIO6sbxe9mtb+e1iYBYnKatIy1bu6k4jBdCMqAEXWW0ZCqcZO1fO+PHPGr30zNW5pavsp1O4izs7anOpvKajXtFGxRRRQW9z4SlncQF8oxXK4+yc1UezIq+uWAd85w2G2FQ4Bwa34leLFc3JhkMmEi6strHrzt31zrP6zSJzHPg/FLexsOIQzrIXuYcJpAwO7fJ9Ip3o6ivwqQN2f4gJ/KmLmwtuA3LK85uIpQWEHg4wAdsaic91INfxxmXwW20BzupkOgeTsj51mYj06xpqe88DE4izIDHqcaguwzyOD5TWruOlc9rEtsyuhTQrA41KDjuxvzGfLv5aqIuC3E3DhcTTKFCaxGsalf7MOQxOTy2B9FarifRvhFtwt7o2oedAhMrM25yuTjNbyIjIY8p5lTLacLsrCOxSMXESHIEpKktkkFscxnupxbrh6JLN4GkTyOJHQ9vrXwMnHKlOlzW9jwd/BURZ+uRU7KnT2xqO/Ps6vxpw2VlJZ6zbr1vZyV2ydQBPozvXSaTz/AAmC0/HIII/4Vrgo4MaHAwmScbcvqiuUlraxPc2yNIOth0ozY0qDpOBvvy76ueM8D4VacOvbiSw6sxQFleWXOhsHGd/Ljas+PBJZZ3eSPtSARuyggqAAMfhXGIS0THpLh0lmOE8Wt7SbrDFBJvp055529eaS4Fp/dUGvSBpGNT4rtepBDbXMeuMTGMgIseg8tq79GFtILRI+KTeDARjJ2HPbyEcq9PQmtay8vVi17Q9j4m0NzA0qdfbxuQYi57YPdkj1VhuOkNxq/YLgGdsAd2/Ktnx/ivDChWIqVhfRGYSAWG51Me/l3Y51h+JyCbiNzMNlkkLAE92adS1bTGO3Spasc+i1FHtpiyjSQuWAOBXOeI13pWbzkF6KfaGI8kH50Vnvh1n/ADXWEmDHEx+yRT3R4heN2GgEds5xSbAiFww3R8n/AF7Kf6NmFeJW+tMuJRhidhXL6h5aTyn0zOb6H/lY/M1RCzuZLZ7lIX6lNmkIwPVWs6Y20YtIbormbXo9HMnl7aQvONG74Mtl1TB9MY1lhjKnc49Vax20zFxdk4RCtx18cEpCBgigZMejOeeKtp+mdnxTgs1jDbXMlyIQsnZ0qHGO8+rb0VhmUyKqvqZVOV9FMwTS28Sx25EaDkAtbznUPcbnHEPCLiSxWOTSNLMQWXtZ5jy8vbVw3GbaOKaG4eFU7mMyrnDZI5+TGKzklzcSxsjNsRvlarLfg9pIiyyXUMJO5j6tiwP4Y/8AFam8+2qU7+Gq470z4NxThV1Z2lrcKzOmktLn6kgJO7bg6aqbY2wZJQkzKpDKrNsDn10nb2Fjb3MUkU1xNMkisirEFDMDsOZ2NWXHYUHFZxHHoUBcaRt9UViLTuQt6RVPid94dfT3JVgJMYVh9XAFJmSMqVfUwPPcVwaId5/KvOq9H5VMlnx/E5mgLDq4urXAyBuPXS7iIjSUz61zU1hXUBgHfyUxxaBDxG7Coir1rYAGMb1JryvdCseC3b6pZT343Fc0ZLbUM6w3sNdWiIGmaPrUHeuzD21a8S4day9G+DokyRYkuGEkiHU31djjPlqTOe3WsZzWVOLhGIzkHPfRXjcMSP6nEoTtsCrkf0ryteLp8nUjjIlcyNqeePOxBI9ec/OucUjpOJU7LAgqT5RSA43Y8+rnzyzoHzr3992I/kuPhj51Yq8MVheXnELy/i6u5kBTWXwFAyTSiR420KB6aQHHrL7E/uD517+/rI81uPcHzrTSx6teRwfQpoEZ/u0C+mq8cfsVOeruD/kHzqX0hsD/AHdx7g+dA8yArltzXkcXPAB3I3pE8e4d9m4+GPnRF0gskTDR3GrO/ZHzoH9BhbrFwrIylT5Dzp/jCE8SlIwdl/6RVBNx+wdCBHcZ2/kHl9dM3nSbh0ty8kcdyVYDmg7gB5amcrEmTGM5dRXjIP5So9lInpDYHnHce6PnXg4/w/7Fz7g+dVDwi7z3d9E5aR2ctqdjknApE9IbLH9lce4PnUP39Yk/2dwP8g+dA4sfZyQDT3ElH7m4TGRtm4I95apf39YAEaJz/lHzrtd9JLGWzs4Vjn1QdZqBQY7TAjG/oqTHJA6gaTlV5cxRSv76se5bj3B86KuIzuaM1tR+xrpEf98sPeavfEz0j++WHvNVGJ3o3ra+JjpH99sfeajxMdI/vtj7zUGKzXqEF1DkhNQzjmK2niZ6R/fLD3mp+D9k/GYra2jePhcrxTdZI5kcGVfsHAzj20SZl8+PVliVZ9OTzUZxXh0nYFs/4RW8n/ZBx17lpI34bFE0gcRK7nAHNQx3wf8AvTF3+ybitxNdvDb8Mt1mRRCiTykW7AfWGeefIafSvnZxg9o58mKiO7Ga+gn9kfGD1g08Mw1uYh/Gk7Lk/wBp6/RUR+yHjmYiH4dhYyrgyMdbfa5beoVcg+2Azvijf/vW+P7IOOGC2jMnD9cTMZX1tmUE5Gdu7l38q9X9kPGSigSWAk64uziVtkzsgGO7y0TZYCjNbu8/Y5xyW6ke3uLKKInKIZGOkevFcvEz0j++2HvNUVic0ZrbeJnpH9+sPxajxM9I/vth7zUGJzRW18TPSIf75Ye81FB9Ut+lenQLyykEjHSyx7NE5ZFVGD4OS0iqCMg5BzjcNSdKrVUZltLuQKO0UCdk6S5X63MKMn0Y8orxpujkcfVM1oFLtFp5nUDkr6+zn0afRU7m54Ra3AtJII1AGp20gImUbmf8CH2Y8ooOv0gtxatO1tcqVm6kxFV1Z06888Y0nPP0c9qXTpXZPHHItvdaXBY5VRoXKjUe136xgDJ57ZFRHG+CG8gtotMjStqRkjyutiy/iSrj2HNdbafggs7NwkMKXMSSRK4wcbMufUce0UHA9K7dbxons7lYyI+rbSpL6tWTgHYABT5e0NqnJ0ngGGS1uHVg7IAEBdRq7Yy2NJCk5ONseWoG86Nw26SRLbMryh0WNN2fsjP/AOk57cq7TXHB7e7ks5oIk6m17blOwkeMaM+o8vIfTQA6TW8iX729tNKtnD1jkFBqPcgBOx2P1sd3lrgOlcEaym6sriNlZhGg0MZAGKnHa23B54zjbNc4ZejvDVk6mUOt8+HQuWByWJODsMdsnvwD5KZa64BLEUXwaRdKgrgDs59PkLfnQHEuPvbXdrFbWkk4lRJGHVvlVZgMkgYXAJbtY5GofS2xjg1yRzal0alCqMFl1ADJGTpycDf8qYHFOGTR3Lwp1q5jt2Kp/aFvqqB/m9WDnlS3hnRuTHZtmXs6Sq51ZPdjf+YejtCg6X3SaG34eb5badoTcCFM4HWbkFhvsARzbAHM7b1FekjJdeCz2UjyDsloHUrryo0jUQTuSM7DsnOKi/HeB+ApIwBiVZGWLq8ldKFm2/w59eatW4ZYSRhTaxFcYA08t8/13oKxuk8ENxNHcW08aKP4bYX+IRo1LjVsQZAN9vTTNj0gtb+5jghguRrA/iMqhQ2gPpO+c6SDyxuN6dPDLEoE8Fi0hSoAUDAJBOPJuq/gKnFZW0JBigjTHLSoHcB/QAeygoY+laRySLd27BtZ0RxAs2gfz74BXBUgqTnVjnTy9IIBbiae3miVpZY0zpOoRhiW2OwOnbPeQKaj4Pw6NFRLOFVVtQAXv/13VOThtlIiI9rEyI5dVK7BjzNBVydJ7QdaRFPmKXQVKjU2x5DOdyBjOM6geRzRVmvC7FDlbWIdoN9XvByPz3ooK5Oi3C0gaGOOZUdlL4mbtYXTvk/nzzk5yc12n4DaXU91Lcl3699ekMVC5jVDy5/UBoooIt0ft0hYWss0M+OxNrLMp7e++39458mTXkvRzhspt9UUipbxLFGiSEAKoIX8AT+R5gGiig6ngNiZklCyqVbUQshAfcEAjvAwPwqB6OcLN0brwb/aGl615snXIdSt2jzIBRQByAAHLaiigg/RnhruC6zEBmfT1rY1srKW9eHYVObo9w6ZpTMkrrIznQZGwus5fHkyd6KKCQ4JAqxBJJtS3Jnd3cs0jdWY9z3dkjGOWBSVx0Vt5L+2uIJ3hWBQmjBbYaRtk43CKNwcYyMHevKKBmfo7ZurKhkjD4D9ssGTSqFNzsCqAe0nmatooxEpUMzDJOXOeZzRRQdaKKKAooooCiiig//Z)
- Universal Scene Description
	HdStorm: Real-time Hydra renderer plugin
	Storm supports OpenGL, Metal and Vulkan graphics APIs by utilizing the Hydra graphics interface (Hgi). This is a thin plugin layer...
- Qt Documentation
	QRhiVulkanNativeHandles Struct | Qt GUI | Qt 6.12.0
	When wanting to import not just a physical device, but also use an already existing VkDevice, set this and the graphics queue inde...

Alle anzeigen

yes please!

### Antwort des KI-Modus für „yes please!“

Below is a complete, minimal C++ skeleton demonstrating how to share an OpenUSD `Hgi` device with Qt Quick (`Vulkan` on Windows/Linux, `Metal` on macOS) and render any Hydra 2.0 delegate inside a `QQuickRhiItem` using a dual-path AOV handoff.

---

1\. `main.cpp`: Aligning Graphics APIs & Device Sharing

Create `Hgi` before initializing your QML window so that Qt RHI adopts the exact same native GPU device (`VkDevice` or `MTLDevice`) that `HdStorm` and other GPU delegates use.

```
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickGraphicsDevice>

#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>

#if defined(Q_OS_MACOS)
#include <pxr/imaging/hgiMetal/hgi.h>
#else
#include <pxr/imaging/hgiVulkan/hgi.h>
#include <pxr/imaging/hgiVulkan/device.h>
#include <pxr/imaging/hgiVulkan/instance.h>
#endif

PXR_NAMESPACE_USING_DIRECTIVE

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // 1. Force Qt Quick to match OpenUSD's native Hgi backend
#if defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
#endif

    // 2. Instantiate OpenUSD's platform default Hgi (HgiMetal or HgiVulkan)
    HgiUniquePtr hgi = Hgi::CreatePlatformDefaultHgi();

    QQmlApplicationEngine engine;
    // Register your custom QQuickRhiItem (e.g., HydraViewportItem) and pass \`hgi.get()\`
    // qmlRegisterType<HydraViewportItem>("Hydra", 1, 0, "HydraViewport");

    engine.loadFromModule("YourApp", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    // 3. Pass Hgi's native GPU device to Qt Quick
#if defined(Q_OS_MACOS)
    auto *hgiMetal = static_cast<HgiMetal *>(hgi.get());
    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndCommandQueue(
        hgiMetal->GetPrimaryDevice(), hgiMetal->GetQueue()));
#else
    auto *hgiVk = static_cast<HgiVulkan *>(hgi.get());
    HgiVulkanDevice *vkDevice = hgiVk->GetPrimaryDevice();
    HgiVulkanInstance *vkInstance = hgiVk->GetVulkanInstance();

    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        vkDevice->GetVulkanPhysicalDevice(),
        vkDevice->GetVulkanDevice(),
        vkDevice->GetGfxQueueFamilyIndex(),
        0 // queueIndex
    ));
#endif

    return app.exec();
}
```

*(Note: On Vulkan, you must also attach the `QVulkanInstance` wrapping `vkInstance->GetVulkanInstance()` to `window->setVulkanInstance(...)` before the window initializes.)*

---

2\. `HydraRhiRenderer.h / .cpp`: Dual-Path AOV Extraction

Inside `QQuickRhiItemRenderer::render()`, invoke `UsdImagingGLEngine::Render()`, inspect the resulting color `HdRenderBuffer`, and choose between a **zero-copy GPU wrap** (`createFrom`) or a **CPU upload** (`Map` / `Unmap`).

```
#include <QQuickRhiItem>
#include <rhi/qrhi.h>

#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hgi/texture.h>

PXR_NAMESPACE_USING_DIRECTIVE

class HydraRhiRenderer : public QQuickRhiItemRenderer {
public:
    HydraRhiRenderer(Hgi *hgi) : _hgi(hgi) {}

    void initialize(QRhiCommandBuffer *cb) override {
        if (!_engine) {
            // Enable Hydra 2.0 Scene Index architecture
            UsdImagingGLEngine::Parameters params;
            params.driver = HdDriver{HgiTokens->renderDriver, VtValue(_hgi)};
            params.allowAsynchronousSceneProcessing = true;

            _engine = std::make_unique<UsdImagingGLEngine>(params);
            // Default to HdStorm; can be changed dynamically via _engine->SetRendererPlugin(id)
        }
        // Initialize your fullscreen quad pipeline, sampler, and sRGB/premul-alpha shader here
    }

    void synchronize(QQuickRhiItem *item) override {
        // Safely copy stage pointer, camera matrices, viewport size, and requested renderer plugin ID
        // from the main-thread QQuickRhiItem into renderer member variables.
    }

    void render(QRhiCommandBuffer *cb) override {
        if (!_engine || !_stage) return;

        const QSize size = renderTarget()->pixelSize();
        _engine->SetRenderViewport(GfVec4d(0, 0, size.width(), size.height()));
        _engine->SetRendererAov(HdAovTokens->color);

        UsdImagingGLRenderParams renderParams;
        renderParams.frame = _currentTimeCode;
        renderParams.clearColor = GfVec4f(0.1f, 0.1f, 0.1f, 1.0f);
        renderParams.enableLighting = true;

        // 1. Execute Hydra 2.0 Scene Index evaluation & Render Delegate pass
        _engine->Render(_stage->GetPseudoRoot(), renderParams);

        // 2. Resolve the color AOV into a QRhiTexture
        HdRenderBuffer *colorAov = _engine->GetAovBuffer(HdAovTokens->color);
        if (colorAov) {
            colorAov->Resolve();
            updateRhiTextureFromAov(colorAov, cb);
        }

        // 3. Draw fullscreen quad in QML's render pass
        QRhiResourceUpdateBatch *u = rhi()->nextResourceUpdateBatch();
        cb->beginPass(renderTarget(), Qt::black, {1.0f, 0}, u);
        // Bind _quadPipeline, _aovRhiTexture, and draw(4)
        cb->endPass();

        // 4. Keep rendering if progressive path tracer (e.g., Cycles, Arnold) hasn't converged
        if (!_engine->IsConverged()) {
            update(); // Schedules another frame on QQuickRhiItem
        }
    }

private:
    void updateRhiTextureFromAov(HdRenderBuffer *aov, QRhiCommandBuffer *cb) {
        const QSize texSize(aov->GetWidth(), aov->GetHeight());
        const QRhiTexture::Format rhiFmt = toRhiFormat(aov->GetFormat()); // e.g., RGBA16F or RGBA8

        // PATH A: Fast Zero-Copy GPU Texture (HdStorm, Vulkan/Metal delegates)
        VtValue resource = aov->GetResource(false);
        if (resource.IsHolding<HgiTextureHandle>()) {
            HgiTextureHandle hgiTex = resource.UncheckedGet<HgiTextureHandle>();
            if (hgiTex) {
                quint64 nativeHandle = hgiTex->GetRawResource(); // VkImage or id<MTLTexture>

                if (!_aovRhiTexture || _aovRhiTexture->pixelSize() != texSize || _isCpuBacked) {
                    _aovRhiTexture.reset(rhi()->newTexture(rhiFmt, texSize, 1));
                    _isCpuBacked = false;
                }
                // Wrap the native GPU texture without copying pixels
                _aovRhiTexture->createFrom({nativeHandle, 0 /* current layout */});
                return;
            }
        }

        // PATH B: Fallback CPU / External Delegate (Blender Cycles, Embree, RenderMan)
        void *mappedData = aov->Map();
        if (mappedData) {
            if (!_aovRhiTexture || _aovRhiTexture->pixelSize() != texSize || !_isCpuBacked) {
                _aovRhiTexture.reset(rhi()->newTexture(rhiFmt, texSize, 1));
                _aovRhiTexture->create();
                _isCpuBacked = true;
            }

            const size_t dataSize = texSize.width() * texSize.height() * HdDataSizeOfFormat(aov->GetFormat());
            QByteArrayView pixelView(static_cast<const char *>(mappedData), dataSize);

            QRhiSubresourceUploadDescription subres(pixelView.data(), pixelView.size());
            QRhiTextureUploadEntry entry(0, 0, subres);
            QRhiTextureUploadDescription desc(entry);

            QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
            batch->uploadTexture(_aovRhiTexture.get(), desc);
            cb->resourceUpdate(batch);

            aov->Unmap();
        }
    }

    Hgi *_hgi = nullptr;
    UsdStageRefPtr _stage;
    UsdTimeCode _currentTimeCode = UsdTimeCode::Default();
    std::unique_ptr<UsdImagingGLEngine> _engine;
    std::unique_ptr<QRhiTexture> _aovRhiTexture;
    bool _isCpuBacked = false;
};
```

---

3\. Switching Delegates Dynamically from QML

Expose available plugins from `HdRendererPluginRegistry` to your QML UI so users can switch between `HdStorm`, Cycles (`HdCyclesRendererPlugin`), or any other installed Hydra plugin at runtime:

```
// Expose to QML via Q_INVOKABLE or a QAbstractListModel
QStringList HydraViewportItem::availableRenderers() const {
    QStringList names;
    HfPluginDescVector descs;
    HdRendererPluginRegistry::GetInstance().GetPluginDescs(&descs);
    for (const HfPluginDesc &desc : descs) {
        names << QString::fromStdString(desc.displayName); // Store desc.id alongside displayName
    }
    return names;
}

// Inside HydraRhiRenderer::synchronize():
if (_requestedPluginId != _activePluginId) {
    _engine->SetRendererPlugin(_requestedPluginId);
    _activePluginId = _requestedPluginId;
    _aovRhiTexture.reset(); // Force texture recreation for the new delegate
}
```

---

Key Build & Runtime Checklist

- **Vulkan Layout Synchronization:** In Path A on Vulkan, `HgiVulkan` leaves the color AOV texture in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` after `HdxColorCorrectionTask` or `Resolve()`. Pass `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` as the second argument to `_aovRhiTexture->createFrom({nativeHandle, layout})` so `QRhi` knows its current layout.
- **Shader Tone Mapping:** Because `HdStorm` outputs linear HDR values (`HdFormatFloat16Vec4`), your QML fullscreen quad fragment shader should apply sRGB gamma (`pow(color.rgb, vec3(1.0 / 2.2))`) and multiply `rgb *= color.a` so QML's scene graph composites transparency properly.
- **Plugin Discovery (`PXR_PLUGINPATH_NAME`):** To load third-party delegates like Blender's Cycles Hydra plugin (`hdCycles`), ensure `PXR_PLUGINPATH_NAME` points to the directory containing the delegate's `plugInfo.json` before `HdRendererPluginRegistry` is first queried.

If you'd like to continue building this out, I can provide:

- The **GLSL / Qt `.qsb` shader** for linear-to-sRGB tone mapping and premultiplied alpha compositing
- A **`CMakeLists.txt` template** linking Qt 6 (`Quick`, `GuiPrivate` for RHI headers) with OpenUSD (`usdImagingGL`, `hgiVulkan` / `hgiMetal`)

Which would be most helpful next?

KI-Antworten können Fehler enthalten. [Weitere Informationen](https://support.google.com/websearch?p=aimode)

In Zwischenablage kopiertFehler beim Kopieren in die Zwischenablage. Versuche es später noch einmal.

## Geteilt

0 Dateien

Alle anzeigen

The GLSL / Qt.qsb shader!

### Antwort des KI-Modus für „The GLSL / Qt.qsb shader!“

To display Hydra's AOV inside Qt Quick, you need a Vulkan-style GLSL vertex and fragment shader pair compiled into `.qsb` (Qt Shader Baker) binaries.

This setup handles three critical details automatically:

- **Cross-Platform UV Orientation:** OpenUSD renders with OpenGL-style bottom-left UV origins, whereas Vulkan and Metal use top-left origins. Passing `rhi()->isYUpInNDC()` via a uniform buffer flips the Y-axis dynamically.
- **Tone Mapping & sRGB Conversion:** Converts Hydra's linear HDR output (`RGBA16F`) into display-ready sRGB so your viewport isn't washed out or overly dark.
- **Premultiplied Alpha:** Qt Quick composites all items using premultiplied alpha. Multiplying `rgb * alpha` prevents dark fringes around transparent edges when layering QML UI over your 3D scene.

---

1\. Vertex Shader (`hydra_quad.vert`)

Generates a fullscreen triangle strip (`draw(4)` with no vertex buffer required) and passes corrected texture coordinates to the fragment shader.

```
#version 440

layout(location = 0) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    int flipY;       // Set to 1 if rhi()->isYUpInNDC() is false (Vulkan/Metal)
    int applyToneMap;// Set to 1 for HDR AOVs (RGBA16F), 0 if already sRGB (RGBA8)
};

void main() {
    // Fullscreen quad positions in Normalized Device Coordinates (-1 to 1)
    vec2 positions[4] = vec2,
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );

    vec2 uvs[4] = vec2,
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );

    vec2 uv = uvs[gl_VertexIndex];
    if (flipY == 1) {
        uv.y = 1.0 - uv.y;
    }

    vTexCoord = uv;
    gl_Position = mvp * vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
```

---

2\. Fragment Shader (`hydra_quad.frag`)

Samples the Hydra color AOV, applies ACES-style filmic tone mapping plus exact linear-to-sRGB gamma correction, and outputs premultiplied alpha.

```
#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    int flipY;
    int applyToneMap;
};

layout(binding = 1) uniform sampler2D aovTexture;

// Filmic tone mapping (Narkowicz ACES approximation) to prevent blown-out highlights
vec3 acesToneMap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Accurate piecewise linear-to-sRGB conversion
vec3 linearToSrgb(vec3 linearRGB) {
    bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = linearRGB * vec3(12.92);
    return mix(higher, lower, cutoff);
}

void main() {
    vec4 color = texture(aovTexture, vTexCoord);

    if (applyToneMap == 1) {
        color.rgb = acesToneMap(color.rgb);
        color.rgb = linearToSrgb(color.rgb);
    }

    // Qt Quick expects premultiplied alpha for scene graph compositing
    fragColor = vec4(color.rgb * color.a, color.a);
}
```

---

3\. Compiling to `.qsb` via CMake

Qt 6 provides `qt_add_shaders()` to automatically compile your GLSL files into SPIR-V, MSL (Metal), HLSL, and GLSL inside `.qsb` packages at build time:

```
qt_add_shaders(YourAppTarget "hydra_shaders"
    BATCHABLE
    PREFIX "/shaders"
    FILES
        shaders/hydra_quad.vert
        shaders/hydra_quad.frag
)
```

*(This embeds `:/shaders/hydra_quad.vert.qsb` and `:/shaders/hydra_quad.frag.qsb` directly into your application's Qt resource system.)*

---

4\. Wiring the Pipeline in `HydraRhiRenderer::initialize()`

Load the compiled `.qsb` files into `QRhiGraphicsPipeline` with no vertex input bindings (since vertices are generated in the shader via `gl_VertexIndex`):

```
QShader loadShader(const QString &name) {
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

// Inside HydraRhiRenderer::initialize(QRhiCommandBuffer *cb):
struct UniformBlock {
    float mvp[16];
    int flipY;
    int applyToneMap;
    float _pad[2]; // Keep 16-byte std140 alignment
};

_ubuf.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(UniformBlock)));
_ubuf->create();

_sampler.reset(rhi()->newSampler(
    QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
_sampler->create();

_srb.reset(rhi()->newShaderResourceBindings());
_srb->setBindings({
    QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, _ubuf.get()),
    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, _aovRhiTexture.get(), _sampler.get())
});
_srb->create();

_pipeline.reset(rhi()->newGraphicsPipeline());
_pipeline->setShaderStages({
    { QRhiShaderStage::Vertex,   loadShader(":/shaders/hydra_quad.vert.qsb") },
    { QRhiShaderStage::Fragment, loadShader(":/shaders/hydra_quad.frag.qsb") }
});
_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
_pipeline->setShaderResourceBindings(_srb.get());
_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
_pipeline->create();
```

Quick Usage Notes

- **Updating Uniforms:** Before `beginPass()`, upload `UniformBlock` with `mvp = rhi()->clipSpaceCorrMatrix()` (identity/correction matrix), `flipY = rhi()->isYUpInNDC() ? 0 : 1`, and `applyToneMap = (_aovRhiTexture->format() == QRhiTexture::RGBA16F) ? 1 : 0`.
- **Rebinding on Resize:** Whenever `_aovRhiTexture` is recreated (e.g., when the viewport resizes or you switch from `HdStorm` to `Cycles`), call `_srb->setBindings(...)` and `_srb->create()` so the shader samples the new texture handle.

If you'd like to wrap up the build setup, I can share the **`CMakeLists.txt` template** that cleanly finds and links both Qt 6 (`Quick`, `GuiPrivate`) and OpenUSD across Windows, Linux, and macOS. Would you like that next?

KI-Antworten können Fehler enthalten. [Weitere Informationen](https://support.google.com/websearch?p=aimode)

In Zwischenablage kopiertFehler beim Kopieren in die Zwischenablage. Versuche es später noch einmal.

## Geteilt

0 Dateien

Alle anzeigen

## Geteilt

0 Dateien

[Zum vorherigen Prompt](#)

Wird transkribiert...

Google-Apps

Google-Konto

Jürgen Riegel

juergen.riegel6820@googlemail.com