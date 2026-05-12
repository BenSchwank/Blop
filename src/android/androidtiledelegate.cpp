#include "androidtiledelegate.h"

#include <QApplication>
#include <QCursor>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPointer>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextOption>
#include <QTimer>

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QEasingCurve>
#include <QVariantAnimation>

#include "androidicons.h"
#include "blopripple.h"
#include "mainwindow.h"
#include "uiscale.h"

namespace {

// Resolve the right glyph + colour for a model row. Folder rows come from
// the directory model and don't have the .blop/.bnote extension.
struct TileVisual {
  QString iconName;
  QColor iconColor;
};

TileVisual visualFor(const QModelIndex &index, MainWindow *win) {
  TileVisual v;
  // Primary: ask the QFileSystemModel directly. UserRole+1 is a fallback
  // that QFileSystemModel doesn't actually populate, and Qt::DisplayRole
  // can hide the file extension (e.g. when the OS / model strips it),
  // both of which used to send notes through the folder branch.
  bool isDir = false;
  QString suffix;
  const auto *fsm = qobject_cast<const QFileSystemModel *>(index.model());
  if (fsm) {
    isDir = fsm->isDir(index);
    suffix = fsm->fileInfo(index).suffix().toLower();
  } else {
    const QString fn = index.data(Qt::DisplayRole).toString();
    const int dot = fn.lastIndexOf(QLatin1Char('.'));
    if (dot >= 0)
      suffix = fn.mid(dot + 1).toLower();
  }

  if (isDir) {
    v.iconName = QStringLiteral("folder");
    v.iconColor = QColor(QStringLiteral("#A8B0DA"));
  } else if (suffix == QLatin1String("blop")) {
    v.iconName = QStringLiteral("note_blop");
    v.iconColor = win ? win->currentAccentColor()
                      : QColor(QStringLiteral("#7C6CFF"));
  } else {
    // .bnote and any other non-folder gets the standard note glyph.
    v.iconName = QStringLiteral("note_bnote");
    v.iconColor = QColor(QStringLiteral("#C8CCE8"));
  }
  return v;
}

static QHash<QPersistentModelIndex, double> s_tilePressScale;
static QHash<QPersistentModelIndex, QPointer<QVariantAnimation>> s_tilePressAnim;

static void stopTilePressAnim(const QPersistentModelIndex &pmi) {
  QPointer<QVariantAnimation> a = s_tilePressAnim.take(pmi);
  if (a) {
    a->stop();
    a->deleteLater();
  }
}

static void runTilePressScaleAnim(const QPersistentModelIndex &pmi,
                                  QAbstractItemView *view, qreal from, qreal to,
                                  int ms, const QEasingCurve &curve) {
  if (!pmi.isValid() || !view)
    return;
  stopTilePressAnim(pmi);
  auto *anim = new QVariantAnimation(view);
  anim->setDuration(ms);
  anim->setStartValue(from);
  anim->setEndValue(to);
  anim->setEasingCurve(curve);
  s_tilePressAnim.insert(pmi, anim);
  QObject::connect(anim, &QVariantAnimation::valueChanged, view,
                   [pmi, view](const QVariant &v) {
                     if (pmi.isValid())
                       s_tilePressScale[pmi] = v.toDouble();
                     if (view && pmi.isValid())
                       view->update(pmi);
                   });
  QObject::connect(anim, &QVariantAnimation::finished, view, [pmi, view, anim]() {
    s_tilePressAnim.remove(pmi);
    const qreal endV = anim->endValue().toDouble();
    if (qAbs(endV - 1.0) < 1e-3)
      s_tilePressScale.remove(pmi);
    else if (pmi.isValid())
      s_tilePressScale[pmi] = endV;
    if (view && pmi.isValid())
      view->update(pmi);
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace

AndroidTileDelegate::AndroidTileDelegate(MainWindow *win, QObject *parent)
    : QStyledItemDelegate(parent), m_window(win) {}

QSize AndroidTileDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
  // The view drives the actual grid size via setGridSize(); returning the
  // option rect lets QListView keep its computed cell extent.
  Q_UNUSED(index);
  return option.rect.size().isValid() ? option.rect.size()
                                       : QSize(UiScale::dp(160),
                                               UiScale::dp(180));
}

void AndroidTileDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

  const QPersistentModelIndex pi(index);
  const qreal pressScale = s_tilePressScale.value(pi, 1.0);
  if (qAbs(pressScale - 1.0) > 1e-6) {
    const QPointF c = option.rect.center();
    painter->translate(c);
    painter->scale(pressScale, pressScale);
    painter->translate(-c);
  }

  const QRect rect = option.rect.adjusted(UiScale::dp(4), UiScale::dp(4),
                                          -UiScale::dp(4), -UiScale::dp(4));

  // Card background (Material elevated surface).
  QColor bg(QStringLiteral("#161423"));
  if (option.state & QStyle::State_Selected) {
    QColor accent = m_window ? m_window->currentAccentColor()
                              : QColor(QStringLiteral("#7C6CFF"));
    bg = accent.darker(160);
  } else if (option.state & QStyle::State_MouseOver) {
    bg = QColor(QStringLiteral("#1C1A2A"));
  }
  painter->setPen(Qt::NoPen);
  painter->setBrush(bg);
  painter->drawRoundedRect(rect, UiScale::dp(16), UiScale::dp(16));

  const TileVisual visual = visualFor(index, m_window);
  const QString text = index.data(Qt::DisplayRole).toString();

  // ---------------------------------------------------------------------
  // Layout: icon centred, text below it. The icon takes ~50% of the tile
  // height but never less than dp(56) (touch-target friendly) nor more
  // than the tile width minus padding.
  // ---------------------------------------------------------------------
  const int textBlockH = UiScale::dp(38); // room for two lines + spacing
  const int gap = UiScale::dp(6);
  int iconDim =
      qMin(rect.width() - UiScale::dp(24), rect.height() - textBlockH - gap);
  iconDim = qBound(UiScale::dp(56), iconDim, UiScale::dp(112));

  const int contentH = iconDim + gap + textBlockH;
  const int startY = rect.top() + qMax(UiScale::dp(8),
                                       (rect.height() - contentH) / 2);
  const QRect iconRect(rect.center().x() - iconDim / 2, startY, iconDim,
                       iconDim);
  const QPixmap glyph =
      AndroidIcons::pixmap(visual.iconName, visual.iconColor, iconDim);
  if (!glyph.isNull())
    painter->drawPixmap(iconRect, glyph);

  // Caption.
  QFont font = option.font;
  font.setPointSize(11);
  font.setWeight(QFont::Medium);
  painter->setFont(font);
  painter->setPen(QColor(QStringLiteral("#E8E6F4")));

  const QRect textRect(rect.left() + UiScale::dp(6),
                       iconRect.bottom() + gap,
                       rect.width() - UiScale::dp(12), textBlockH);
  QTextOption opt;
  opt.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  // Elide manually so we get a clean two-line cap.
  QFontMetrics fm(font);
  QString display = text;
  // Strip the .blop / .bnote extension so the user-visible label matches
  // the file system name without noise.
  if (display.endsWith(QStringLiteral(".blop"), Qt::CaseInsensitive))
    display.chop(5);
  else if (display.endsWith(QStringLiteral(".bnote"), Qt::CaseInsensitive))
    display.chop(6);
  painter->drawText(textRect, display, opt);

  // Three-dot menu in the top-right corner.
  const int pillW = UiScale::dp(28);
  const int pillH = UiScale::dp(20);
  const QRect pillRect(rect.right() - pillW - UiScale::dp(6),
                       rect.top() + UiScale::dp(6), pillW, pillH);
  // Subtle pill background so the dots have contrast even on a hovered
  // tile.
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(0, 0, 0, 90));
  painter->drawRoundedRect(pillRect, pillH / 2, pillH / 2);
  const int dotsSize = qMin(pillW, pillH) - UiScale::dp(2);
  const QPixmap dots = AndroidIcons::pixmap(
      QStringLiteral("more_horz"), QColor(QStringLiteral("#E8E6F4")), dotsSize);
  if (!dots.isNull()) {
    const QRect dotsRect(pillRect.center().x() - dotsSize / 2,
                         pillRect.center().y() - dotsSize / 2, dotsSize,
                         dotsSize);
    painter->drawPixmap(dotsRect, dots);
  }

  painter->restore();
}

bool AndroidTileDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                      const QStyleOptionViewItem &option,
                                      const QModelIndex &index) {
  auto *view = qobject_cast<QAbstractItemView *>(parent());
  const QRect rect = option.rect.adjusted(UiScale::dp(4), UiScale::dp(4),
                                          -UiScale::dp(4), -UiScale::dp(4));
  const int hitW = qMax(UiScale::dp(48), UiScale::dp(28) + UiScale::dp(20));
  const int hitH = qMax(UiScale::dp(40), UiScale::dp(20) + UiScale::dp(20));
  const QRect pillHit(option.rect.right() - hitW, option.rect.top(), hitW,
                        hitH);

  if (event->type() == QEvent::MouseButtonPress && m_window && view) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton && rect.contains(me->pos()) &&
        !pillHit.contains(me->pos())) {
      m_tilePressIndex = QPersistentModelIndex(index);
      const qreal from = s_tilePressScale.value(m_tilePressIndex, 1.0);
      runTilePressScaleAnim(m_tilePressIndex, view, from, 0.96, 90,
                            QEasingCurve::OutCubic);
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  if (event->type() == QEvent::MouseButtonRelease && m_window) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (pillHit.contains(me->pos())) {
      m_window->setAndroidPillClickPending(true);
      QPointer<MainWindow> safeWin(m_window);
      QPersistentModelIndex safeIdx(index);
      QTimer::singleShot(0, m_window, [safeWin, safeIdx]() {
        if (safeWin && safeIdx.isValid())
          safeWin->showAndroidTileContextOverlay(safeIdx);
      });
      return true;
    }
    if (view && m_tilePressIndex.isValid()) {
      const QPersistentModelIndex pressed = m_tilePressIndex;
      m_tilePressIndex = QPersistentModelIndex();
      const qreal from = s_tilePressScale.value(pressed, 1.0);
      runTilePressScaleAnim(pressed, view, from, 1.0, 220, QEasingCurve::OutBack);
      if (pressed == QPersistentModelIndex(index) &&
          me->button() == Qt::LeftButton && rect.contains(me->pos()) &&
          !pillHit.contains(me->pos()) && view->viewport()) {
        const QPoint g = view->viewport()->mapToGlobal(
            view->visualRect(index).center());
        BlopRipple::spawn(m_window, g);
      }
    }
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}
