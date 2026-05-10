#pragma once

#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QImage>
#include <QObject>
#include <QPointer>

class BlopImageResponse : public QQuickImageResponse
{
    Q_OBJECT
public:
    BlopImageResponse(const QString &id, const QSize &requestedSize);

    QQuickTextureFactory *textureFactory() const override;

    void handleDone(QImage image);

private:
    QImage m_image;
};

class BlopImageRunnable : public QRunnable
{
public:
    BlopImageRunnable(const QString &id, const QSize &requestedSize, BlopImageResponse *response);

    void run() override;

private:
    QString m_id;
    QSize m_requestedSize;
    QPointer<BlopImageResponse> m_response;
};
