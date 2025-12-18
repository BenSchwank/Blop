#pragma once

#include <QQuickAsyncImageProvider>

class BlopAsyncImageProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
