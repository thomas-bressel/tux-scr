#pragma once

#include <QDialog>
#include <QList>

class QRadioButton;
class QSlider;
class QSpinBox;

namespace tuxscr {

class ConverterWindow;
class Document;

// ConvImgCpc's "Paramètres internes": the weights of red, green and blue
// when two colours are compared, the levels at which a component goes from
// one of the CPC's three intensities to the next, and the K-means filter.
class InternalSettings : public QDialog {
    Q_OBJECT

public:
    explicit InternalSettings(ConverterWindow* window);

    // Shows what the settings are.
    void fromDocument();

private:
    Document* document_;
    struct Value {
        int* value;
        QSlider* slider;
        QSpinBox* box;
    };
    QList<Value> values_;
    QRadioButton* distances_[3];
    QSpinBox* numColor_;
    QSpinBox* numPass_;
    bool filling_ = false;

    void applyTexts();
};

}  // namespace tuxscr
