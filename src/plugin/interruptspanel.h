#pragma once

#include <QList>
#include <QWidget>

class Document;
class PenButton;
class QCheckBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

// The palette interrupts of a Plus: lines of the picture where the pens
// take other colours. "Add Palette Interrupt" turns them on; each one added
// has its line, which a mark shows on the picture and follows as the number
// goes up or down, and beside it the pens' colours from that line on. They
// are the ones the pens have above until one is given a colour of its own
// (a dot then marks it). "Apply to Screen" converts the picture again with
// them, and "Export as Text" writes the lines and their palettes.
class InterruptsPanel : public QWidget {
    Q_OBJECT

public:
    explicit InterruptsPanel(Document* document, QWidget* parent = nullptr);

    // Shows what the document has: called when it changed.
    void refresh();
    void retranslate();

    // The lines of the interrupts in use, and the one being set (-1 for
    // none): what the picture marks.
    QList<int> lines() const;
    int currentLine() const;
    // The interrupt being set, or -1.
    int current() const { return current_; }

signals:
    // The lines or the current one changed.
    void marksChanged();

protected:
    // The interrupt whose line is being typed is the one being set.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        QWidget* widget = nullptr;
        QLabel* marker = nullptr;
        QSpinBox* line = nullptr;
        PenButton* pens[16] = {};
        QPushButton* search = nullptr;
        QPushButton* remove = nullptr;
    };

    Document* document_;
    QCheckBox* enable_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* apply_ = nullptr;
    QPushButton* export_ = nullptr;
    QWidget* rowsHolder_ = nullptr;
    QVBoxLayout* rowsLayout_ = nullptr;
    QList<Row> rows_;
    int current_ = -1;

    bool possible() const;
    void buildRows();
    void setCurrent(int row);
    void addInterrupt();
    void removeInterrupt(int row);
    void lineChanged(int row, int line);
    void penClicked(int row, int pen, bool right);
    void exportText();
};
