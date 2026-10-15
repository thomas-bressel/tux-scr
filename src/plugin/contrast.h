#pragma once

#include <algorithm>
#include <cmath>

#include <QColor>

// The colours TuxSCR chooses itself are held to WCAG 2's level AAA, as
// TuxAPE's are: text stands out from what is behind it by 7 to 1 at least,
// and what is drawn to tell a state by 3 to 1. The colours of a CPC's
// palette are the picture's, not ours: what is written or drawn on one of
// them is fitted to it with these.

inline constexpr double kTextContrast = 7.0;
inline constexpr double kGraphicContrast = 3.0;

inline double relativeLuminance(const QColor& colour)
{
    const auto channel = [](double value) {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(colour.redF()) + 0.7152 * channel(colour.greenF()) + 0.0722 * channel(colour.blueF());
}

inline double contrastRatio(const QColor& one, const QColor& other)
{
    const double a = relativeLuminance(one), b = relativeLuminance(other);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

// Black or white: the one that reads better on a background.
inline QColor inkOn(const QColor& background)
{
    return contrastRatio(Qt::black, background) >= contrastRatio(Qt::white, background) ? QColor(Qt::black) : QColor(Qt::white);
}

// The colour nearest to `wanted`, of the same hue, that stands out from
// `against` by `ratio` at least: darker or lighter, down to black or white.
inline QColor readable(const QColor& wanted, const QColor& against, double ratio = kTextContrast)
{
    if (contrastRatio(wanted, against) >= ratio)
        return wanted;
    const QColor end = inkOn(against);
    for (int step = 1; step <= 20; ++step) {
        const double t = step / 20.0;
        const QColor tried = QColor::fromRgbF(wanted.redF() + (end.redF() - wanted.redF()) * t,
                                              wanted.greenF() + (end.greenF() - wanted.greenF()) * t,
                                              wanted.blueF() + (end.blueF() - wanted.blueF()) * t);
        if (contrastRatio(tried, against) >= ratio)
            return tried;
    }
    return end;
}
