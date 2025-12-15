#include "BlopImageResponse.h"
#include <QDebug>

// =============================================================================
// ImageGenerationJob Implementation (läuft im Worker-Thread)
// =============================================================================

ImageGenerationJob::ImageGenerationJob(QSize size, QColor bgColor, QColor accentColor, BlopImageResponse* response)
    : m_size(size), m_bgColor(bgColor), m_accentColor(accentColor), m_response(response)
{
    setAutoDelete(true);
}

void ImageGenerationJob::run()
{
    // Hier findet die rechenintensive Arbeit statt, die vorher den UI-Thread blockiert hat.

    // 1. QImage erstellen (muss QImage sein, da QPixmap an GUI-Thread gebunden ist)
    QImage image(m_size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    // 2. Rendering-Logik (Simulierte Darstellung aus ModernItemDelegate)
    // Hier sollten Sie die tatsächliche Zeichnungslogik für Ihre Notiz-Vorschau einfügen.
    // (Ausschnitt basierend auf der Logik in mainwindow.cpp für ModernItemDelegate)

    QRect rect(0, 0, m_size.width(), m_size.height());
    QRect adjustedRect = rect.adjusted(4, 4, -4, -4);

    // a) Einfacher Offset-Schatten
    p.setBrush(QColor(0, 0, 0, 80));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(adjustedRect.translated(2, 2), 8, 8);

    // b) Haupt-Hintergrund (z.B. aus der Farbe des UI-Profils)
    p.setBrush(m_bgColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(adjustedRect, 8, 8);

    // c) Simulierter Akzent/Hover-Effekt Rahmen (optional)
    p.setPen(QPen(m_accentColor, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(adjustedRect.adjusted(1, 1, -1, -1), 8, 8);

    // 3. Ergebnis an den UI-Thread senden
    QMetaObject::invokeMethod(m_response, "handleFinished", Qt::QueuedConnection, Q_ARG(QImage, image));
}


// =============================================================================
// BlopImageResponse Implementation (kommuniziert mit UI-Thread)
// =============================================================================

BlopImageResponse::BlopImageResponse(QSize size, QColor bgColor, QColor accentColor)
    : m_job(new ImageGenerationJob(size, bgColor, accentColor, this))
{
    // Startet den Worker-Job im globalen Thread Pool.
    // Dies blockiert NICHT den UI-Thread.
    QThreadPool::globalInstance()->start(m_job);
}

BlopImageResponse::~BlopImageResponse()
{
    // Der Job ist AutoDelete, es muss kein manuelles Löschen erfolgen.
}

QQuickTextureFactory* BlopImageResponse::textureFactory() const
{
    // WICHTIG: Erstellen des TextureFactory MUSS im GUI-Thread erfolgen!
    // Die Konvertierung von QImage zu QQuickTextureFactory::Texture erfolgt hier.
    if (m_image.isNull())
        return nullptr;

    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void BlopImageResponse::handleFinished(const QImage& result)
{
    // DIESER SLOT WIRD DANK Qt::QueuedConnection IM GUI-THREAD AUSGEFÜHRT!
    m_image = result;
    // Informiert die QML-Engine, dass das Bild fertig ist.
    emit finished();
}
