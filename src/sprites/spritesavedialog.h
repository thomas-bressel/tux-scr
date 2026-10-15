#pragma once

#include <QDialog>

#include "spritefiles.h"

class QCheckBox;
class QLineEdit;
class QSpinBox;

// The labels a source gives the sprites it holds (ConvImgCpc's SaveMedia,
// as its sprite editor asks it): one for the sprites, one for their palette
// and, when each sprite is packed by itself, one for the table of their
// addresses, which may end with a zero. When the sprites are packed as one
// block, how many of them are to be saved is asked too.
class SpriteSaveDialog : public QDialog {
public:
    // `name` is the file's, which the labels are made of. `countLabel`, if
    // not empty, has the number of sprites asked, `count` of `most` at first.
    SpriteSaveDialog(const QString& kind, const QString& name, bool withPalette, bool withPointers, const QString& countLabel,
                     int count, int most, QWidget* parent = nullptr);

    tuxscr::SpriteLabels labels() const;
    int count() const;

private:
    QCheckBox* chkMedia_;
    QLineEdit* txbMedia_;
    QCheckBox* chkPalette_;
    QLineEdit* txbPalette_;
    QCheckBox* chkPointers_;
    QLineEdit* txbPointers_;
    QCheckBox* chkZero_;
    QSpinBox* numCount_;
};
