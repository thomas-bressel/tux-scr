#pragma once

#include <vector>

#include <QImage>
#include <QObject>
#include <QString>

#include "convert/convert.h"
#include "convert/param.h"
#include "convert/picture.h"
#include "convert/screen.h"
#include "formats/animfile.h"
#include "formats/pack.h"
#include "formats/screenfile.h"

namespace tuxscr {

// What TuxSCR's windows work on together: the CPC the picture is for, the
// settings, the pictures read from the PC and what they were converted to.
// A file with several pictures (an animated GIF, an animation of Impdraw's)
// gives as many frames, of which one is the current one.
//
// The windows change `screen` and `param` as they please and call touch()
// or convert(); they hear of what the others did through changed().
class Document : public QObject {
    Q_OBJECT

public:
    explicit Document(QObject* parent = nullptr);

    tuxscr::Screen screen;
    tuxscr::Param param;
    tuxscr::PackMethod packMethod = tuxscr::PackMethod::ZX0_V2;
    // A conversion asked for converts every frame, not the current one only.
    bool allPictures = false;
    // A packed screen of more than 16K is written as two sources.
    bool inTwoFiles = false;
    // The hardware sprites' places on the picture, for those who show them.
    struct SpritePlaces {
        int x[16] = {};
        int y[16] = {};
        int zoomX[16] = {};
        int zoomY[16] = {};
    } spritePlaces;
    bool showSprites = false;

    // ---- Frames ----
    int frameCount() const { return static_cast<int>(frames_.size()); }
    // Every frame's picture, in order.
    std::vector<tuxscr::Picture> pictures() const;
    int current() const { return current_; }
    void select(int frame, bool quiet = false);
    // How long a frame stays, in milliseconds; 0 for a picture.
    int frameTime() const { return frameTime_; }
    int frameTime(int frame) const { return frames_[static_cast<size_t>(frame)].time; }
    void setFrameTime(int frame, int time) { frames_[static_cast<size_t>(frame)].time = time; }
    const QImage& source() const { return frames_[static_cast<size_t>(current_)].source; }
    const QImage& source(int frame) const { return frames_[static_cast<size_t>(frame)].source; }
    tuxscr::Picture& picture() { return frames_[static_cast<size_t>(current_)].picture; }
    const tuxscr::Picture& picture() const { return frames_[static_cast<size_t>(current_)].picture; }
    tuxscr::Picture& picture(int frame) { return frames_[static_cast<size_t>(frame)].picture; }

    // New frames: read from a file, or blank ones to draw on or to fill one
    // by one.
    void setSources(const QList<QImage>& pictures, int time);
    void setBlankFrames(int count, int time = 100);
    void importSource(const QImage& picture, int frame);
    void removeFrame(int frame);

    // ---- Conversion ----
    // Converts the current frame (or all of them) if `force` or if the
    // settings ask for it at every change, and has the windows draw again.
    // `searchInterrupt` is the palette interrupt whose colours are to be
    // searched too, if any.
    void convert(bool force, bool quiet = false, int searchInterrupt = -1);
    // Tells the windows the picture, the palette or the screen changed.
    void touch() { emit changed(); }
    // The pictures cleared to pen 0: all of them, or the current one if it
    // was never converted.
    void resetPictures(bool all);
    // From a CPC's palette to a Plus's and back: the colours are brought to
    // the nearest the other has.
    void setPlus(bool plus);
    // The picture as the CPC shows it.
    QImage shown() const;
    QImage shown(int frame) const;
    // Which pens the picture uses.
    std::array<bool, 16> usedPens() const;

    // ---- Files ----
    // Reads a picture of the PC's or a screen of the CPC's. `single` puts it
    // in the current frame of an animation, leaving the others. False,
    // having said why, if it could not.
    bool readFile(const QString& path, bool single = false);
    bool readPalette(const QString& path);
    bool readKit(const QString& path);
    bool readParam(const QString& path);
    bool saveParam(const QString& path);

    // What a source's first line says wrote it.
    static tuxscr::AsmStamp stamp();

    bool saveScreen(const QString& path, bool toDsk);
    bool savePng(const QString& path);
    bool saveSprite(const QString& path, const tuxscr::AsmLabels& labels, bool packed);
    bool savePacked(const QString& path, const tuxscr::AsmLabels& labels, int format);  // 0 file, 1 source, 2 disc
    bool saveImp(const QString& path);
    bool saveTiles(const QString& path, int tileWidth, int tileHeight);
    bool savePalette(const QString& path);
    bool saveKit(const QString& path, bool picture);
    bool saveEgx(const QString& path);
    bool saveBump(const QString& path);
    bool saveMatrix(const QString& path);
    bool saveTunnel(const QString& path);
    bool saveDiffImages(const QString& path);
    // The frames as the source of a program that plays them.
    bool saveAnimation(const QString& path, const tuxscr::AnimOptions& options);
    // The frames as they were converted, as an animated GIF.
    bool saveGif(const QString& path);
    bool savePaletteInterrupts(const QString& path);
    // The picture and the routine that shows it, as a source to assemble
    // and run: what "Show in TuxAPE" makes.
    std::string showProgram();

    // A line for the information window.
    void say(const QString& text) { emit said(text); }
    // A problem: said, and shown in a box by the window.
    void fail(const QString& text);

signals:
    void changed();
    // The number of frames or the current one changed.
    void framesChanged();
    // A file gave the screen its size, its mode or its kind: the controls
    // are to show them.
    void screenChanged();
    void titleChanged(const QString& fileName);
    void said(const QString& text);
    void failed(const QString& text);
    // Packing is at so many percent.
    void packing(int percent);

private:
    struct Frame {
        QImage source;
        tuxscr::Picture picture;
        int time = 100;
    };
    void convertMissing();
    std::vector<Frame> frames_;
    int current_ = 0;
    int frameTime_ = 0;

    tuxscr::PackProgress progress();
    bool write(const QString& path, const std::vector<uint8_t>& bytes);
    bool write(const QString& path, const std::string& text);
    void syncParam();
};

}  // namespace tuxscr
