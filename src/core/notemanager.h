#pragma once
#include "Note.h"
#include <QObject>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class NoteManager : public QObject {
    Q_OBJECT
public:
    explicit NoteManager(QObject* parent=nullptr);

    // Async API (callbacks, no signals for save request)
    void saveNoteAsync(const Note& note, const QString& path, std::function<void(bool)> onDone);
    void loadNoteAsync(const QString& path, std::function<void(bool, Note)> onDone);

    // Sync helpers used inside async
    static bool saveNote(const Note& note, const QString& path);
    static bool loadNote(const QString& path, Note& out);

private:
    static QJsonDocument toJson(const Note& note);
    static bool fromJson(const QJsonDocument& doc, Note& out);
};
