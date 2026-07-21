#pragma once

#include <QPointer>
#include <QString>
#include <QStringList>

#include "blop_modal.h"

class QWidget;
class QLabel;
class QProgressBar;

/// Shared Android-safe dialog helpers. Never spawn a top-level QMessageBox /
/// QInputDialog / bare QDialog::exec — those trip Qt 6.10 EGL deadlocks on
/// Android. All of these route through BlopModal::execBlocking.
namespace BlopDialogs {

bool confirm(QWidget *parent, const QString &title, const QString &message,
             const QString &acceptLabel = QString(),
             const QString &rejectLabel = QString());

/// Returns empty string on cancel.
QString promptText(QWidget *parent, const QString &title, const QString &label,
                   const QString &initial = QString());

/// Single-choice list. Returns empty on cancel.
QString promptChoice(QWidget *parent, const QString &title,
                     const QString &label, const QStringList &options,
                     int currentIndex = 0);

/// Integer prompt. Sets *ok=false on cancel when ok is non-null.
/// Returns `value` on cancel if ok is null (caller should check).
int promptInt(QWidget *parent, const QString &title, const QString &label,
              int value, int min, int max, bool *ok = nullptr);

void notify(QWidget *parent, const QString &title, const QString &message);

/// Non-blocking progress card (BlopModal) for long PDF import/export jobs.
struct ProgressSession {
  QPointer<BlopModal> modal;
  QPointer<QLabel> label;
  QPointer<QProgressBar> bar;

  void setMessage(const QString &text);
  void setRange(int minimum, int maximum);
  void setValue(int value);
  void close();
  bool isOpen() const { return modal != nullptr; }
};

ProgressSession presentProgress(QWidget *parent, const QString &title,
                                const QString &message = QString());

} // namespace BlopDialogs
