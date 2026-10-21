#pragma once

#include <span>

#include <QCoreApplication>
#include <QString>

class QWidget;

// The windows' texts: ConvImgCpc's own, by the names it gives its controls
// ("Main.bpLoad"), and TuxSCR's for what it adds. Each is kept in English,
// marked for Qt's translation in its window's context ("tuxscr::Main") with
// its name as the comment that tells it from another of the same English;
// it is given in TuxAPE's language, the one the plugin's translator was
// installed for. A control is given its text by its object name.

namespace tuxscr::texts {

// What QT_TRANSLATE_NOOP3(context, english, name) makes: a table's entry.
struct WindowText {
    const char* english;
    const char* name;
};

// The text of a name, or an empty string if it has none.
QString text(const char* name);
// The text of a name, or `fallback` if it has none.
QString text(const char* name, const QString& fallback);

// More texts, for the windows that are not ConvImgCpc's main one; a name
// that has one already is given this one. The table stays where it is.
void add(std::span<const WindowText> table);

// A window's title: the plugin's name, then what the window is.
QString windowTitle(const QString& title);

// Gives every control under `root` whose object name has a text under
// `prefix` ("Main", "ImageCpc"...) that text: buttons, boxes, labels, group
// boxes.
void apply(QWidget* root, const char* prefix);

}  // namespace tuxscr::texts
