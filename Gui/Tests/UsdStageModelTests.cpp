// (c)2024 Juergen Riegel, mail@juergen-riegel.net
//
// Headless checks for UsdStageModel, the lazy shadow-tree behind the Scene
// Graph dock.  QAbstractItemModel works without a QApplication, so this runs
// under ctest like the App tests.  The focus is on the properties the big-tree
// research notes call out: laziness (bounded node count), chunked fetchMore,
// O(1) index/parent/path lookups, in-model filtering and live refresh through
// UsdNotice::ObjectsChanged.

#include <UsdStageModel.h>

#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primFlags.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>

#include <QModelIndex>
#include <QString>
#include <QVariant>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#ifndef DATA_DIR
#define DATA_DIR ""
#endif

namespace
{

using pxr::SdfPath;
using pxr::TfToken;
using pxr::UsdPrim;
using pxr::UsdStageRefPtr;

int failures = 0;

void check(bool condition, std::string_view what)
{
    std::cout << (condition ? "PASS  " : "FAIL  ") << what << std::endl;
    if (!condition)
        ++failures;
}

constexpr int kChildCount = 12000;

// Stage with one root prim and kChildCount Mesh children - the "one huge
// sibling group" case the notes single out.  The root layer identifier has to
// be a path OpenUSD may actually write to (CreateNew creates the file); the
// temp directory keeps the scratch layer out of the source tree.
UsdStageRefPtr makeBigStage()
{
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "UsdStageModelTests_inMemory.usda";
    std::error_code ec;
    std::filesystem::remove(scratch, ec);

    UsdStageRefPtr stage = pxr::UsdStage::CreateNew(scratch.generic_string());
    stage->DefinePrim(SdfPath("/Root"), TfToken("Xform"));
    for (int i = 0; i < kChildCount; ++i)
    {
        stage->DefinePrim(SdfPath(pxr::TfStringPrintf("/Root/child_%05d", i)),
                          TfToken("Mesh"));
    }
    return stage;
}

// Fully realizes a node's children (the view would do this while scrolling).
void fetchAll(UsdStageModel& model, const QModelIndex& parent)
{
    while (model.canFetchMore(parent))
        model.fetchMore(parent);
}

} // namespace

int main()
{
    // --- empty model ------------------------------------------------------
    {
        UsdStageModel model;
        check(model.rowCount() == 0, "no stage: no rows");
        check(!model.hasChildren(), "no stage: root has no children");
        check(!model.canFetchMore(), "no stage: nothing to fetch");
        check(model.columnCount() == 3, "three columns");
        check(model.headerData(UsdStageModel::NameColumn, Qt::Horizontal).toString() == "Name",
              "header 0 is Name");
    }

    const UsdStageRefPtr stage = makeBigStage();

    // --- stage swap realizes only the top level ---------------------------
    UsdStageModel model;
    model.setStage(stage);
    check(model.stage() == stage, "stage is retained");
    check(model.rowCount() == 1, "one top level prim");
    check(model.loadedNodeCount() == 2, "only pseudo root + /Root realized");

    const QModelIndex rootIdx = model.index(0, 0);
    check(rootIdx.isValid(), "index(0,0) valid");
    check(model.pathForIndex(rootIdx) == SdfPath("/Root"), "pathForIndex -> /Root");
    check(!model.parent(rootIdx).isValid(), "/Root has no visible parent row");
    check(model.data(rootIdx, Qt::DisplayRole).toString() == "Root", "display name");
    check(model.data(model.index(0, UsdStageModel::TypeColumn, QModelIndex()), Qt::DisplayRole)
              .toString() == "Xform", "type column");
    check(model.data(model.index(0, UsdStageModel::PathColumn, QModelIndex()), Qt::DisplayRole)
              .toString() == "/Root", "path column");
    check(model.data(rootIdx, UsdStageModel::PathRole).toString() == "/Root", "PathRole");
    check(model.data(rootIdx, UsdStageModel::TypeNameRole).toString() == "Xform", "TypeNameRole");
    check(!model.data(rootIdx, UsdStageModel::IsInstanceProxyRole).toBool(),
          "/Root is no instance proxy");

    // --- hasChildren must not realize the subtree -------------------------
    check(model.hasChildren(rootIdx), "hasChildren(/Root)");
    check(model.rowCount(rootIdx) == 0, "hasChildren does not materialize rows");
    check(model.canFetchMore(rootIdx), "12000 children pending -> canFetchMore");
    check(model.loadedNodeCount() == 2, "still only 2 nodes alive");

    // --- chunked fetchMore ------------------------------------------------
    model.fetchMore(rootIdx);
    check(model.rowCount(rootIdx) == UsdStageModel::chunkSize, "first chunk = chunkSize rows");
    check(model.loadedNodeCount() == 2 + UsdStageModel::chunkSize, "node count grew by one chunk");
    check(model.canFetchMore(rootIdx), "more chunks available");

    const QModelIndex deep = model.index(5000, 0, rootIdx);
    check(!deep.isValid(), "row 5000 not realized yet");

    fetchAll(model, rootIdx);
    check(model.rowCount(rootIdx) == kChildCount, "all 12000 children after fetchAll");
    check(!model.canFetchMore(rootIdx), "nothing left to fetch");
    check(model.loadedNodeCount() == 2 + kChildCount, "node count matches realized rows");

    const QModelIndex child = model.index(5000, 0, rootIdx);
    check(child.isValid(), "row 5000 realized");
    check(model.data(child, Qt::DisplayRole).toString() == "child_05000", "child name");
    check(model.parent(child) == rootIdx, "parent() is O(1) pointer lookup");
    check(model.pathForIndex(child) == SdfPath("/Root/child_05000"), "child path");

    // --- indexForPath stays lazy -----------------------------------------
    {
        UsdStageModel jumper;
        jumper.setStage(stage);
        const QModelIndex target = jumper.indexForPath(SdfPath("/Root/child_00042"));
        check(target.isValid(), "indexForPath resolved a deep prim");
        check(jumper.pathForIndex(target) == SdfPath("/Root/child_00042"), "round trip path");
        check(jumper.loadedNodeCount() < 100, "indexForPath only realized the path");
        check(!jumper.indexForPath(SdfPath("/Root/nope")).isValid(),
              "indexForPath of a missing prim is invalid");
    }

    // --- filtering happens inside the model ------------------------------
    model.setFilterText(QStringLiteral("child_00123"));
    check(model.matchCount() == 1, "exact name filter: one match");
    check(model.rowCount() == 1, "ancestor /Root stays visible");
    const QModelIndex filteredRoot = model.index(0, 0);
    fetchAll(model, filteredRoot);
    check(model.rowCount(filteredRoot) == 1, "filter prunes the other 11999 children");
    check(model.data(model.index(0, 0, filteredRoot), Qt::DisplayRole).toString() == "child_00123",
          "surviving child is the match");

    model.setFilterText(QStringLiteral("mesh"));
    check(model.matchCount() == kChildCount, "type filter matches all meshes");

    model.setFilterText(QString());
    check(model.matchCount() == 0, "empty filter clears matches");
    const QModelIndex clearedRoot = model.index(0, 0);
    check(model.rowCount(clearedRoot) == 0, "filter reset re-arms lazy fetching");
    model.fetchMore(clearedRoot);
    check(model.rowCount(clearedRoot) == UsdStageModel::chunkSize, "chunking survives a filter reset");

    // --- live refresh: UsdNotice::ObjectsChanged -------------------------
    const QModelIndex expandedRoot = model.index(0, 0);
    fetchAll(model, expandedRoot);
    check(model.rowCount(expandedRoot) == kChildCount, "fully expanded before the edit");

    // NOTE: no SdfChangeBlock here. This prebuilt OpenUSD 26.11 reports
    // "Failed to define UsdPrim" for *new* prims created while a change block
    // is already active (the spec is authored, only the immediate lookup
    // inside _DefinePrim fails). Re-check this when prim commands land.
    stage->DefinePrim(SdfPath("/Root/added_prim"), TfToken("Mesh"));
    const QModelIndex afterAdd = model.index(0, 0);
    check(model.rowCount(afterAdd) == kChildCount + 1, "added prim shows up without a model reset");
    check(model.indexForPath(SdfPath("/Root/added_prim")).isValid(), "added prim reachable by path");

    {
        pxr::SdfChangeBlock block;
        stage->RemovePrim(SdfPath("/Root/child_00001"));
    }
    const QModelIndex afterDelete = model.index(0, 0);
    check(model.rowCount(afterDelete) == kChildCount, "deleted prim disappears");
    check(!model.indexForPath(SdfPath("/Root/child_00001")).isValid(), "deleted prim unreachable");

    // A change under a collapsed node must not realize anything.
    UsdStageModel lazy;
    lazy.setStage(stage);
    stage->DefinePrim(SdfPath("/Root/late_child"), TfToken("Mesh"));
    check(lazy.rowCount() == 1, "collapsed stage still has one top level row");
    check(lazy.loadedNodeCount() < UsdStageModel::chunkSize,
          "notice on a collapsed subtree stays lazy");

    // --- instance proxies (Kitchen_set_instanced.usd) --------------------
    const std::string kitchen = std::string(DATA_DIR) + "/Kitchen_set/Kitchen_set.usd";
    if (UsdStageRefPtr kitchenStage = pxr::UsdStage::Open(kitchen))
    {
        UsdStageModel kitchenModel;
        kitchenModel.setStage(kitchenStage);
        check(kitchenModel.rowCount() >= 1, "Kitchen_set has top level prims");
    }
    else
    {
        std::cout << "SKIP  Kitchen_set.usd not found at " << kitchen << std::endl;
    }

    // The instanced variant of the same asset is the big-tree case: thousands
    // of instance proxies that do not exist in the stage namespace.
    const std::string instanced =
        std::string(DATA_DIR) + "/Kitchen_set/Kitchen_set_instanced.usd";
    if (UsdStageRefPtr instancedStage = pxr::UsdStage::Open(instanced))
    {
        UsdStageModel instancedModel;
        instancedModel.setStage(instancedStage);
        check(instancedModel.rowCount() >= 1, "Kitchen_set_instanced has top level prims");

        SdfPath proxyPath;
        for (const UsdPrim& prim : instancedStage->Traverse(
                 pxr::UsdTraverseInstanceProxies(pxr::UsdPrimIsDefined && !pxr::UsdPrimIsAbstract)))
        {
            if (prim.IsInstanceProxy())
            {
                proxyPath = prim.GetPath();
                break;
            }
        }
        check(!proxyPath.IsEmpty(), "Kitchen_set_instanced contains instance proxies");
        if (!proxyPath.IsEmpty())
        {
            // Proxies do not exist in the stage namespace: only a traversal
            // can find them, which is why the model caches prim handles.
            const QModelIndex proxy = instancedModel.indexForPath(proxyPath);
            check(proxy.isValid(), "model resolves the proxy path");
            check(instancedModel.data(proxy, UsdStageModel::IsInstanceProxyRole).toBool(),
                  "IsInstanceProxyRole set for a proxy row");
            check(instancedModel.data(proxy, Qt::DisplayRole).toString() ==
                      QString::fromStdString(proxyPath.GetName()),
                  "proxy row shows its name");
        }
    }
    else
    {
        std::cout << "SKIP  Kitchen_set_instanced.usd not found at " << instanced << std::endl;
    }

    std::cout << (failures == 0 ? "ALL OK" : "FAILURES") << " (" << failures << " failed)"
              << std::endl;
    return failures == 0 ? 0 : 1;
}
