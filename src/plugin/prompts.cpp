#include "prompts.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSaveFile>

namespace prompts {

namespace {

std::function<void(const QString&)> g_problemHandler;
FileAnswerer g_openAnswerer;
FileAnswerer g_saveAnswerer;

QString fileName(bool save, QWidget* parent, const QString& title, const QString& folder, const QStringList& filters, int* chosen)
{
    const FileAnswerer& answerer = save ? g_saveAnswerer : g_openAnswerer;
    int picked = 0;
    QString name;
    if (answerer) {
        name = answerer(title, filters, &picked);
    } else {
        QString filter = filters.value(0);
        name = save ? QFileDialog::getSaveFileName(parent, title, folder, filters.join(QStringLiteral(";;")), &filter)
                    : QFileDialog::getOpenFileName(parent, title, folder, filters.join(QStringLiteral(";;")), &filter);
        picked = std::max<int>(0, static_cast<int>(filters.indexOf(filter)));
        // A file to write is given the extension of its kind if it has none.
        if (save && !name.isEmpty() && !name.section(QLatin1Char('/'), -1).contains(QLatin1Char('.'))) {
            const int star = static_cast<int>(filter.indexOf(QLatin1String("*.")));
            if (star >= 0) {
                int end = star + 2;
                while (end < filter.size() && (filter[end].isLetterOrNumber()))
                    ++end;
                if (end > star + 2)
                    name += filter.mid(star + 1, end - star - 1);
            }
        }
    }
    if (chosen)
        *chosen = picked;
    return name;
}

}  // namespace

void problem(QWidget* parent, const QString& text)
{
    if (g_problemHandler) {
        g_problemHandler(text);
        return;
    }
    QMessageBox::warning(parent, QStringLiteral("TuxSCR"), text);
}

void inform(QWidget* parent, const QString& text)
{
    if (g_problemHandler) {
        g_problemHandler(text);
        return;
    }
    QMessageBox::information(parent, QStringLiteral("TuxSCR"), text);
}

QString openName(QWidget* parent, const QString& title, const QString& folder, const QStringList& filters, int* chosen)
{
    return fileName(false, parent, title, folder, filters, chosen);
}

QString saveName(QWidget* parent, const QString& title, const QString& folder, const QStringList& filters, int* chosen)
{
    return fileName(true, parent, title, folder, filters, chosen);
}

void setProblemHandler(std::function<void(const QString&)> handler)
{
    g_problemHandler = std::move(handler);
}

void setOpenAnswerer(FileAnswerer answerer)
{
    g_openAnswerer = std::move(answerer);
}

void setSaveAnswerer(FileAnswerer answerer)
{
    g_saveAnswerer = std::move(answerer);
}

}  // namespace prompts

std::optional<std::vector<uint8_t>> readHostFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray all = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return std::nullopt;
    return std::vector<uint8_t>(all.begin(), all.end());
}

bool writeHostFile(const QString& path, const std::vector<uint8_t>& bytes)
{
    // Written beside its place, then moved there: a file that cannot be
    // written whole leaves the one that was there.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    return file.commit();
}

bool writeHostText(const QString& path, const std::string& text)
{
    return writeHostFile(path, std::vector<uint8_t>(text.begin(), text.end()));
}
