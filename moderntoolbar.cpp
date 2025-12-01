#include "moderntoolbar.h"
#include "UIStyles.h"
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>


ModernToolbar::ModernToolbar(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(Qt::FramelessWindowHint);
  setObjectName("ModernToolbar");
  setupUi();
}

void ModernToolbar::setupUi() {
  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto btnPen = new QPushButton("Pen", this);
  auto btnEraser = new QPushButton("Eraser", this);
  auto btnLasso = new QPushButton("Lasso", this);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);
  group_->addButton(btnPen, 0);
  group_->addButton(btnEraser, 1);
  group_->addButton(btnLasso, 2);

  btnPen->setCheckable(true);
  btnPen->setChecked(true);
  btnEraser->setCheckable(true);
  btnLasso->setCheckable(true);

  layout->addWidget(btnPen);
  layout->addWidget(btnEraser);
  layout->addWidget(btnLasso);

  connect(group_, &QButtonGroup::idClicked, this, [this](int id) {
    ToolMode m = ToolMode::Pen;
    if (id == 1)
      m = ToolMode::Eraser;
    else if (id == 2)
      m = ToolMode::Lasso;
    mode_ = m;
    emit toolChanged(m);
  });

  setFixedHeight(44);
}

void ModernToolbar::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(UIStyles::ToolbarBg);
  p.setPen(QPen(UIStyles::ToolbarBorder, 1));
  QRect r = rect();
  r.adjust(0, 0, -1, -1);
  p.drawRoundedRect(r, 8, 8);
}

void ModernToolbar::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    dragStartPos_ = e->pos();
  }
}

void ModernToolbar::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() & Qt::LeftButton) {
    QPoint delta = e->pos() - dragStartPos_;
    move(pos() + delta);
  }
}

void ModernToolbar::setToolMode(ToolMode mode) {
  mode_ = mode;
  group_->button(mode == ToolMode::Pen ? 0 : (mode == ToolMode::Eraser ? 1 : 2))
      ->setChecked(true);
}
