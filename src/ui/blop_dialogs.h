#pragma once

#include <QString>

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

void notify(QWidget *parent, const QString &title, const QString &message);

} // namespace BlopDialogs
