#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QThreadPool>
#include <QImage>
#include <QPainter>
#include <QObject>
#include <QtQuick/qquickimageprovider.h>

// Die Response-Klasse kapselt das Ergebnis und signalisiert, wann es fertig ist.
class BlopImageResponse : public QQuickImageResponse
{
    Q_OBJECT
public:
    BlopImageResponse(const QString &id, const QSize &requestedSize);

    // Wird von der QML Engine aufgerufen, um die Textur zu holen
    QQuickTextureFactory *textureFactory() const override;

public slots:
    // Wird vom Worker-Thread via QMetaObject::invokeMethod aufgerufen
    void handleDone(QImage image);

private:
    QImage m_image;
};

// Der Worker, der die CPU-intensiven Zeichenoperationen übernimmt.
class BlopImageRunnable : public QRunnable
{
public:
    BlopImageRunnable(const QString &id, const QSize &requestedSize, BlopImageResponse *response);

    void run() override;

private:
    QString m_id;
    QSize m_requestedSize;
    // Pointer auf Response (Thread-safe behandelt via invokeMethod)
    QPointer<BlopImageResponse> m_response;
};

// Der Provider, der in der Engine registriert wird.
class BlopAsyncImageProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
