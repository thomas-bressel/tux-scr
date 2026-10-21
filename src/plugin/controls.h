#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QString>

namespace tuxscr {

// What the controls of TuxSCR's windows are made with. A control is known
// by its object name, which is ConvImgCpc's name for it: that is what gives
// it its text, in French or in English (texts.h), and what a test finds it
// by.

// A button of a window. Return does not press it: a window's buttons are
// many, and none of them is the window's own.
inline QPushButton* namedButton(const char* name, const QString& text = QString())
{
    auto* button = new QPushButton(text);
    button->setObjectName(QLatin1String(name));
    button->setAutoDefault(false);
    return button;
}

inline QCheckBox* namedCheck(const char* name, const QString& text = QString())
{
    auto* box = new QCheckBox(text);
    box->setObjectName(QLatin1String(name));
    return box;
}

inline QRadioButton* namedRadio(const char* name, const QString& text = QString())
{
    auto* radio = new QRadioButton(text);
    radio->setObjectName(QLatin1String(name));
    return radio;
}

inline QLabel* namedLabel(const char* name, const QString& text = QString())
{
    auto* label = new QLabel(text);
    label->setObjectName(QLatin1String(name));
    return label;
}

inline QSpinBox* namedSpin(const char* name, int minimum, int maximum, int value)
{
    auto* spin = new QSpinBox;
    spin->setObjectName(QLatin1String(name));
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    spin->setAccelerated(true);
    return spin;
}

// Sets a control without its change being heard: for a window that shows
// what a file or another window set.
template <typename Control, typename Value>
void setQuietly(Control* control, void (Control::*setter)(Value), Value value)
{
    const bool blocked = control->blockSignals(true);
    (control->*setter)(value);
    control->blockSignals(blocked);
}

}  // namespace tuxscr
