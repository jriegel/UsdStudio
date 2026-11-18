#pragma once

#include <QTreeView>
#include <QWidget>

#include <vector>

class BreadcrumbBar;
class QHBoxLayout;
class QLabel;
class QPersistentModelIndex;
class QToolButton;
class QModelIndex;

// ---------------------------------------------------------------------------
// SceneTreeView: a QTreeView preset with the settings the research notes
// ("QT tree view tec for handling real big trees") identify as essential for
// millions of rows.  Each is a one-liner, but forgetting one costs orders of
// magnitude:
//   * uniform row heights -> the view computes visibility arithmetically
//     instead of querying the delegate for every row
//   * no animation, no sorting, no filter/sort proxy model
//   * pixel scrolling and an interactive (never ResizeToContents) header
// The heavy lifting is in UsdStageModel; the view must simply not undo it.
// ---------------------------------------------------------------------------
class SceneTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit SceneTreeView(QWidget* parent = nullptr);

    // QTreeView ignores setColumnWidth() calls made before a model is set,
    // so the default widths are (re-)applied here instead of in the ctor.
    void setModel(QAbstractItemModel* model) override;

private:
    void applyDefaultColumnWidths();
};

// ---------------------------------------------------------------------------
// BreadcrumbBar: clickable ancestor chain of the current selection.  In a deep
// scene graph it answers "where am I?" and lets the user jump to any ancestor.
// ---------------------------------------------------------------------------
class BreadcrumbBar : public QWidget
{
    Q_OBJECT

public:
    explicit BreadcrumbBar(QWidget* parent = nullptr);

    /// Tracks the view's selection model (and re-links after a model swap).
    /// Call after the view already has its model.
    void setTreeView(SceneTreeView* view);

    /// Rebuilds the bar for \a index (invalid index = current selection).
    void updateForIndex(const QModelIndex& index);

private:
    void addCrumb(const QPersistentModelIndex& index, bool leaf);
    void jumpTo(const QPersistentModelIndex& index);
    void clearButtons();

    SceneTreeView* view_ = nullptr;
    QHBoxLayout* layout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    std::vector<QToolButton*> buttons_;
    // The "/" labels between crumbs must be torn down with the buttons;
    // leaking them turns the bar into an ever-growing row of slashes.
    std::vector<QWidget*> separators_;
};
