#include "pictures.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>

namespace tuxscr {

tuxscr::Bitmap toBitmap(const QImage& image)
{
    if (image.isNull())
        return {};
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
    tuxscr::Bitmap bitmap(rgb.width(), rgb.height());
    for (int y = 0; y < rgb.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        uint32_t* out = bitmap.pixels().data() + static_cast<size_t>(y) * static_cast<size_t>(rgb.width());
        for (int x = 0; x < rgb.width(); ++x)
            out[x] = row[x] & 0xFFFFFF;
    }
    return bitmap;
}

QImage toQImage(const tuxscr::Bitmap& bitmap)
{
    if (bitmap.empty())
        return {};
    QImage image(bitmap.width(), bitmap.height(), QImage::Format_RGB32);
    for (int y = 0; y < bitmap.height(); ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        const uint32_t* in = bitmap.pixels().data() + static_cast<size_t>(y) * static_cast<size_t>(bitmap.width());
        for (int x = 0; x < bitmap.width(); ++x)
            row[x] = 0xFF000000u | in[x];
    }
    return image;
}

QList<QImage> readPictures(const QByteArray& file, int* time)
{
    QList<QImage> pictures;
    QBuffer buffer;
    buffer.setData(file);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    int delay = 0;
    // A picture of 768 by 544 in every frame is far more than a CPC can
    // play: a file with frames past any use is cut there.
    for (int n = 0; n < 4096; ++n) {
        const QImage frame = reader.read();
        if (frame.isNull())
            break;
        if (n == 0)
            delay = reader.nextImageDelay();
        pictures << frame.convertToFormat(QImage::Format_RGB32);
        if (!reader.supportsAnimation() || !reader.canRead())
            break;
    }
    if (time)
        *time = pictures.size() > 1 ? delay : 0;
    return pictures;
}

tuxscr::Bitmap resizeSource(const QImage& source, const tuxscr::Screen& screen, const tuxscr::Param& param)
{
    using tuxscr::Param;
    const int width = screen.width();
    const int height = screen.height();
    QImage target(std::max(1, width), std::max(1, height), QImage::Format_RGB32);
    target.fill(toQColor(screen.rgb(screen.palette[0])));
    if (!source.isNull() && width > 0 && height > 0) {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const double ratio = source.width() * static_cast<double>(height) / (static_cast<double>(source.height()) * width);
        switch (param.sizeMode) {
        case Param::SizeMode::KeepSmaller:
            if (ratio < 1) {
                const int newW = static_cast<int>(width * ratio);
                painter.drawImage(QRect((width - newW) >> 1, 0, newW, height), source);
            } else {
                const int newH = static_cast<int>(height / ratio);
                painter.drawImage(QRect(0, (height - newH) >> 1, width, newH), source);
            }
            break;
        case Param::SizeMode::KeepLarger:
            if (ratio < 1) {
                const int newY = static_cast<int>(height / ratio);
                painter.drawImage(QRect(0, (height - newY) >> 1, width, newY), source);
            } else {
                const int newX = static_cast<int>(width * ratio);
                painter.drawImage(QRect((width - newX) >> 1, 0, newX, height), source);
            }
            break;
        case Param::SizeMode::Fit:
            painter.drawImage(QRect(0, 0, width, height), source);
            break;
        case Param::SizeMode::UserSize:
        case Param::SizeMode::Origin:
            painter.drawImage(QRect(-(param.posX << 1), -(param.posY << 1), param.sizeX << 1, param.sizeY << 1), source);
            break;
        }
    }
    return toBitmap(target);
}

}  // namespace tuxscr
