#pragma once

#include <QString>

class QWidget;

// The windows' texts, in French and in English: ConvImgCpc's own, by the
// names it gives its controls ("Main.bpLoad"), and TuxSCR's for what it
// adds. A control is given its text by its object name.

namespace texts {

// "FR" or "EN".
void setLanguage(const QString& language);
QString language();

// The text of a name, or an empty string if it has none in the language.
QString text(const char* name);
// The text of a name, or `fallback` if it has none.
QString text(const char* name, const QString& fallback);

// One more text, or another for a name that has one: for the windows that
// are not ConvImgCpc's own. Either language may be null.
void add(const char* name, const char* french, const char* english);

// Gives every control under `root` whose object name has a text under
// `prefix` ("Main", "ImageCpc"...) that text: buttons, boxes, labels, group
// boxes.
void apply(QWidget* root, const char* prefix);

}  // namespace texts
