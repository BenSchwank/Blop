#include "BlopAsyncImageProvider.h"
#include "BlopImageResponse.h"

QQuickImageResponse *BlopAsyncImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new BlopImageResponse(id, requestedSize);
}
