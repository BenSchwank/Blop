#include "NoteWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {

QString safeStem(QString name) {
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]")),
                 QStringLiteral("_"));
    name = name.trimmed();
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    if (name.isEmpty())
        name = QStringLiteral("Neue Notiz");
    if (name.size() > 60)
        name = name.left(60).trimmed();
    return name;
}

} // namespace

QString NoteWriter::libraryRoot() {
#ifdef Q_OS_ANDROID
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    const QString root = base + QStringLiteral("/BlopNotizen");
    QDir().mkpath(root);
    return root;
}

QString NoteWriter::write(const QString &title, const QString &body, QString *error) {
    const QString root = libraryRoot();
    if (root.isEmpty()) {
        if (error)
            *error = QStringLiteral("Der Notiz-Ordner fehlt.");
        return {};
    }

    const QString stem = safeStem(title);
    QString path = root + QLatin1Char('/') + stem + QStringLiteral(".bnote");
    int counter = 2;
    while (QFileInfo::exists(path)) {
        path = root + QLatin1Char('/') + stem + QStringLiteral(" (") +
               QString::number(counter++) + QStringLiteral(").bnote");
    }

    // .bnote is the multi-page JSON Blop opens. .blop is the older canvas file.
    QJsonObject text;
    QJsonArray texts;
    if (!body.trimmed().isEmpty()) {
        text.insert(QStringLiteral("x"), 48);
        text.insert(QStringLiteral("y"), 56);
        text.insert(QStringLiteral("w"), 620);
        text.insert(QStringLiteral("text"), body);
        text.insert(QStringLiteral("c"), QStringLiteral("#ff111111"));
        text.insert(QStringLiteral("font"), QString());
        text.insert(QStringLiteral("size"), 16);
        texts.append(text);
    }

    QJsonObject page;
    page.insert(QStringLiteral("strokes"), QJsonArray());
    page.insert(QStringLiteral("graphs"), QJsonArray());
    page.insert(QStringLiteral("stickies"), QJsonArray());
    page.insert(QStringLiteral("texts"), texts);
    page.insert(QStringLiteral("bg"), 2);
    page.insert(QStringLiteral("title"), QStringLiteral("Seite 1"));
    page.insert(QStringLiteral("rot"), 0);
    page.insert(QStringLiteral("bm"), false);
    page.insert(QStringLiteral("paper"), QStringLiteral("#ffffff"));

    QJsonArray pages;
    pages.append(page);

    QJsonObject cover;
    cover.insert(QStringLiteral("bg"), 2);
    cover.insert(QStringLiteral("paper"), QStringLiteral("#ffffff"));

    QJsonArray tags;
    tags.append(QStringLiteral("assistent"));

    QJsonObject rootObj;
    rootObj.insert(QStringLiteral("id"), QUuid::createUuid().toString());
    rootObj.insert(QStringLiteral("title"), title.trimmed().isEmpty() ? stem : title.trimmed());
    rootObj.insert(QStringLiteral("tags"), tags);
    rootObj.insert(QStringLiteral("cover"), cover);
    rootObj.insert(QStringLiteral("pages"), pages);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Die Notiz konnte nicht gespeichert werden.");
        return {};
    }
    file.write(QJsonDocument(rootObj).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Die Notiz konnte nicht gespeichert werden.");
        return {};
    }
    return path;
}
