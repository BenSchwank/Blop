#include "BlopAsyncImageProvider.h"
#include "BlopImageResponse.h"

QQuickImageResponse *BlopAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // Erzeugt für jede Bildanfrage ein neues Response-Objekt
    return new BlopImageResponse(id, requestedSize);
}
