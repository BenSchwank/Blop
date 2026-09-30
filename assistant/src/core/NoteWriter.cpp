#include "NoteWriter.h"

#include <QDir>
#include <QFile>
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

QString NoteWriter::pathFor(const QString &title) {
    return libraryRoot() + QLatin1Char('/') + safeStem(title) + QStringLiteral(".bnote");
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

namespace {

QJsonObject textObject(qreal y, int size, const QString &text) {
    QJsonObject obj;
    obj.insert(QStringLiteral("x"), 48);
    obj.insert(QStringLiteral("y"), y);
    obj.insert(QStringLiteral("w"), 620);
    obj.insert(QStringLiteral("text"), text);
    obj.insert(QStringLiteral("c"), QStringLiteral("#ff111111"));
    obj.insert(QStringLiteral("font"), QString());
    obj.insert(QStringLiteral("size"), size);
    return obj;
}

QJsonObject emptyNote(const QString &title) {
    QJsonObject page;
    page.insert(QStringLiteral("strokes"), QJsonArray());
    page.insert(QStringLiteral("graphs"), QJsonArray());
    page.insert(QStringLiteral("stickies"), QJsonArray());
    page.insert(QStringLiteral("texts"), QJsonArray());
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
    QJsonObject root;
    root.insert(QStringLiteral("id"), QUuid::createUuid().toString());
    root.insert(QStringLiteral("title"), title);
    root.insert(QStringLiteral("tags"), tags);
    root.insert(QStringLiteral("cover"), cover);
    root.insert(QStringLiteral("pages"), pages);
    return root;
}

} // namespace

QString NoteWriter::compose(const QString &title, const QString &heading,
                            const QStringList &points, QString *error) {
    const QString rootDir = libraryRoot();
    if (rootDir.isEmpty()) {
        if (error)
            *error = QStringLiteral("Der Notiz-Ordner fehlt.");
        return {};
    }
    const QString stem = safeStem(title);
    const QString path = pathFor(title);

    QJsonObject root;
    if (QFileInfo::exists(path)) {
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("Die Notiz konnte nicht gelesen werden.");
            return {};
        }
        root = QJsonDocument::fromJson(in.readAll()).object();
    }
    if (root.isEmpty())
        root = emptyNote(stem);

    QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
    if (pages.isEmpty())
        pages.append(emptyNote(stem).value(QStringLiteral("pages")).toArray().at(0));
    QJsonObject page = pages.at(0).toObject();
    QJsonArray texts = page.value(QStringLiteral("texts")).toArray();

    qreal y = 48;
    if (!texts.isEmpty()) {
        y = 0;
        for (const QJsonValue &value : texts) {
            const QJsonObject obj = value.toObject();
            const qreal bottom = obj.value(QStringLiteral("y")).toDouble() +
                                 qMax(28, obj.value(QStringLiteral("size")).toInt(16) + 12);
            if (bottom > y)
                y = bottom;
        }
        y += 28;
    }

    const QString head = heading.trimmed();
    if (!head.isEmpty()) {
        texts.append(textObject(y, 22, head));
        y += 40;
    }
    for (QString point : points) {
        point = point.trimmed();
        if (point.isEmpty())
            continue;
        if (!point.startsWith(QChar(0x2022)))
            point.prepend(QString(QChar(0x2022)) + QLatin1Char(' '));
        texts.append(textObject(y, 16, point));
        y += 32;
    }

    page.insert(QStringLiteral("texts"), texts);
    pages.replace(0, page);
    root.insert(QStringLiteral("pages"), pages);
    if (root.value(QStringLiteral("title")).toString().trimmed().isEmpty())
        root.insert(QStringLiteral("title"), stem);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Die Notiz konnte nicht gespeichert werden.");
        return {};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Die Notiz konnte nicht gespeichert werden.");
        return {};
    }
    return path;
}
