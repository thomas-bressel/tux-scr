#pragma once

#include <QColor>
#include <QImage>
#include <QList>

#include "base/bitmap.h"
#include "convert/param.h"
#include "convert/screen.h"

namespace tuxscr {

// Pictures between Qt and the engine, and what Qt does for the conversion:
// reading a file's pictures, and bringing one to the size of the CPC's
// screen.

tuxscr::Bitmap toBitmap(const QImage& image);
QImage toQImage(const tuxscr::Bitmap& bitmap);

inline QColor toQColor(tuxscr::Rgb colour)
{
    return QColor(colour.r, colour.g, colour.b);
}
inline QColor toQColor(int rgb)
{
    return QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

// The pictures of a file a PC shows: one, or the frames of an animated GIF.
// `time`, if given, is told how long a frame stays, in milliseconds (0 for
// a picture that does not move).
QList<QImage> readPictures(const QByteArray& file, int* time = nullptr);

// The source brought to the size of the screen on a PC, as the settings
// say: stretched to fit, kept to its proportions inside or over it, or
// placed and sized by the user. What it does not cover has pen 0's colour.
tuxscr::Bitmap resizeSource(const QImage& source, const tuxscr::Screen& screen, const tuxscr::Param& param);

}  // namespace tuxscr
