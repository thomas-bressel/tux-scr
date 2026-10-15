#include "patterndialog.h"

#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>

#include "contrast.h"
#include "controls.h"
#include "converterwindow.h"
#include "engine/convert.h"
#include "penbutton.h"
#include "pictures.h"
#include "prompts.h"
#include "texts.h"

using namespace tuxscr;

namespace {

constexpr int kCell = 60;    // a pixel of the pattern being edited
constexpr int kSmall = 8;    // a pixel of a pattern of the row

QString both(const char* french, const char* englishText)
{
    return QString::fromUtf8(texts::language() == QLatin1String("EN") ? englishText : french);
}

}  // namespace

PatternDialog::PatternDialog(ConverterWindow* window)
    : QDialog(window)
    , window_(window)
    , document_(window->document())
{
    setObjectName(QStringLiteral("EditTrameAscii"));
    setWindowTitle(texts::text("Main.bpEditTrame"));
    auto* grid = new QGridLayout(this);
    all_ = new QWidget;
    all_->setObjectName(QStringLiteral("pictAllMatrice"));
    all_->setFixedSize(16 * 4 * kSmall + 1, 4 * kSmall + 12);
    all_->installEventFilter(this);
    grid->addWidget(all_, 0, 0, 1, 4);

    edit_ = new QWidget;
    edit_->setObjectName(QStringLiteral("pictEditMatrice"));
    edit_->setFixedSize(4 * kCell + 1, 4 * kCell + 1);
    edit_->setMouseTracking(true);
    edit_->installEventFilter(this);
    grid->addWidget(edit_, 1, 0, 6, 1);

    bpPrev_ = namedButton("bpPrev");
    bpSuiv_ = namedButton("bpSuiv");
    grid->addWidget(bpPrev_, 1, 1);
    grid->addWidget(bpSuiv_, 1, 2);
    auto* pens = new QHBoxLayout;
    for (int pen = 0; pen < 4; ++pen) {
        pens_[pen] = new PenButton(pen);
        pens_[pen]->setObjectName(QStringLiteral("lblPen%1").arg(pen));
        pens->addWidget(pens_[pen]);
        connect(pens_[pen], &QAbstractButton::clicked, this, [this, pen] {
            penLeft_ = pen;
            showAll();
        });
        connect(pens_[pen], &PenButton::rightClicked, this, [this, pen] {
            penRight_ = pen;
            showAll();
        });
    }
    pens->addStretch();
    grid->addLayout(pens, 2, 1, 1, 3);
    auto* label1 = namedLabel("label1");
    auto* label2 = namedLabel("label2");
    lblPenLeft_ = namedLabel("lblPenLeft");
    lblPenRight_ = namedLabel("lblPenRight");
    for (QLabel* label : {lblPenLeft_, lblPenRight_}) {
        label->setFixedSize(46, 24);
        label->setAlignment(Qt::AlignCenter);
        label->setAutoFillBackground(true);
        label->setFrameShape(QFrame::Box);
    }
    grid->addWidget(label1, 3, 1);
    grid->addWidget(lblPenLeft_, 3, 2);
    grid->addWidget(label2, 4, 1);
    grid->addWidget(lblPenRight_, 4, 2);
    auto* bpRead = namedButton("bpRead");
    auto* bpSave = namedButton("bpSave");
    auto* bpAutoGene = namedButton("bpAutoGene");
    auto* bpCopyTrame = namedButton("bpCopyTrame");
    auto* bpForceCalc = namedButton("bpForceCalc");
    numTabTrame_ = namedSpin("numTabTrame", 0, 7, 0);
    grid->addWidget(bpRead, 5, 1);
    grid->addWidget(bpSave, 5, 2);
    grid->addWidget(bpAutoGene, 5, 3);
    grid->addWidget(bpCopyTrame, 6, 1);
    grid->addWidget(numTabTrame_, 6, 2);
    grid->addWidget(bpForceCalc, 6, 3);
    texts::apply(this, "EditTrameAscii");
    numTabTrame_->setAccessibleName(both("Jeu de trames prédéfini", "Ready-made set of patterns"));
    edit_->setAccessibleName(both("Trame en cours d'édition", "Pattern being edited"));
    all_->setAccessibleName(both("Les seize trames", "The sixteen patterns"));

    connect(bpPrev_, &QPushButton::clicked, this, [this] {
        current_ = std::max(0, current_ - 1);
        showAll();
    });
    connect(bpSuiv_, &QPushButton::clicked, this, [this] {
        current_ = std::min(15, current_ + 1);
        showAll();
    });
    connect(bpRead, &QPushButton::clicked, this, &PatternDialog::readPatterns);
    connect(bpSave, &QPushButton::clicked, this, &PatternDialog::savePatterns);
    connect(bpAutoGene, &QPushButton::clicked, this, &PatternDialog::makePatterns);
    connect(bpCopyTrame, &QPushButton::clicked, this, [this] {
        document_->screen.copyPatterns(numTabTrame_->value());
        showAll();
    });
    connect(bpForceCalc, &QPushButton::clicked, this, [this] { document_->convert(true); });
    showAll();
}

QPoint PatternDialog::cellCentre(int x, int y) const
{
    return QPoint(x * kCell + kCell / 2, y * kCell + kCell / 2);
}

QPoint PatternDialog::patternCentre(int pattern) const
{
    return QPoint(pattern * 4 * kSmall + 2 * kSmall, 2 * kSmall);
}

void PatternDialog::showAll()
{
    const Screen& screen = document_->screen;
    bpPrev_->setEnabled(current_ > 0);
    bpSuiv_->setEnabled(current_ < 15);
    for (int pen = 0; pen < 4; ++pen)
        pens_[pen]->setColour(toQColor(screen.penColour(pen)));
    const struct {
        QLabel* label;
        int pen;
    } shown[2] = {{lblPenLeft_, penLeft_}, {lblPenRight_, penRight_}};
    for (const auto& one : shown) {
        const QColor colour = toQColor(screen.penColour(one.pen));
        QPalette look = one.label->palette();
        look.setColor(QPalette::Window, colour);
        look.setColor(QPalette::WindowText, readable(inkOn(colour), colour));
        one.label->setPalette(look);
        one.label->setText(QString::number(one.pen));
    }
    all_->update();
    edit_->update();
}

void PatternDialog::paintAll()
{
    QPainter painter(all_);
    const Screen& screen = document_->screen;
    for (int i = 0; i < 16; ++i)
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                painter.fillRect((x + i * 4) * kSmall, y * kSmall, x == 3 ? kSmall - 1 : kSmall, kSmall,
                                 toQColor(screen.penColour(screen.patterns[i][x][y] & 3)));
    // The one being edited: a bar under it, in the colour of what is
    // selected.
    painter.fillRect(current_ * 4 * kSmall, 4 * kSmall + 3, 4 * kSmall - 1, 6,
                     readable(all_->palette().color(QPalette::Highlight), all_->palette().color(QPalette::Window), kGraphicContrast));
}

void PatternDialog::paintEdit()
{
    QPainter painter(edit_);
    const Screen& screen = document_->screen;
    QFont small = edit_->font();
    small.setBold(true);
    painter.setFont(small);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const int pen = screen.patterns[current_][x][y] & 3;
            const QRect cell(x * kCell, y * kCell, kCell - 2, kCell - 2);
            painter.fillRect(cell, toQColor(screen.penColour(pen)));
            // The pen's number, on a plate: two pens of colours close to
            // each other are told apart.
            QRect plate(0, 0, 18, 16);
            plate.moveCenter(cell.center());
            painter.fillRect(plate, edit_->palette().color(QPalette::Base));
            painter.setPen(edit_->palette().color(QPalette::Text));
            painter.drawText(plate, Qt::AlignCenter, QString::number(pen));
        }
}

bool PatternDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Paint) {
        if (watched == all_)
            paintAll();
        else if (watched == edit_)
            paintEdit();
        return true;
    }
    if (watched == all_ && event->type() == QEvent::MouseButtonPress) {
        const int pattern = static_cast<QMouseEvent*>(event)->position().toPoint().x() / (4 * kSmall);
        if (pattern >= 0 && pattern < 16) {
            current_ = pattern;
            showAll();
        }
        return true;
    }
    if (watched == edit_ && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove)) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint at = mouse->position().toPoint();
        const int x = at.x() / kCell, y = at.y() / kCell;
        if (at.x() >= 0 && at.y() >= 0 && x < 4 && y < 4 && (mouse->buttons() & (Qt::LeftButton | Qt::RightButton))) {
            document_->screen.patterns[current_][x][y] = static_cast<uint8_t>(mouse->buttons() & Qt::LeftButton ? penLeft_ : penRight_);
            showAll();
        }
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

// The patterns as a file: what C# writes of 256 bytes, pattern after
// pattern, each line after line.
void PatternDialog::readPatterns()
{
    const QString path = prompts::openName(this, texts::text("EditTrameAscii.bpRead"), QString::fromStdString(document_->param.lastReadPath),
                                           {both("Modèle trames (*.xml)", "Patterns (*.xml)")});
    if (path.isEmpty())
        return;
    const auto file = readHostFile(path);
    QByteArray bytes;
    if (file) {
        const QByteArray xml(reinterpret_cast<const char*>(file->data()), static_cast<qsizetype>(file->size()));
        const qsizetype start = xml.indexOf("<base64Binary");
        const qsizetype from = start < 0 ? -1 : xml.indexOf('>', start) + 1;
        const qsizetype to = xml.indexOf("</base64Binary>");
        if (from > 0 && to > from)
            bytes = QByteArray::fromBase64(xml.mid(from, to - from).trimmed());
    }
    if (bytes.size() < 256) {
        document_->fail(both("Impossible de lire la trame.", "The patterns could not be read."));
        return;
    }
    int pos = 0;
    for (int i = 0; i < 16; ++i)
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                document_->screen.patterns[i][x][y] = static_cast<uint8_t>(bytes[pos++]) & 3;
    showAll();
}

void PatternDialog::savePatterns()
{
    const QString path = prompts::saveName(this, texts::text("EditTrameAscii.bpSave"), QString::fromStdString(document_->param.lastSavePath),
                                           {both("Modèle trames (*.xml)", "Patterns (*.xml)")});
    if (path.isEmpty())
        return;
    QByteArray bytes;
    for (int i = 0; i < 16; ++i)
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                bytes.append(static_cast<char>(document_->screen.patterns[i][x][y]));
    const QByteArray xml = "<?xml version=\"1.0\"?>\r\n<base64Binary xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                           "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
        + bytes.toBase64() + "</base64Binary>";
    if (!writeHostFile(path, std::vector<uint8_t>(xml.begin(), xml.end())))
        document_->fail(both("Impossible de sauver la trame.", "The patterns could not be saved."));
}

void PatternDialog::makePatterns()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::vector<Pattern> found;
    for (int i = 0; i < document_->frameCount(); ++i) {
        Bitmap source = resizeSource(document_->source(i), document_->screen, document_->param);
        findPatterns(source, document_->screen, document_->param, found);
        std::stable_sort(found.begin(), found.end(), [](const Pattern& a, const Pattern& b) { return a.found > b.found; });
    }
    for (size_t i = 0; i < std::min<size_t>(16, found.size()); ++i)
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                document_->screen.patterns[i][x][y] = found[i].pens[x][y];
    QApplication::restoreOverrideCursor();
    showAll();
    document_->touch();
}
