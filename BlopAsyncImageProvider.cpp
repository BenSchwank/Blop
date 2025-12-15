#include "BlopAsyncImageProvider.h"
#include <QQuickTextureFactory>

// ============================================================================
// BlopImageResponse Implementation
// ============================================================================

BlopImageResponse::BlopImageResponse(const QString &id, const QSize &requestedSize)
{
    // Automatische Löschung des Runnables nach Ausführung
    auto *runnable = new BlopImageRunnable(id, requestedSize, this);
    QThreadPool::globalInstance()->start(runnable);
}

QQuickTextureFactory *BlopImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void BlopImageResponse::handleDone(QImage image)
{
    m_image = image;
    emit finished(); // Signal an QML: Bild ist da!
}

// ============================================================================
// BlopImageRunnable Implementation
// ============================================================================

BlopImageRunnable::BlopImageRunnable(const QString &id, const QSize &requestedSize, BlopImageResponse *response)
    : m_id(id), m_requestedSize(requestedSize), m_response(response)
{
    // Wichtig: Wir wollen nicht, dass das Runnable deleteLater aufruft,
    // da QThreadPool das übernimmt (setAutoDelete ist default true).
}

void BlopImageRunnable::run()
{
    // 1. Größe bestimmen (Fallback falls 0)
    QSize size = m_requestedSize;
    if (size.isEmpty()) size = QSize(200, 280); // Default Thumbnail Größe

    // 2. QImage erstellen (KEIN QPixmap im Worker Thread!)
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    // 3. Zeichnen (CPU heavy task)
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Simulation des Styles aus deinem PageItem.h
    int shadowOffset = 4;
    QRect drawRect(shadowOffset, shadowOffset, size.width() - shadowOffset*2, size.height() - shadowOffset*2);

    // Schatten
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawRoundedRect(drawRect.translated(2, 2), 6, 6);

    // Papierblatt
    p.setBrush(Qt::white);
    p.setPen(QPen(QColor(200, 200, 200), 1));
    p.drawRoundedRect(drawRect, 6, 6);

    // Gittermuster zeichnen (Simuliert PageBackgroundType::Grid)
    // Wir clippen auf das Papier, damit das Gitter nicht überrundet wird
    p.setClipRect(drawRect);
    p.setPen(QPen(QColor(230, 230, 245), 1));

    int gridSize = 20; // Skalierung für Thumbnail

    // Vertikale Linien
    for (int x = drawRect.left(); x < drawRect.right(); x += gridSize)
        p.drawLine(x, drawRect.top(), x, drawRect.bottom());

    // Horizontale Linien
    for (int y = drawRect.top(); y < drawRect.bottom(); y += gridSize)
        p.drawLine(drawRect.left(), y, drawRect.right(), y);

    // Optional: Hier könnte man Text oder Skizzen-Inhalte aus 'm_id' (Dateipfad) laden
    // und rendern, solange man keine GUI-Klassen verwendet.

    p.end();

    // 4. Ergebnis an den Main-Thread übergeben
    // Wir nutzen invokeMethod mit QueuedConnection, um Thread-Grenzen sicher zu überqueren.
    if (m_response) {
        QMetaObject::invokeMethod(m_response, "handleDone",
                                  Qt::QueuedConnection,
                                  Q_ARG(QImage, image));
    }
}

// ============================================================================
// BlopAsyncImageProvider Implementation
// ============================================================================

QQuickImageResponse *BlopAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new BlopImageResponse(id, requestedSize);
}
