#include "blop_dialogs.h"

#include "blop_modal.h"
#include "blop_theme.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace BlopDialogs {

bool confirm(QWidget *parent, const QString &title, const QString &message,
             const QString &acceptLabel, const QString &rejectLabel) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setContentsMargins(20, 18, 20, 16);
  lay->setSpacing(12);
  auto *lbl = new QLabel(message, &dlg);
  lbl->setWordWrap(true);
  lbl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; background: transparent;")
          .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(lbl);

  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto *no = new QPushButton(
      rejectLabel.isEmpty() ? QObject::tr("Abbrechen") : rejectLabel, &dlg);
  no->setStyleSheet(BlopTheme::secondaryButtonQss());
  auto *yes = new QPushButton(
      acceptLabel.isEmpty() ? QObject::tr("Ja") : acceptLabel, &dlg);
  yes->setStyleSheet(BlopTheme::primaryButtonQss());
  yes->setDefault(true);
  btnRow->addWidget(no);
  btnRow->addWidget(yes);
  lay->addLayout(btnRow);
  QObject::connect(no, &QPushButton::clicked, &dlg, &QDialog::reject);
  QObject::connect(yes, &QPushButton::clicked, &dlg, &QDialog::accept);
  return BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg) ==
         QDialog::Accepted;
}

QString promptText(QWidget *parent, const QString &title, const QString &label,
                   const QString &initial) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setContentsMargins(20, 18, 20, 16);
  lay->setSpacing(12);

  auto *titleLbl = new QLabel(title, &dlg);
  titleLbl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; font-size: 16px; font-weight: 700; "
                     "background: transparent;")
          .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(titleLbl);

  if (!label.isEmpty()) {
    auto *hint = new QLabel(label, &dlg);
    hint->setStyleSheet(BlopTheme::themed(
        QStringLiteral("color: %1; background: transparent;")
            .arg(BlopTheme::textSecondary().name())));
    lay->addWidget(hint);
  }

  auto *edit = new QLineEdit(initial, &dlg);
  edit->setStyleSheet(BlopTheme::themed(
      QStringLiteral("QLineEdit { background: %1; color: %2; border: 1px solid %3;"
                     "  border-radius: 12px; padding: 8px 12px; }"
                     "QLineEdit:focus { border: 1px solid %4; }")
          .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
               BlopTheme::textPrimary().name(QColor::HexRgb),
               BlopTheme::borderSubtle().name(QColor::HexRgb),
               BlopTheme::accentPrimary().name(QColor::HexRgb))));
  lay->addWidget(edit);

  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto *cancel = new QPushButton(QObject::tr("Abbrechen"), &dlg);
  cancel->setStyleSheet(BlopTheme::secondaryButtonQss());
  auto *ok = new QPushButton(QObject::tr("OK"), &dlg);
  ok->setStyleSheet(BlopTheme::primaryButtonQss());
  ok->setDefault(true);
  btnRow->addWidget(cancel);
  btnRow->addWidget(ok);
  lay->addLayout(btnRow);
  QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
  QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
  QObject::connect(edit, &QLineEdit::returnPressed, &dlg, &QDialog::accept);

  if (BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg) !=
      QDialog::Accepted)
    return {};
  return edit->text().trimmed();
}

void notify(QWidget *parent, const QString &title, const QString &message) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setContentsMargins(20, 18, 20, 16);
  lay->setSpacing(12);
  auto *lbl = new QLabel(message, &dlg);
  lbl->setWordWrap(true);
  lbl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; background: transparent;")
          .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(lbl);
  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto *ok = new QPushButton(QObject::tr("OK"), &dlg);
  ok->setStyleSheet(BlopTheme::primaryButtonQss());
  ok->setDefault(true);
  btnRow->addWidget(ok);
  lay->addLayout(btnRow);
  QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
  BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg);
}

} // namespace BlopDialogs
