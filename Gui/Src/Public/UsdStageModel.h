#pragma once

#include <pxr/base/tf/hash.h>
#include <pxr/base/tf/notice.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <QAbstractItemModel>
#include <QString>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Model behind the Scene Graph tree view.  It mirrors a UsdStage prim
// hierarchy without ever materializing the whole stage and follows the
// recommendations in
// Doc/USD-Studion-Research/Clippings/QT tree view tec for handling real big trees.md:
//   * a tiny "shadow" node per *visible* prim; QModelIndex::internalPointer()
//     only ever carries that node pointer (never a UsdPrim),
//   * index()/parent()/rowCount() are pointer or hash lookups, never traversals,
//   * children are enumerated on demand and created in chunks (canFetchMore /
//     fetchMore) so one huge sibling group cannot freeze the UI,
//   * hasChildren() is cached, so the view does not expand subtrees to test them,
//   * UsdNotice::ObjectsChanged updates the affected rows instead of resetting,
//   * filtering happens inside the model - a QSortFilterProxyModel over a lazy
//     model would force the whole stage to be realized.
class UsdStageModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum Column
	{
		NameColumn = 0,
		TypeColumn = 1,
		PathColumn = 2,
	};

	// Roles on top of DisplayRole / ToolTipRole / ForegroundRole.
	enum ExtraRole
	{
		PathRole = Qt::UserRole + 1, // full prim path, QString
		TypeNameRole,                // prim type name, QString
		IsInstanceProxyRole,         // true for instance proxies, bool
	};

	// Rows created per fetchMore() call.
	static constexpr int chunkSize = 4096;
	// Upper bound of collected search matches (keeps typing responsive).
	static constexpr int maxFilterMatches = 20000;

	explicit UsdStageModel(QObject* parent = nullptr);
	~UsdStageModel() override;

	// Swaps the observed stage.  Cheap: the previous shadow tree is dropped and
	// only the top level is realized.  Does nothing when the stage is unchanged.
	void setStage(const pxr::UsdStageRefPtr& stage);
	pxr::UsdStageRefPtr stage() const { return stage_; }

	// Case insensitive substring filter on prim name and type.  Matching prims
	// and their ancestors stay visible, everything else is pruned.  An empty
	// string shows the full hierarchy.
	void setFilterText(const QString& text);
	QString filterText() const { return filterText_; }
	int matchCount() const { return static_cast<int>(matches_.size()); }

	pxr::SdfPath pathForIndex(const QModelIndex& index) const;

	// Realizes the chain of nodes leading to path and returns its index.
	// Returns an invalid index when the prim does not exist or is filtered out.
	QModelIndex indexForPath(const pxr::SdfPath& path);

	// Number of shadow nodes currently alive - used by tests to prove the
	// tree really is lazy.
	int loadedNodeCount() const { return static_cast<int>(nodeByPath_.size()); }

	// QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
	void fetchMore(const QModelIndex& parent) override;

private:
	// Lightweight mirror of one prim.  Nodes are owned by their parent's
	// "children" vector, so the pointers handed to Qt stay stable.
	struct Node
	{
		pxr::SdfPath path;
		pxr::UsdPrim prim; // captured during traversal - also valid for instance proxies
		Node* parent = nullptr;
		int row = 0;       // row inside the parent's children
		std::vector<std::unique_ptr<Node>> children;

		// Children prims enumerated from the stage but not yet turned into
		// nodes.  children.size() always equals materialized.
		std::vector<pxr::UsdPrim> childPrims;
		size_t materialized = 0;
		bool childPrimsFetched = false;
	};

	// Registered with TfNotice so a stage edit can never call into a deleted
	// model.  Defined in the .cpp.
	class Listener;

	UsdStageModel::Node* nodeFromIndex(const QModelIndex& index) const;
	QModelIndex indexForNode(Node* node) const;

	void registerListener();
	void unregisterListener();
	void clearTree();
	void enumerateChildren(Node* node);
	void materializeUpTo(Node* node, size_t target);
	void removeChildren(Node* node);
	void dropSubtreeNodes(Node* node);
	void recomputeFilter();
	bool visibleByFilter(const pxr::SdfPath& path) const;
	void refreshChildren(Node* node);
	void emitRowChanged(Node* node);
	void onObjectsChanged(const pxr::UsdNotice::ObjectsChanged& notice);

	pxr::UsdStageRefPtr stage_;
	pxr::UsdStageWeakPtr stageWeak_;
	std::unique_ptr<Node> rootNode_;
	Node* root_ = nullptr;

	// Fast path lookup used by indexForPath() and the notice handler.
	std::unordered_map<pxr::SdfPath, Node*, pxr::TfHash> nodeByPath_;

	// Search state: matching prims, plus every ancestor of a match.
	std::unordered_set<pxr::SdfPath, pxr::TfHash> matches_;
	std::unordered_set<pxr::SdfPath, pxr::TfHash> ancestors_;
	std::string filterNeedle_;

	std::unique_ptr<Listener> listener_;
	pxr::TfNotice::Key noticeKey_;
	bool listenerRegistered_ = false;
	QString filterText_;

	// True while the whole model is being reset; row signals are suppressed.
	bool resetting_ = false;
};
