#pragma once

#include <QString>
#include <QStringList>

class NoteWriter {
public:
    /// Same library root Blop uses: Documents/BlopNotizen on the desktop,
    /// app data/BlopNotizen on Android.
    static QString libraryRoot();
    static QString pathFor(const QString &title);

    /// Writes a multi-page Blop note (.bnote) and returns its absolute path.
    static QString write(const QString &title, const QString &body, QString *error);

    /// Opens an existing note with this title or creates it, then appends a
    /// heading and bullet lines. Returns the absolute path.
    static QString compose(const QString &title, const QString &heading,
                           const QStringList &points, QString *error);
};
