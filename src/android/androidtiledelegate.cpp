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

#include "androidicons.h"
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

bool AndroidTileDelegate::editorEvent(QEvent *event,
                                       QAbstractItemModel *model,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) {
  if (event->type() == QEvent::MouseButtonRelease && m_window) {
    auto *me = static_cast<QMouseEvent *>(event);
    const QRect rect = option.rect.adjusted(UiScale::dp(4), UiScale::dp(4),
                                             -UiScale::dp(4), -UiScale::dp(4));
    // Generous click area so the user only needs to land somewhere in the
    // top-right corner rather than precisely on the small pill graphic.
    const int hitW = qMax(UiScale::dp(48), UiScale::dp(28) + UiScale::dp(20));
    const int hitH = qMax(UiScale::dp(40), UiScale::dp(20) + UiScale::dp(20));
    const QRect hit(rect.right() - hitW, rect.top(), hitW, hitH);
    if (hit.contains(me->pos())) {
      // Tell MainWindow to swallow the matching QListView::clicked
      // signal that fires immediately after this editorEvent - without
      // this guard the note is opened underneath the menu and the menu
      // never becomes visible to the user.
      m_window->setAndroidPillClickPending(true);
      // Anchor under the pill via the actual viewport widget instead of
      // QMouseEvent::globalPosition() - the latter can be (0,0) when Qt
      // synthesises the mouse from a touch on Android. Mapping the
      // option.rect avoids that whole class of issues.
      QPoint anchor;
      if (option.widget) {
        anchor = option.widget->mapToGlobal(
            QPoint(option.rect.right() - UiScale::dp(8),
                   option.rect.top() + UiScale::dp(28)));
      } else {
        anchor = me->globalPosition().toPoint();
      }
      // CRITICAL: defer the menu open. Calling QMenu::exec() (inside
      // showContextMenu) from inside QStyledItemDelegate::editorEvent
      // while a touch event is mid-delivery deadlocks Qt's Android
      // touch synthesiser - the user sees this as an instant crash on
      // tap. Posting via singleShot lets the current touch chain
      // finish first; the menu then opens on the next event-loop tick.
      QPointer<MainWindow> safeWin(m_window);
      QPersistentModelIndex safeIdx(index);
      QTimer::singleShot(0, m_window, [safeWin, safeIdx, anchor]() {
        if (safeWin && safeIdx.isValid())
          safeWin->showContextMenu(anchor, QModelIndex(safeIdx));
      });
      return true;
    }
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}
