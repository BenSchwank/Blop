#include "blop_dialogs.h"

#include "blop_modal.h"
#include "blop_theme.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

namespace BlopDialogs {

bool confirm(QWidget *parent, const QString &title, const QString &message,
             const QString &acceptLabel, const QString &rejectLabel) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  dlg.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setContentsMargins(20, 18, 20, 16);
  lay->setSpacing(12);
  auto *titleLbl = new QLabel(title, &dlg);
  titleLbl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; font-size: 16px; font-weight: 700; "
                     "background: transparent;")
          .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(titleLbl);
  auto *lbl = new QLabel(message, &dlg);
  lbl->setWordWrap(true);
  lbl->setMaximumWidth(360);
  lbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
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
  dlg.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
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

QString promptChoice(QWidget *parent, const QString &title,
                     const QString &label, const QStringList &options,
                     int currentIndex) {
  if (options.isEmpty())
    return {};
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

  auto *list = new QListWidget(&dlg);
  list->addItems(options);
  list->setCurrentRow(qBound(0, currentIndex, options.size() - 1));
  list->setStyleSheet(BlopTheme::themed(
      QStringLiteral("QListWidget { background: %1; color: %2; border: 1px solid %3;"
                     "  border-radius: 12px; padding: 4px; }"
                     "QListWidget::item { padding: 10px 12px; border-radius: 8px; }"
                     "QListWidget::item:selected { background: %4; color: %5; }")
          .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
               BlopTheme::textPrimary().name(QColor::HexRgb),
               BlopTheme::borderSubtle().name(QColor::HexRgb),
               BlopTheme::accentPrimary().name(QColor::HexRgb),
               BlopTheme::textOnAccent().name(QColor::HexRgb))));
  list->setMinimumHeight(180);
  lay->addWidget(list);

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
  QObject::connect(list, &QListWidget::itemDoubleClicked, &dlg,
                   &QDialog::accept);

  if (BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg) !=
      QDialog::Accepted)
    return {};
  if (auto *it = list->currentItem())
    return it->text();
  return {};
}

int promptInt(QWidget *parent, const QString &title, const QString &label,
              int value, int min, int max, bool *ok) {
  if (ok)
    *ok = false;
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

  auto *spin = new QSpinBox(&dlg);
  spin->setRange(min, max);
  spin->setValue(qBound(min, value, max));
  spin->setStyleSheet(BlopTheme::themed(
      QStringLiteral("QSpinBox { background: %1; color: %2; border: 1px solid %3;"
                     "  border-radius: 12px; padding: 8px 12px; }")
          .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
               BlopTheme::textPrimary().name(QColor::HexRgb),
               BlopTheme::borderSubtle().name(QColor::HexRgb))));
  lay->addWidget(spin);

  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto *cancel = new QPushButton(QObject::tr("Abbrechen"), &dlg);
  cancel->setStyleSheet(BlopTheme::secondaryButtonQss());
  auto *okBtn = new QPushButton(QObject::tr("OK"), &dlg);
  okBtn->setStyleSheet(BlopTheme::primaryButtonQss());
  okBtn->setDefault(true);
  btnRow->addWidget(cancel);
  btnRow->addWidget(okBtn);
  lay->addLayout(btnRow);
  QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
  QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

  if (BlopModal::execBlocking(parent ? parent->window() : nullptr, &dlg) !=
      QDialog::Accepted)
    return value;
  if (ok)
    *ok = true;
  return spin->value();
}

void notify(QWidget *parent, const QString &title, const QString &message) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  dlg.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setContentsMargins(20, 18, 20, 16);
  lay->setSpacing(12);
  auto *titleLbl = new QLabel(title, &dlg);
  titleLbl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; font-size: 16px; font-weight: 700; "
                     "background: transparent;")
          .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(titleLbl);
  auto *lbl = new QLabel(message, &dlg);
  lbl->setWordWrap(true);
  lbl->setMaximumWidth(360);
  lbl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
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

void ProgressSession::setMessage(const QString &text) {
  if (label)
    label->setText(text);
}

void ProgressSession::setRange(int minimum, int maximum) {
  if (bar)
    bar->setRange(minimum, maximum);
}

void ProgressSession::setValue(int value) {
  if (bar)
    bar->setValue(value);
}

void ProgressSession::close() {
  if (modal)
    modal->dismiss();
  modal.clear();
  label.clear();
  bar.clear();
}

ProgressSession presentProgress(QWidget *parent, const QString &title,
                                const QString &message) {
  ProgressSession session;
  auto *form = new QWidget;
  form->setMinimumWidth(320);
  auto *lay = new QVBoxLayout(form);
  lay->setContentsMargins(8, 4, 8, 8);
  lay->setSpacing(12);
  auto *lbl = new QLabel(message.isEmpty() ? QObject::tr("Bitte warten…")
                                           : message,
                         form);
  lbl->setWordWrap(true);
  lbl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: %1; background: transparent; font-size: 13px;")
          .arg(BlopTheme::textPrimary().name())));
  auto *bar = new QProgressBar(form);
  bar->setRange(0, 0); // indeterminate until caller sets a range
  bar->setTextVisible(true);
  bar->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QProgressBar {"
      "  background: %1; border: 1px solid %2; border-radius: 8px;"
      "  min-height: 14px; text-align: center; color: %3;"
      "}"
      "QProgressBar::chunk {"
      "  background: %4; border-radius: 7px;"
      "}")
                                           .arg(BlopTheme::surfaceMuted().name(),
                                                BlopTheme::borderDefault().name(),
                                                BlopTheme::textSecondary().name(),
                                                BlopTheme::accentPrimary().name())));
  lay->addWidget(lbl);
  lay->addWidget(bar);

  BlopModal *modal =
      BlopModal::present(parent ? parent->window() : parent, form,
                         BlopModal::Mode::Card, title);
  if (modal)
    modal->setPreferredCardWidth(420);

  session.modal = modal;
  session.label = lbl;
  session.bar = bar;
  return session;
}

} // namespace BlopDialogs
