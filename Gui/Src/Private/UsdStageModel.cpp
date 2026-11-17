#include "../Public/UsdStageModel.h"

#include <pxr/base/tf/token.h>
#include <pxr/base/tf/weakPtr.h>
#include <pxr/base/tf/weakbase.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/primFlags.h>
#include <pxr/usd/usd/primRange.h>

#include <QColor>
#include <QVariant>

#include <algorithm>
#include <cctype>
#include <functional>

namespace
{
// Prims shown in the outliner: everything that is defined, plus the proxies
// instance/prototype prims generate.  Instance proxies do not exist in the
// stage namespace, so they can only be reached through a traversal predicate -
// UsdStage::GetPrimAtPath() will not find them.  Inactive prims stay visible
// (they are drawn dimmed) so the tree matches what the user expects to edit.
const pxr::Usd_PrimFlagsPredicate& outlinerPredicate()
{
	static const pxr::Usd_PrimFlagsPredicate predicate =
		pxr::UsdTraverseInstanceProxies(pxr::UsdPrimIsDefined && !pxr::UsdPrimIsAbstract);
	return predicate;
}

const pxr::SdfPath& absoluteRootPath()
{
	static const pxr::SdfPath path = pxr::SdfPath::AbsoluteRootPath();
	return path;
}

std::string lowered(const std::string& text)
{
	std::string out = text;
	std::transform(out.begin(), out.end(), out.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}
} // namespace

// ---------------------------------------------------------------------------
// Notice listener
// ---------------------------------------------------------------------------

class UsdStageModel::Listener : public pxr::TfWeakBase
{
public:
	explicit Listener(UsdStageModel* model)
		: model_(model)
	{
	}

	void objectsChanged(const pxr::UsdNotice::ObjectsChanged& notice)
	{
		model_->onObjectsChanged(notice);
	}

private:
	UsdStageModel* model_;
};

// ---------------------------------------------------------------------------
// Lifetime / stage handling
// ---------------------------------------------------------------------------

UsdStageModel::UsdStageModel(QObject* parent)
	: QAbstractItemModel(parent)
{
	listener_ = std::make_unique<Listener>(this);
}

UsdStageModel::~UsdStageModel()
{
	unregisterListener();
}

void UsdStageModel::registerListener()
{
	if (!stage_ || !listener_ || listenerRegistered_)
		return;

	noticeKey_ = pxr::TfNotice::Register(
		pxr::TfWeakPtr<Listener>(listener_.get()),
		&Listener::objectsChanged,
		stageWeak_);

	listenerRegistered_ = true;
}

void UsdStageModel::unregisterListener()
{
	if (!listenerRegistered_)
		return;

	pxr::TfNotice::Revoke(noticeKey_);
	listenerRegistered_ = false;
}

void UsdStageModel::setStage(const pxr::UsdStageRefPtr& stage)
{
	const pxr::UsdStageWeakPtr newWeak(stage);
	if (newWeak == stageWeak_)
		return;

	unregisterListener();

	resetting_ = true;
	beginResetModel();

	clearTree();
	stage_ = stage;
	stageWeak_ = newWeak;
	matches_.clear();
	ancestors_.clear();

	if (stage_)
	{
		rootNode_ = std::make_unique<Node>();
		root_ = rootNode_.get();
		root_->prim = stage_->GetPseudoRoot();
		root_->path = absoluteRootPath();
		nodeByPath_.emplace(root_->path, root_);

		recomputeFilter();
		enumerateChildren(root_);
		materializeUpTo(root_, std::min<size_t>(root_->childPrims.size(), chunkSize));
	}

	endResetModel();
	resetting_ = false;

	registerListener();
}

void UsdStageModel::clearTree()
{
	unregisterListener();
	if (root_)
		dropSubtreeNodes(root_);
	rootNode_.reset();
	root_ = nullptr;
	nodeByPath_.clear();
}

// ---------------------------------------------------------------------------
// Lazy realization
// ---------------------------------------------------------------------------

void UsdStageModel::enumerateChildren(Node* node)
{
	if (!node)
		return;

	node->childPrims.clear();
	node->materialized = 0;
	node->childPrimsFetched = true;

	if (!stage_ || !node->prim)
		return;

	// One USD enumeration per expand; the result is cached and reused by every
	// following fetchMore(), which is what keeps repeated expansion cheap.
	for (const pxr::UsdPrim& child : node->prim.GetFilteredChildren(outlinerPredicate()))
	{
		if (visibleByFilter(child.GetPath()))
			node->childPrims.push_back(child);
	}
}

void UsdStageModel::materializeUpTo(Node* node, size_t target)
{
	if (!node)
		return;

	if (!node->childPrimsFetched)
		enumerateChildren(node);

	target = std::min(target, node->childPrims.size());

	while (node->materialized < target)
	{
		const size_t first = node->materialized;
		const size_t last = std::min(target, first + static_cast<size_t>(chunkSize)) - 1;

		// Insert the whole chunk as a single batch so the view repaints once.
		const QModelIndex parentNode = indexForNode(node);
		if (!resetting_)
			beginInsertRows(parentNode, static_cast<int>(first), static_cast<int>(last));

		for (size_t i = first; i <= last; ++i)
		{
			auto child = std::make_unique<Node>();
			child->prim = node->childPrims[i];
			child->path = child->prim.GetPath();
			child->parent = node;
			child->row = static_cast<int>(i);
			Node* raw = child.get();
			nodeByPath_.emplace(child->path, raw);
			node->children.push_back(std::move(child));
		}

		node->materialized = last + 1;
		if (!resetting_)
			endInsertRows();
	}
}

void UsdStageModel::removeChildren(Node* node)
{
	if (!node)
		return;

	if (!node->children.empty())
	{
		const QModelIndex parentNode = indexForNode(node);
		if (!resetting_)
			beginRemoveRows(parentNode, 0, static_cast<int>(node->children.size()) - 1);

		// Only the child subtrees go away - "node" itself stays alive and must
		// keep its entry in nodeByPath_.
		for (const auto& child : node->children)
			dropSubtreeNodes(child.get());

		node->children.clear();
		node->materialized = 0;

		if (!resetting_)
			endRemoveRows();
	}

	node->childPrims.clear();
	node->childPrimsFetched = false;
}

void UsdStageModel::dropSubtreeNodes(Node* node)
{
	if (!node)
		return;

	for (const auto& child : node->children)
		dropSubtreeNodes(child.get());

	nodeByPath_.erase(node->path);
}

void UsdStageModel::refreshChildren(Node* node)
{
	if (!node)
		return;

	// Rebuilding replaces the node objects below "node", so their UsdPrim
	// handles are refreshed as well - a handle that survived a resync must
	// never be used for display.
	const bool wasFetched = node->childPrimsFetched;
	const bool fullyShown = wasFetched && node->materialized >= node->childPrims.size();
	const size_t shown = node->materialized;

	removeChildren(node);

	// A node the view never looked at stays untouched: a change deep in a
	// collapsed subtree must not realize thousands of rows.  fetchMore() will
	// pick up the current state when the user expands it again.
	if (wasFetched)
	{
		enumerateChildren(node);
		// A fully expanded node keeps showing everything (new prims appear
		// right away); a partially fetched one keeps its chunk and lets
		// fetchMore() bring the rest.
		materializeUpTo(node, fullyShown ? node->childPrims.size() : shown);
	}

	if (node != root_)
		emitRowChanged(node);
}

void UsdStageModel::emitRowChanged(Node* node)
{
	if (!node)
		return;

	const QModelIndex first = indexForNode(node);
	if (!first.isValid())
		return;

	const QModelIndex last = index(std::max(first.row(), 0), PathColumn, first.parent());
	Q_EMIT dataChanged(first, last.isValid() ? last : first,
		{ Qt::DisplayRole, Qt::ToolTipRole, Qt::ForegroundRole, PathRole, TypeNameRole });
}

// ---------------------------------------------------------------------------
// Filtering (inside the model on purpose - no QSortFilterProxyModel)
// ---------------------------------------------------------------------------

void UsdStageModel::setFilterText(const QString& text)
{
	if (text == filterText_)
		return;

	filterText_ = text;

	resetting_ = true;
	beginResetModel();

	if (root_)
	{
		removeChildren(root_);
		recomputeFilter();
		enumerateChildren(root_);
		materializeUpTo(root_, std::min<size_t>(root_->childPrims.size(), chunkSize));
	}

	endResetModel();
	resetting_ = false;
}

void UsdStageModel::recomputeFilter()
{
	matches_.clear();
	ancestors_.clear();
	filterNeedle_.clear();

	if (!stage_)
		return;

	filterNeedle_ = lowered(filterText_.trimmed().toStdString());
	if (filterNeedle_.empty())
		return;

	int found = 0;
	for (const pxr::UsdPrim& prim : stage_->Traverse(outlinerPredicate()))
	{
		if (lowered(prim.GetName().GetString()).find(filterNeedle_) == std::string::npos &&
			lowered(prim.GetTypeName().GetString()).find(filterNeedle_) == std::string::npos)
			continue;

		const pxr::SdfPath& path = prim.GetPath();
		matches_.insert(path);
		for (const pxr::SdfPath& prefix : path.GetPrefixes())
		{
			if (prefix != path && prefix != absoluteRootPath())
				ancestors_.insert(prefix);
		}

		if (++found >= maxFilterMatches)
			break;
	}
}

bool UsdStageModel::visibleByFilter(const pxr::SdfPath& path) const
{
	if (filterNeedle_.empty())
		return true;

	return matches_.contains(path) || ancestors_.contains(path);
}

// ---------------------------------------------------------------------------
// Stage change handling
// ---------------------------------------------------------------------------

void UsdStageModel::onObjectsChanged(const pxr::UsdNotice::ObjectsChanged& notice)
{
	if (resetting_ || !stage_ || !root_)
		return;

	if (notice.GetStage() != stageWeak_)
		return;

	// Attribute-only changes: touch the rows that are already on screen.
	for (const pxr::SdfPath& path : notice.GetChangedInfoOnlyPaths())
	{
		const auto it = nodeByPath_.find(path);
		if (it != nodeByPath_.end())
			emitRowChanged(it->second);
	}

	// Structural changes: mark the *loaded* nodes whose child list is affected.
	std::vector<pxr::SdfPath> dirty;
	auto markDirty = [&dirty](const pxr::SdfPath& path)
	{
		if (std::find(dirty.begin(), dirty.end(), path) == dirty.end())
			dirty.push_back(path);
	};

	for (const pxr::SdfPath& path : notice.GetResyncedPaths())
	{
		if (path == absoluteRootPath())
		{
			root_->prim = stage_->GetPseudoRoot();
			markDirty(root_->path);
			continue;
		}

		const auto it = nodeByPath_.find(path);
		if (it != nodeByPath_.end())
		{
			Node* node = it->second;
			if (node->parent)
				markDirty(node->parent->path);
			if (!node->children.empty())
				markDirty(node->path);
		}
		else
		{
			// Added or re-parented prim: the closest loaded ancestor has to
			// re-enumerate its children.
			for (pxr::SdfPath parent = path.GetParentPath(); !parent.IsEmpty(); parent = parent.GetParentPath())
			{
				if (nodeByPath_.contains(parent))
				{
					markDirty(parent);
					break;
				}
				if (parent == absoluteRootPath())
					break;
			}
		}
	}

	// Shallowest first: refreshing a parent deletes the nodes below it, so a
	// deeper entry may legitimately be gone afterwards.
	std::sort(dirty.begin(), dirty.end(), [](const pxr::SdfPath& a, const pxr::SdfPath& b)
		{
			const size_t countA = a.GetPathElementCount();
			const size_t countB = b.GetPathElementCount();
			return countA != countB ? countA < countB : a < b;
		});

	for (const pxr::SdfPath& path : dirty)
	{
		const auto it = nodeByPath_.find(path);
		if (it != nodeByPath_.end())
			refreshChildren(it->second);
	}
}

// ---------------------------------------------------------------------------
// QAbstractItemModel interface
// ---------------------------------------------------------------------------

UsdStageModel::Node* UsdStageModel::nodeFromIndex(const QModelIndex& index) const
{
	if (index.isValid())
		return static_cast<Node*>(index.internalPointer());

	return root_;
}

QModelIndex UsdStageModel::indexForNode(Node* node) const
{
	if (!node || node == root_)
		return QModelIndex();

	return createIndex(node->row, 0, node);
}

QModelIndex UsdStageModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	Node* parentNode = nodeFromIndex(parent);
	if (!parentNode || row < 0 || row >= static_cast<int>(parentNode->children.size()))
		return QModelIndex();

	return createIndex(row, column, parentNode->children[row].get());
}

QModelIndex UsdStageModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	Node* node = static_cast<Node*>(index.internalPointer());
	if (!node || !node->parent || node->parent == root_)
		return QModelIndex(); // top level prims are children of the invisible pseudo root

	return createIndex(node->parent->row, 0, node->parent);
}

int UsdStageModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	const Node* node = nodeFromIndex(parent);
	return node ? static_cast<int>(node->children.size()) : 0;
}

int UsdStageModel::columnCount(const QModelIndex& /*parent*/) const
{
	return 3;
}

QVariant UsdStageModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || !index.internalPointer())
		return QVariant();

	const Node* node = static_cast<const Node*>(index.internalPointer());
	if (!node->prim)
		return QVariant();

	switch (role)
	{
	case Qt::DisplayRole:
		switch (index.column())
		{
		case NameColumn:
			return QString::fromStdString(node->prim.GetName().GetString());
		case TypeColumn:
		{
			QString type = QString::fromStdString(node->prim.GetTypeName().GetString());
			if (node->prim.IsInstanceProxy())
				type += QStringLiteral(" (proxy)");
			else if (node->prim.GetSpecifier() == pxr::SdfSpecifierOver)
				type += QStringLiteral(" (over)");
			return type;
		}
		case PathColumn:
			return QString::fromStdString(node->path.GetAsString());
		}
		return QVariant();

	case Qt::ToolTipRole:
		return QString::fromStdString(node->path.GetAsString());

	case Qt::ForegroundRole:
		if (!node->prim.IsActive())
			return QColor(130, 130, 130);
		if (node->prim.IsInstanceProxy())
			return QColor(90, 130, 190);
		return QVariant();

	case PathRole:
		return QString::fromStdString(node->path.GetAsString());

	case TypeNameRole:
		return QString::fromStdString(node->prim.GetTypeName().GetString());

	case IsInstanceProxyRole:
		return node->prim.IsInstanceProxy();

	default:
		return QVariant();
	}
}

QVariant UsdStageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QVariant();

	switch (section)
	{
	case NameColumn:
		return QStringLiteral("Name");
	case TypeColumn:
		return QStringLiteral("Type");
	case PathColumn:
		return QStringLiteral("Path");
	}

	return QVariant();
}

Qt::ItemFlags UsdStageModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return QAbstractItemModel::flags(index) | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool UsdStageModel::hasChildren(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return false;

	// The cache is filled on first ask, which is exactly what stops the view
	// from expanding subtrees just to find out whether they are empty.
	UsdStageModel* self = const_cast<UsdStageModel*>(this);
	Node* node = self->nodeFromIndex(parent);
	if (!node)
		return false;

	if (!node->children.empty())
		return true;

	if (!node->childPrimsFetched)
		self->enumerateChildren(node);

	return !node->childPrims.empty();
}

bool UsdStageModel::canFetchMore(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return false;

	UsdStageModel* self = const_cast<UsdStageModel*>(this);
	Node* node = self->nodeFromIndex(parent);
	if (!node)
		return false;

	if (!node->childPrimsFetched)
		self->enumerateChildren(node);

	return node->materialized < node->childPrims.size();
}

void UsdStageModel::fetchMore(const QModelIndex& parent)
{
	Node* node = nodeFromIndex(parent);
	if (!node)
		return;

	if (!node->childPrimsFetched)
		enumerateChildren(node);

	materializeUpTo(node, std::min(node->childPrims.size(), node->materialized + static_cast<size_t>(chunkSize)));
}

pxr::SdfPath UsdStageModel::pathForIndex(const QModelIndex& index) const
{
	if (!index.isValid() || !index.internalPointer())
		return pxr::SdfPath();

	return static_cast<const Node*>(index.internalPointer())->path;
}

QModelIndex UsdStageModel::indexForPath(const pxr::SdfPath& path)
{
	if (!stage_ || !root_ || !path.IsAbsoluteRootOrPrimPath() || path == absoluteRootPath())
		return QModelIndex();

	const auto existing = nodeByPath_.find(path);
	if (existing != nodeByPath_.end())
		return createIndex(existing->second->row, 0, existing->second);

	Node* node = root_;
	for (const pxr::SdfPath& prefix : path.GetPrefixes())
	{
		if (prefix == absoluteRootPath())
			continue;

		if (!node->childPrimsFetched)
			enumerateChildren(node);

		const auto child = std::find_if(node->childPrims.begin(), node->childPrims.end(),
			[&prefix](const pxr::UsdPrim& prim) { return prim.GetPath() == prefix; });

		if (child == node->childPrims.end())
			return QModelIndex(); // not in the stage, or filtered out

		const size_t row = static_cast<size_t>(std::distance(node->childPrims.begin(), child));
		materializeUpTo(node, row + 1);

		Node* childNode = node->children[row].get();
		if (prefix == path)
			return createIndex(childNode->row, 0, childNode);

		node = childNode;
	}

	return QModelIndex();
}
