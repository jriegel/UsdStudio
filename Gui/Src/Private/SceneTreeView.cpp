#include "../Public/SceneTreeView.h"

#include "../Public/UsdStageModel.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QToolButton>

// ---------------------------------------------------------------------------
// SceneTreeView
// ---------------------------------------------------------------------------
SceneTreeView::SceneTreeView(QWidget* parent)
    : QTreeView(parent)
{
    // --- performance ------------------------------------------------------
    setUniformRowHeights(true);                            // the big one
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setAnimated(false);
    setSortingEnabled(false);

    // --- appearance / interaction -----------------------------------------
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setIndentation(16);
    setWordWrap(false);

    // ResizeToContents on a header walks every row of the column: never do it
    // here.  Interactive sections stay user-resizable at O(1).
    if (auto* hdr = header()) {
        hdr->setSectionsMovable(true);
        hdr->setSectionResizeMode(QHeaderView::Interactive);
        hdr->setStretchLastSection(false);
        hdr->setSortIndicatorShown(false);
    }
    setColumnWidth(0, 280);   // UsdStageModel::NameColumn
    setColumnWidth(1, 160);   // UsdStageModel::TypeColumn
    setColumnWidth(2, 300);   // UsdStageModel::PathColumn
}

// ---------------------------------------------------------------------------
// BreadcrumbBar
// ---------------------------------------------------------------------------
BreadcrumbBar::BreadcrumbBar(QWidget* parent)
    : QWidget(parent)
{
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(4, 2, 4, 2);
    layout_->setSpacing(2);

    titleLabel_ = new QLabel(this);
    titleLabel_->setText(QStringLiteral("Path:"));
    layout_->addWidget(titleLabel_);
}

void BreadcrumbBar::setTreeView(SceneTreeView* view)
{
    view_ = view;
    if (!view_)
        return;

    // The view installs a fresh selection model whenever its model is swapped,
    // so the selection connections are re-done on modelReset as well.
    auto reconnect = [this]() {
        if (auto* sel = view_->selectionModel()) {
            connect(sel, &QItemSelectionModel::selectionChanged, this,
                    [this](const QItemSelection&, const QItemSelection&) {
                        updateForIndex(view_->currentIndex());
                    });
            connect(sel, &QItemSelectionModel::currentChanged, this,
                    [this](const QModelIndex& index) { updateForIndex(index); });
        }
        updateForIndex(view_->currentIndex());
    };

    if (auto* model = view_->model())
        connect(model, &QAbstractItemModel::modelReset, this, reconnect);
    reconnect();
}

void BreadcrumbBar::clearButtons()
{
    for (auto* button : buttons_) {
        layout_->removeWidget(button);
        button->deleteLater();
    }
    buttons_.clear();
}

void BreadcrumbBar::addCrumb(const QPersistentModelIndex& index, bool leaf)
{
    if (!buttons_.empty()) {
        auto* separator = new QLabel(QStringLiteral("/"), this);
        layout_->addWidget(separator);
    }

    auto* button = new QToolButton(this);
    button->setAutoRaise(true);
    button->setText(index.data(Qt::DisplayRole).toString());
    button->setToolTip(index.data(Qt::ToolTipRole).toString());
    if (leaf)
        button->setStyleSheet(QStringLiteral("font-weight: bold;"));

    // Persistent index: survives row insertions while the model fetches more
    // children, so a crumb never jumps to the wrong prim.
    connect(button, &QToolButton::clicked, this, [this, index]() { jumpTo(index); });

    layout_->addWidget(button);
    buttons_.push_back(button);
}

void BreadcrumbBar::updateForIndex(const QModelIndex& index)
{
    if (!view_ || !view_->model())
        return;

    clearButtons();

    QModelIndex current = index;
    if (!current.isValid())
        current = view_->currentIndex();
    // No selection yet: show the first top level prim so the bar always has
    // context instead of being empty.
    if (!current.isValid())
        current = view_->model()->index(0, 0);
    if (!current.isValid()) {
        setVisible(false);
        return;
    }

    // Walk to the root, then emit crumbs top-down.
    std::vector<QPersistentModelIndex> chain;
    for (QModelIndex walk = current; walk.isValid(); walk = walk.parent())
        chain.push_back(QPersistentModelIndex(walk));

    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        addCrumb(*it, it + 1 == chain.rend());

    setVisible(true);
}

void BreadcrumbBar::jumpTo(const QPersistentModelIndex& index)
{
    if (!view_ || !index.isValid())
        return;

    const QModelIndex target = index;
    view_->expand(target);
    view_->setCurrentIndex(target);
    if (auto* sel = view_->selectionModel()) {
        sel->select(target,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    view_->scrollTo(target);
}
