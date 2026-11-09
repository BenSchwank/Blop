#pragma once

#include <QPoint>
#include <QWidget>

class QButtonGroup;
class QGridLayout;
class QHBoxLayout;
class QPushButton;
class QStackedWidget;

/// Categorized shape picker for Studio Dock second-tap.
class ShapesCatalogPopover : public QWidget {
  Q_OBJECT
public:
  explicit ShapesCatalogPopover(QWidget *parent = nullptr);
  /// When mintTools is false, only the basic Formen tab is shown.
  void present(const QPoint &globalAnchor, bool mintTools = true);

signals:
  void shapeChosen(int shapeToolKind);

private:
  void rebuild(bool mintTools);
  QGridLayout *addPage(QPushButton *tab, const QString &title);
  void addShapeButton(QWidget *page, QGridLayout *grid, const QString &label,
                      int kind);
  void selectTab(int index);
  static QIcon shapePreviewIcon(int kind, int px);

  QHBoxLayout *m_tabLay{nullptr};
  QButtonGroup *m_tabGroup{nullptr};
  QPushButton *m_tabFormen{nullptr};
  QPushButton *m_tabKoordinaten{nullptr};
  QPushButton *m_tabET{nullptr};
  QStackedWidget *m_stack{nullptr};
};
