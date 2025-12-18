#pragma once

#include <QQuickImageResponse>
#include <QRunnable>
#include <QImage>
#include <QObject>
#include <QPointer>

// Die Response-Klasse (FIX: Erbt NUR von QQuickImageResponse)
class BlopImageResponse : public QQuickImageResponse
{
    Q_OBJECT
public:
    BlopImageResponse(const QString &id, const QSize &requestedSize);

    // Liefert die fertige Textur an QML
    QQuickTextureFactory *textureFactory() const override;

    // Helper für den Worker-Thread
    void handleDone(QImage image);

private:
    QImage m_image;
};

// Der Worker-Thread (Lädt das Bild im Hintergrund)
class BlopImageRunnable : public QRunnable
{
public:
    BlopImageRunnable(const QString &id, const QSize &requestedSize, BlopImageResponse *response);

    void run() override;

private:
    QString m_id;
    QSize m_requestedSize;
    // Sicherer Pointer, falls die Response gelöscht wird bevor der Thread fertig ist
    QPointer<BlopImageResponse> m_response;
};
