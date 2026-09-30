#pragma once

#include <QString>

class NoteWriter {
public:
    /// Same library root Blop uses: Documents/BlopNotizen on the desktop,
    /// app data/BlopNotizen on Android.
    static QString libraryRoot();

    /// Writes a multi-page Blop note (.bnote) and returns its absolute path.
    static QString write(const QString &title, const QString &body, QString *error);
};
