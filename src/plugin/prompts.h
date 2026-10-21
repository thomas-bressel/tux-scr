#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>

class QWidget;

namespace tuxscr {

// What TuxSCR's windows tell and ask in a box of their own: a problem, a
// piece of news, the name of a file to read or to write. A test that drives
// the windows has no one to answer: it says what the answers are, and notes
// what was told.

namespace prompts {

void problem(QWidget* parent, const QString& text);
void inform(QWidget* parent, const QString& text);

// The file to read: empty if the user gave up. `filters` are the kinds
// offered, as Qt writes them ("Pictures (*.png *.gif)"); `chosen`, if
// given, is told the number of the one the file was picked under.
QString openName(QWidget* parent, const QString& title, const QString& folder, const QStringList& filters, int* chosen = nullptr);
QString saveName(QWidget* parent, const QString& title, const QString& folder, const QStringList& filters, int* chosen = nullptr);

// For tests: null puts the boxes back.
void setProblemHandler(std::function<void(const QString&)> handler);
// Given the title and the filters, answers the name and the filter's number.
using FileAnswerer = std::function<QString(const QString& title, const QStringList& filters, int* chosen)>;
void setOpenAnswerer(FileAnswerer answerer);
void setSaveAnswerer(FileAnswerer answerer);

}  // namespace prompts

// Files of the PC's, read and written whole.
std::optional<std::vector<uint8_t>> readHostFile(const QString& path);
bool writeHostFile(const QString& path, const std::vector<uint8_t>& bytes);
bool writeHostText(const QString& path, const std::string& text);

}  // namespace tuxscr
