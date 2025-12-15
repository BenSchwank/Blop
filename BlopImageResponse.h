#pragma once
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QThreadPool>
#include <QPainter>
#include <QImage>
#include <QColor>
#include <QObject>

class BlopImageResponse; // Forward declaration

/**
 * @brief Führt die rechenintensive Bildgenerierung in einem Worker-Thread durch.
 * Ersetzt die synchrone Logik aus ModernItemDelegate::paint.
 */
class ImageGenerationJob : public QRunnable
{
public:
    ImageGenerationJob(QSize size, QColor bgColor, QColor accentColor, BlopImageResponse* response);
    void run() override;

private:
    QSize m_size;
    QColor m_bgColor;
    QColor m_accentColor;
    BlopImageResponse* m_response;
};

/**
 * @brief Kapselt die asynchrone Operation und liefert das Ergebnis an die QML-Engine.
 */
class BlopImageResponse : public QObject, public QQuickImageResponse
{
    Q_OBJECT

public:
    BlopImageResponse(QSize size, QColor bgColor, QColor accentColor);
    ~BlopImageResponse() override;

    // Liefert die fertige Textur (wird im UI-Thread aufgerufen).
    QQuickTextureFactory* textureFactory() const override;

signals:
    // Signalisiert der QML-Engine, dass das Bild fertig ist.
    void finished() override;

private slots:
    // Wird vom Worker-Thread aufgerufen (Qt::QueuedConnection), läuft im UI-Thread.
    void handleFinished(const QImage& result);

private:
    QImage m_image;
    ImageGenerationJob* m_job;
};
