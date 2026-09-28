#include "calendareventeditor.h"

#include "blop_modal.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace {
QString fieldQss() {
  return BlopTheme::themed(
      QStringLiteral("QLineEdit, QDateEdit, QTimeEdit {"
                     "  background: %1; color: %2; border: 1px solid %3;"
                     "  border-radius: 10px; padding: 8px 10px;"
                     "}"
                     "QLineEdit:focus, QDateEdit:focus, QTimeEdit:focus {"
                     "  border: 1px solid %4;"
                     "}")
          .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
               BlopTheme::textPrimary().name(QColor::HexRgb),
               BlopTheme::borderSubtle().name(QColor::HexRgb),
               BlopTheme::accentPrimary().name(QColor::HexRgb)));
}

QString captionQss() {
  return BlopTheme::themed(
      QStringLiteral("color: %1; font-size: 12px; font-weight: 550;"
                     "background: transparent;")
          .arg(BlopTheme::textSecondary().name()));
}
} // namespace

bool CalendarEventEditor::promptNew(QWidget *parent,
                                    const QDateTime &presetStart,
                                    CalendarEvent *out) {
  if (!out)
    return false;

  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("Neuer Termin"));
  auto *root = new QVBoxLayout(&dlg);
  root->setContentsMargins(20, 18, 20, 16);
  root->setSpacing(10);

  auto *heading = new QLabel(QStringLiteral("Neuer Termin"), &dlg);
  heading->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; font-size: 16px; font-weight: 700;"
                     "background: transparent;")
          .arg(BlopTheme::textPrimary().name())));
  root->addWidget(heading);

  auto addCaption = [&](const QString &text) {
    auto *c = new QLabel(text, &dlg);
    c->setStyleSheet(captionQss());
    root->addWidget(c);
  };

  addCaption(QStringLiteral("Titel"));
  auto *title = new QLineEdit(&dlg);
  title->setPlaceholderText(QStringLiteral("z. B. Meeting, Lekce…"));
  title->setStyleSheet(fieldQss());
  root->addWidget(title);

  addCaption(QStringLiteral("Datum"));
  auto *date = new QDateEdit(presetStart.isValid() ? presetStart.date()
                                                   : QDate::currentDate(),
                             &dlg);
  date->setCalendarPopup(true);
  date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
  date->setStyleSheet(fieldQss());
  root->addWidget(date);

  auto *allDay = new QCheckBox(QStringLiteral("Ganztägig"), &dlg);
  allDay->setStyleSheet(BlopTheme::themed(
      QStringLiteral("QCheckBox { color: %1; spacing: 8px; }"
                     "QCheckBox::indicator { width: 16px; height: 16px; }")
          .arg(BlopTheme::textPrimary().name())));
  root->addWidget(allDay);

  auto *timeRow = new QHBoxLayout();
  timeRow->setSpacing(UiScale::dp(8));
  auto *timeColStart = new QVBoxLayout();
  timeColStart->setSpacing(4);
  auto *startCap = new QLabel(QStringLiteral("Von"), &dlg);
  startCap->setStyleSheet(captionQss());
  timeColStart->addWidget(startCap);
  QTime startT = presetStart.isValid() && presetStart.time().isValid()
                     ? presetStart.time()
                     : QTime(9, 0);
  startT = QTime(startT.hour(), (startT.minute() / 15) * 15);
  auto *startTime = new QTimeEdit(startT, &dlg);
  startTime->setDisplayFormat(QStringLiteral("HH:mm"));
  startTime->setStyleSheet(fieldQss());
  timeColStart->addWidget(startTime);
  timeRow->addLayout(timeColStart, 1);

  auto *timeColEnd = new QVBoxLayout();
  timeColEnd->setSpacing(4);
  auto *endCap = new QLabel(QStringLiteral("Bis"), &dlg);
  endCap->setStyleSheet(captionQss());
  timeColEnd->addWidget(endCap);
  auto *endTime = new QTimeEdit(startT.addSecs(3600), &dlg);
  endTime->setDisplayFormat(QStringLiteral("HH:mm"));
  endTime->setStyleSheet(fieldQss());
  timeColEnd->addWidget(endTime);
  timeRow->addLayout(timeColEnd, 1);
  root->addLayout(timeRow);

  const auto syncAllDay = [allDay, startTime, endTime, startCap, endCap]() {
    const bool on = allDay->isChecked();
    startTime->setEnabled(!on);
    endTime->setEnabled(!on);
    startCap->setEnabled(!on);
    endCap->setEnabled(!on);
  };
  QObject::connect(allDay, &QCheckBox::toggled, &dlg, syncAllDay);
  syncAllDay();

  addCaption(QStringLiteral("Ort (optional)"));
  auto *location = new QLineEdit(&dlg);
  location->setPlaceholderText(QStringLiteral("Raum, Link, Adresse…"));
  location->setStyleSheet(fieldQss());
  root->addWidget(location);

  addCaption(QStringLiteral("Farbe"));
  auto *colorRow = new QHBoxLayout();
  colorRow->setSpacing(UiScale::dp(6));
  const QStringList palette = {
      QStringLiteral("#5B9DFF"), QStringLiteral("#34C759"),
      QStringLiteral("#FF9F0A"), QStringLiteral("#FF453A"),
      QStringLiteral("#BF5AF2"), QStringLiteral("#64D2FF"),
  };
  QString selectedColor = palette.first();
  QList<QPushButton *> swatches;
  for (const QString &hex : palette) {
    auto *sw = new QPushButton(&dlg);
    sw->setFixedSize(UiScale::dp(28), UiScale::dp(28));
    sw->setCursor(Qt::PointingHandCursor);
    sw->setProperty("calColor", hex);
    swatches.append(sw);
    colorRow->addWidget(sw, 0);
  }
  colorRow->addStretch(1);
  root->addLayout(colorRow);

  const auto paintSwatches = [swatches, &selectedColor]() {
    for (QPushButton *sw : swatches) {
      const QString hex = sw->property("calColor").toString();
      const bool on = (hex.compare(selectedColor, Qt::CaseInsensitive) == 0);
      sw->setStyleSheet(
          QStringLiteral("QPushButton {"
                         "  background: %1; border: %2px solid %3;"
                         "  border-radius: %4px;"
                         "}")
              .arg(hex, QString::number(on ? 2 : 1),
                   on ? QStringLiteral("#FFFFFF") : QStringLiteral("transparent"),
                   QString::number(UiScale::dp(8))));
    }
  };
  for (QPushButton *sw : swatches) {
    QObject::connect(sw, &QPushButton::clicked, &dlg, [sw, &selectedColor,
                                                       paintSwatches]() {
      selectedColor = sw->property("calColor").toString();
      paintSwatches();
    });
  }
  paintSwatches();

  auto *btns = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  btns->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Speichern"));
  btns->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
  btns->button(QDialogButtonBox::Ok)->setStyleSheet(BlopTheme::primaryButtonQss());
  btns->button(QDialogButtonBox::Cancel)
      ->setStyleSheet(BlopTheme::secondaryButtonQss());
  QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  root->addWidget(btns);

  title->setFocus();
  if (BlopModal::execBlocking(parent, &dlg, BlopModal::Mode::Card,
                              UiScale::dp(420)) != QDialog::Accepted)
    return false;

  const QString t = title->text().trimmed();
  if (t.isEmpty())
    return false;

  out->title = t;
  out->allDay = allDay->isChecked();
  out->location = location->text().trimmed();
  out->color = selectedColor;
  out->source = QStringLiteral("local");
  if (out->allDay) {
    out->start = QDateTime(date->date(), QTime(0, 0));
    out->end = QDateTime(date->date().addDays(1), QTime(0, 0));
  } else {
    QTime a = startTime->time();
    QTime b = endTime->time();
    if (b <= a)
      b = a.addSecs(3600);
    out->start = QDateTime(date->date(), a);
    out->end = QDateTime(date->date(), b);
  }
  return true;
}
