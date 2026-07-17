#pragma once

#include <QString>
#include <QStringList>

class QWidget;

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

} // namespace BlopDialogs
