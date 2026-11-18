// (c)2026 Juergen Riegel, mail@juergen-riegel.net
//
// Widget-level regression checks for the Scene Graph dock:
//   * BreadcrumbBar must tear down its "/" separators with the crumbs -
//     leaking them used to fill the bar with an ever-growing slash chain.
//   * default column widths only stick when applied after setModel(), and
//     the horizontal scrollbar has to appear once the columns no longer fit
//     the viewport (long prim paths stay readable by scrolling).

#include <SceneTreeView.h>
#include <UsdStageModel.h>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>

namespace
{

using pxr::SdfPath;
using pxr::TfToken;
using pxr::UsdStageRefPtr;

int failures = 0;

void check(bool condition, std::string_view what)
{
    std::cout << (condition ? "PASS  " : "FAIL  ") << what << std::endl;
    if (!condition)
        ++failures;
}

UsdStageRefPtr makeStage()
{
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "SceneTreeViewTests.usda";
    std::error_code ec;
    std::filesystem::remove(scratch, ec);

    UsdStageRefPtr stage = pxr::UsdStage::CreateNew(scratch.generic_string());
    stage->DefinePrim(SdfPath("/Root"), TfToken("Xform"));
    stage->DefinePrim(SdfPath("/Root/a"), TfToken("Xform"));
    stage->DefinePrim(SdfPath("/Root/a/b"), TfToken("Xform"));
    stage->DefinePrim(SdfPath("/Root/a/b/c"), TfToken("Mesh"));
    return stage;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const UsdStageRefPtr stage = makeStage();
    if (!stage)
    {
        std::cout << "FAIL  could not create scratch stage" << std::endl;
        return 1;
    }

    UsdStageModel model;
    model.setStage(stage);

    // --- default column widths survive setModel ---------------------------
    SceneTreeView view;
    view.setModel(&model);
    check(view.header()->sectionSize(0) == 280, "Name column width applied after setModel");
    check(view.header()->sectionSize(1) == 160, "Type column width applied after setModel");
    check(view.header()->sectionSize(2) == 900, "Path column is wide by default");

    // --- horizontal scrollbar shows exactly when the columns overflow -----
    view.resize(400, 300);
    view.show();
    QApplication::processEvents();
    check(view.horizontalScrollBar()->isVisible(),
          "horizontal scrollbar visible in a narrow view");

    view.resize(1700, 300);
    QApplication::processEvents();
    check(!view.horizontalScrollBar()->isVisible(),
          "horizontal scrollbar hidden when everything fits");

    // --- breadcrumb separators do not accumulate --------------------------
    // Heap + unique_ptr while top-level: once the wrapper takes parenting
    // ownership below, releasing the unique_ptr avoids a double destruction.
    auto bar = std::make_unique<BreadcrumbBar>();
    BreadcrumbBar* barPtr = bar.get();
    barPtr->setTreeView(&view);

    const QModelIndex leaf = model.indexForPath(SdfPath("/Root/a/b/c"));
    check(leaf.isValid(), "deep leaf resolved");

    barPtr->updateForIndex(leaf);
    // deleteLater() only runs on a DeferredDelete flush, otherwise the stale
    // crumb from setTreeView()'s initial update is still counted here.
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const int buttons = barPtr->findChildren<QToolButton*>().size();
    const int labels = barPtr->findChildren<QLabel*>().size();
    check(buttons == 4, "one crumb per ancestor plus the leaf");
    check(labels == 4, "three separators plus the title label");

    // Repeated selection updates must not leave stale "/" labels behind.
    barPtr->updateForIndex(leaf);
    barPtr->updateForIndex(leaf);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(barPtr->findChildren<QToolButton*>().size() == buttons,
          "crumb count stable across repeated updates");
    check(barPtr->findChildren<QLabel*>().size() == labels,
          "separator labels do not accumulate");

    // --- full path tooltip -------------------------------------------------
    check(barPtr->toolTip() == QStringLiteral("/Root/a/b/c"), "bar tooltip shows the full path");
    bool leafHasPath = false;
    for (auto* button : barPtr->findChildren<QToolButton*>())
        leafHasPath = leafHasPath || button->toolTip() == QStringLiteral("/Root/a/b/c");
    check(leafHasPath, "leaf crumb tooltip shows the full path");

    // --- the crumb row must not dictate the dock's minimum width ----------
    // The wrapper owns the bar from here on; releasing the unique_ptr keeps a
    // stack address from ever being deleted by a parent (heap corruption).
    QWidget wrapper;
    auto* box = new QVBoxLayout(&wrapper);
    box->setContentsMargins(0, 0, 0, 0);
    barPtr->setParent(&wrapper);
    box->addWidget(barPtr);
    bar.release();
    wrapper.resize(80, 120);
    wrapper.show();
    QApplication::processEvents();
    check(barPtr->width() <= 120, "breadcrumb clips instead of forcing the dock width");

    // --- crumbs stay left-aligned in a wide bar ----------------------------
    wrapper.resize(1000, 120);
    QApplication::processEvents();
    const auto crumbs = barPtr->findChildren<QToolButton*>();
    int rightMost = 0;
    for (auto* button : crumbs)
        rightMost = std::max(rightMost, button->geometry().right());
    check(!crumbs.isEmpty() && rightMost < barPtr->width() / 2, "crumb chain is left-aligned");

    std::cout << (failures == 0 ? "ALL OK" : "FAILURES") << " (" << failures << " failed)"
              << std::endl;
    return failures == 0 ? 0 : 1;
}
