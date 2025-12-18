#include "BlopImageResponse.h"
#include <QThreadPool>
#include <QImageReader>
#include <QUrl>
#include <QDebug>

// --- BlopImageResponse Implementierung ---

BlopImageResponse::BlopImageResponse(const QString &id, const QSize &requestedSize)
{
    // Starte den Worker sofort
    BlopImageRunnable *runnable = new BlopImageRunnable(id, requestedSize, this);
    // Qt kümmert sich um das Löschen des Runnables nach 'run()'
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
    emit finished(); // Sag QML Bescheid, dass wir fertig sind
}

// --- BlopImageRunnable Implementierung ---

BlopImageRunnable::BlopImageRunnable(const QString &id, const QSize &requestedSize, BlopImageResponse *response)
    : m_id(id), m_requestedSize(requestedSize), m_response(response)
{
}

void BlopImageRunnable::run()
{
    // Pfad bereinigen (file:/// entfernen für lokale Dateien)
    QString cleanPath = m_id;
    if (cleanPath.startsWith("file:///")) {
#ifdef Q_OS_WIN
        cleanPath = cleanPath.mid(8); // "file:///C:/..." -> "C:/..."
#else
        cleanPath = cleanPath.mid(7); // "file:///home..." -> "/home..."
#endif
    }

    QImageReader reader(cleanPath);

    // Performance: Wenn QML eine Größe wünscht, laden wir das Bild direkt kleiner!
    if (m_requestedSize.isValid()) {
        reader.setScaledSize(m_requestedSize);
    }

    QImage image = reader.read();

    if (image.isNull()) {
        // Fallback: Leeres Bild, falls Ladefehler
        image = QImage(m_requestedSize.isValid() ? m_requestedSize : QSize(50, 50), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
    }

    // Übergabe an den Haupt-Thread (Thread-Safe!)
    if (m_response) {
        // Wir nutzen invokeMethod, damit 'handleDone' sicher im UI-Thread läuft
        QMetaObject::invokeMethod(m_response.data(), [=]() {
            if (m_response) m_response->handleDone(image);
        });
    }
}
