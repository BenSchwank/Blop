#include "BlopImageResponse.h"
#include <QThreadPool>
#include <QImageReader>
#include <QUrl>
#include <QDebug>

// --- BlopImageResponse Implementation ---

BlopImageResponse::BlopImageResponse(const QString &id, const QSize &requestedSize)
{
    BlopImageRunnable *runnable = new BlopImageRunnable(id, requestedSize, this);
    runnable->setAutoDelete(true);
    QThreadPool::globalInstance()->start(runnable);
}

QQuickTextureFactory *BlopImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void BlopImageResponse::handleDone(QImage image)
{
    m_image = image;
    emit finished();
}

// --- BlopImageRunnable Implementation ---

BlopImageRunnable::BlopImageRunnable(const QString &id, const QSize &requestedSize, BlopImageResponse *response)
    : m_id(id), m_requestedSize(requestedSize), m_response(response)
{
}

void BlopImageRunnable::run()
{
    QString cleanPath = m_id;
    if (cleanPath.startsWith("file:///")) {
#ifdef Q_OS_WIN
        cleanPath = cleanPath.mid(8);
#else
        cleanPath = cleanPath.mid(7);
#endif
    }

    QImageReader reader(cleanPath);

    if (m_requestedSize.isValid()) {
        reader.setScaledSize(m_requestedSize);
    }

    QImage image = reader.read();

    if (image.isNull()) {
        // Fallback: Transparent 50x50 image
        image = QImage(m_requestedSize.isValid() ? m_requestedSize : QSize(50, 50), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
    }

    if (m_response) {
        QMetaObject::invokeMethod(m_response.data(), [=]() {
            if (m_response) m_response->handleDone(image);
        });
    }
}
