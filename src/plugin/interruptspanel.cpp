#include "interruptspanel.h"

#include <QCheckBox>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "colourdialog.h"
#include "controls.h"
#include "document.h"
#include "penbutton.h"
#include "pictures.h"
#include "prompts.h"
#include "texts.h"

namespace tuxscr {

InterruptsPanel::InterruptsPanel(Document* document, QWidget* parent)
    : QWidget(parent)
    , document_(document)
{
    setObjectName(QStringLiteral("Interrupts"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto* top = new QHBoxLayout;
    enable_ = namedCheck("chkInterrupts");
    add_ = namedButton("bpAdd");
    apply_ = namedButton("bpApply");
    export_ = namedButton("bpExport");
    top->addWidget(enable_);
    top->addSpacing(12);
    top->addWidget(add_);
    top->addWidget(apply_);
    top->addWidget(export_);
    top->addStretch();
    layout->addLayout(top);

    hint_ = new QLabel;
    hint_->setObjectName(QStringLiteral("lblHint"));
    hint_->setWordWrap(true);
    layout->addWidget(hint_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    rowsHolder_ = new QWidget;
    rowsLayout_ = new QVBoxLayout(rowsHolder_);
    rowsLayout_->setContentsMargins(0, 0, 0, 0);
    rowsLayout_->setSpacing(4);
    rowsLayout_->addStretch();
    scroll->setWidget(rowsHolder_);
    layout->addWidget(scroll, 1);

    connect(enable_, &QCheckBox::clicked, this, [this](bool on) {
        document_->screen.interruptsOn = on;
        document_->screen.tidyInterrupts();
        document_->touch();
    });
    connect(add_, &QPushButton::clicked, this, &InterruptsPanel::addInterrupt);
    connect(apply_, &QPushButton::clicked, this, [this] {
        document_->screen.tidyInterrupts();
        document_->convert(true);
    });
    connect(export_, &QPushButton::clicked, this, &InterruptsPanel::exportText);
    applyTexts();
    refresh();
}

bool InterruptsPanel::possible() const
{
    return document_->screen.plus && document_->screen.mode <= kMode2;
}

void InterruptsPanel::applyTexts()
{
    texts::apply(this, "Interrupts");
    for (const Row& row : rows_) {
        row.line->setToolTip(texts::text("Interrupts.tipLine"));
        row.search->setToolTip(texts::text("Interrupts.tipAuto"));
        row.search->setText(texts::text("Interrupts.bpAuto"));
        row.remove->setText(texts::text("Interrupts.bpRemove"));
        for (PenButton* pen : row.pens)
            if (pen)
                pen->setToolTip(texts::text("Interrupts.tipPen"));
    }
    refresh();
}

QList<int> InterruptsPanel::lines() const
{
    QList<int> all;
    if (possible() && document_->screen.interruptsOn)
        for (const PaletteInterrupt& interrupt : document_->screen.interrupts)
            all << interrupt.line;
    return all;
}

int InterruptsPanel::currentLine() const
{
    const std::vector<PaletteInterrupt>& interrupts = document_->screen.interrupts;
    if (!possible() || !document_->screen.interruptsOn || current_ < 0 || current_ >= static_cast<int>(interrupts.size()))
        return -1;
    return interrupts[static_cast<size_t>(current_)].line;
}

void InterruptsPanel::buildRows()
{
    for (const Row& row : rows_)
        delete row.widget;
    rows_.clear();
    const int count = static_cast<int>(document_->screen.interrupts.size());
    for (int r = 0; r < count; ++r) {
        Row row;
        row.widget = new QWidget;
        row.widget->setObjectName(QStringLiteral("interrupt%1").arg(r));
        auto* line = new QHBoxLayout(row.widget);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(6);
        // Which one is being set: an arrow before it.
        row.marker = new QLabel;
        row.marker->setFixedWidth(14);
        line->addWidget(row.marker);
        auto* label = namedLabel("lblLine", texts::text("Interrupts.lblLine"));
        line->addWidget(label);
        row.line = namedSpin("numLine", 1, kMaxLines - 1, 1);
        row.line->setToolTip(texts::text("Interrupts.tipLine"));
        label->setBuddy(row.line);
        line->addWidget(row.line);
        for (int pen = 0; pen < 16; ++pen) {
            row.pens[pen] = new PenButton(pen);
            row.pens[pen]->setObjectName(QStringLiteral("pen%1").arg(pen));
            row.pens[pen]->setToolTip(texts::text("Interrupts.tipPen"));
            line->addWidget(row.pens[pen]);
            connect(row.pens[pen], &QAbstractButton::clicked, this, [this, r, pen] { penClicked(r, pen, false); });
            connect(row.pens[pen], &PenButton::rightClicked, this, [this, r, pen] { penClicked(r, pen, true); });
        }
        row.search = namedButton("bpAuto", texts::text("Interrupts.bpAuto"));
        // Pressed in while the interrupt's lines are converted with colours
        // of their own.
        row.search->setCheckable(true);
        row.search->setToolTip(texts::text("Interrupts.tipAuto"));
        row.remove = namedButton("bpRemove", texts::text("Interrupts.bpRemove"));
        line->addWidget(row.search);
        line->addWidget(row.remove);
        line->addStretch();
        rowsLayout_->insertWidget(r, row.widget);
        // Added to a panel that is shown already, it is to be shown itself.
        row.widget->show();
        connect(row.line, &QSpinBox::valueChanged, this, [this, r](int value) { lineChanged(r, value); });
        connect(row.search, &QPushButton::clicked, this, [this, r](bool on) {
            setCurrent(r);
            Screen& screen = document_->screen;
            if (r >= static_cast<int>(screen.interrupts.size()))
                return;
            screen.tidyInterrupts();
            if (on) {
                document_->convert(true, false, r);
            } else {
                // Its lines are converted with the palette of above again.
                // The colours found go with that; those chosen by hand
                // since stay.
                PaletteInterrupt& interrupt = screen.interrupts[static_cast<size_t>(r)];
                interrupt.matched = false;
                for (size_t pen = 0; pen < 16; ++pen)
                    if (interrupt.colours[pen] == interrupt.matchColours[pen])
                        interrupt.defined[pen] = false;
                screen.tidyInterrupts();
                document_->convert(true);
            }
        });
        connect(row.remove, &QPushButton::clicked, this, [this, r] { removeInterrupt(r); });
        // The one whose controls are used is the one being set.
        row.line->installEventFilter(this);
        rows_ << row;
    }
}

void InterruptsPanel::refresh()
{
    Screen& screen = document_->screen;
    const bool can = possible();
    const bool on = can && screen.interruptsOn;
    const int count = static_cast<int>(screen.interrupts.size());
    if (rows_.size() != count)
        buildRows();
    if (current_ >= count)
        current_ = count - 1;
    if (current_ < 0 && count > 0)
        current_ = 0;

    const bool wasBlocked = enable_->blockSignals(true);
    enable_->setChecked(screen.interruptsOn);
    enable_->blockSignals(wasBlocked);
    enable_->setEnabled(can);
    add_->setEnabled(on);
    apply_->setEnabled(on);
    export_->setEnabled(on && count > 0);
    hint_->setText(!can ? texts::text("Interrupts.onlyPlus") : (on && count == 0 ? texts::text("Interrupts.none") : QString()));
    hint_->setVisible(!hint_->text().isEmpty());
    rowsHolder_->setEnabled(on);

    const int maxPen = screen.maxPen();
    for (int r = 0; r < count; ++r) {
        const PaletteInterrupt& interrupt = screen.interrupts[static_cast<size_t>(r)];
        const Row& row = rows_[r];
        const bool blocked = row.line->blockSignals(true);
        row.line->setMaximum(std::max(1, screen.lines - 1));
        row.line->setValue(interrupt.line);
        row.line->blockSignals(blocked);
        row.marker->setText(r == current_ ? QStringLiteral("▶") : QString());
        const bool wasSearching = row.search->blockSignals(true);
        row.search->setChecked(interrupt.matched);
        row.search->blockSignals(wasSearching);
        const std::array<int, 16> pens = screen.paletteAt(interrupt.line);
        for (int pen = 0; pen < 16; ++pen) {
            const size_t p = static_cast<size_t>(pen);
            // Its own colours, whether the interrupts are in use or not.
            const int colour = interrupt.defined[p] ? interrupt.colours[p] : (on ? pens[p] : interrupt.colours[p]);
            row.pens[pen]->setColour(toQColor(screen.rgb(colour)));
            row.pens[pen]->setMark(interrupt.defined[p] ? PenButton::Mark::Dot : PenButton::Mark::None);
            row.pens[pen]->setVisible(pen < maxPen);
            row.pens[pen]->setAccessibleName(tr("Line %1, pen %2").arg(interrupt.line).arg(pen));
        }
    }
    emit marksChanged();
}

bool InterruptsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusIn)
        for (int r = 0; r < rows_.size(); ++r)
            if (rows_[r].line == watched)
                setCurrent(r);
    return QWidget::eventFilter(watched, event);
}

void InterruptsPanel::setCurrent(int row)
{
    if (current_ == row)
        return;
    current_ = row;
    for (int r = 0; r < rows_.size(); ++r)
        rows_[r].marker->setText(r == current_ ? QStringLiteral("▶") : QString());
    emit marksChanged();
}

void InterruptsPanel::addInterrupt()
{
    Screen& screen = document_->screen;
    PaletteInterrupt interrupt;
    // Half way between the last one and the bottom of the picture.
    const int last = screen.interrupts.empty() ? 0 : screen.interrupts.back().line;
    interrupt.line = std::clamp((last + screen.lines) / 2, 1, std::max(1, screen.lines - 1));
    screen.interrupts.push_back(interrupt);
    screen.tidyInterrupts();
    current_ = static_cast<int>(screen.interrupts.size()) - 1;
    document_->touch();
    if (current_ < rows_.size())
        rows_[current_].line->setFocus();
}

void InterruptsPanel::removeInterrupt(int row)
{
    Screen& screen = document_->screen;
    if (row < 0 || row >= static_cast<int>(screen.interrupts.size()))
        return;
    screen.interrupts.erase(screen.interrupts.begin() + row);
    screen.tidyInterrupts();
    if (current_ >= row)
        current_ = std::max(0, current_ - 1);
    document_->touch();
}

void InterruptsPanel::lineChanged(int row, int line)
{
    Screen& screen = document_->screen;
    if (row < 0 || row >= static_cast<int>(screen.interrupts.size()))
        return;
    screen.interrupts[static_cast<size_t>(row)].line = line;
    // The interrupts are kept by rising line: the one being moved may pass
    // another, and is followed to its new place.
    int place = row;
    while (place > 0 && screen.interrupts[static_cast<size_t>(place) - 1].line > line) {
        std::swap(screen.interrupts[static_cast<size_t>(place)], screen.interrupts[static_cast<size_t>(place) - 1]);
        --place;
    }
    while (place + 1 < static_cast<int>(screen.interrupts.size()) && screen.interrupts[static_cast<size_t>(place) + 1].line < line) {
        std::swap(screen.interrupts[static_cast<size_t>(place)], screen.interrupts[static_cast<size_t>(place) + 1]);
        ++place;
    }
    screen.tidyInterrupts();
    current_ = place;
    document_->touch();
    if (place != row && place < rows_.size())
        rows_[place].line->setFocus();
}

void InterruptsPanel::penClicked(int row, int pen, bool right)
{
    Screen& screen = document_->screen;
    if (row < 0 || row >= static_cast<int>(screen.interrupts.size()))
        return;
    setCurrent(row);
    PaletteInterrupt& interrupt = screen.interrupts[static_cast<size_t>(row)];
    const size_t p = static_cast<size_t>(pen);
    if (right && interrupt.matched) {
        // The colour that was found for it, again.
        interrupt.colours[p] = interrupt.matchColours[p];
    } else if (right) {
        // The colour the pen has above, again.
        interrupt.defined[p] = false;
    } else {
        int colour = screen.paletteAt(interrupt.line)[p];
        if (!interrupt.defined[p])
            colour = interrupt.colours[p];
        if (!ColourDialog::choose(this, pen, colour, true))
            return;
        interrupt.colours[p] = colour;
        interrupt.defined[p] = true;
    }
    screen.tidyInterrupts();
    document_->touch();
}

void InterruptsPanel::exportText()
{
    const QString path = prompts::saveName(this, texts::text("Interrupts.bpExport"), QString::fromStdString(document_->param.lastSavePath),
                                           {tr("Text (*.txt)"), tr("Assembler source (*.asm)")});
    if (path.isEmpty())
        return;
    if (document_->savePaletteInterrupts(path))
        document_->param.lastSavePath = QFileInfo(path).absolutePath().toStdString();
}

}  // namespace tuxscr
