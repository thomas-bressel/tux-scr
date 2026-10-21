#pragma once

#include <functional>

#include <QDialog>
#include <QList>
#include <QPointer>

#include "document.h"
#include "plugin.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QTabWidget;
class QVBoxLayout;

namespace tuxscr {

class AnimationPanel;
class EditTools;
class InternalSettings;
class InterruptsPanel;
class PaletteBar;
class PictureView;

// TuxSCR's window: ConvImgCpc's two, side by side. On the left what its
// main window has: files, the CPC's resolution and mode, the size of the
// source, dithering, colours. On the right its "Image CPC" window: the
// palette, the picture, and under it the tools, the first of which is
// TuxSCR's own: the palette interrupts of a Plus.
class ConverterWindow : public QDialog {
    Q_OBJECT

public:
    // `host` is TuxAPE, or null when the window is shown by itself.
    explicit ConverterWindow(tuxape::PluginHost* host, QWidget* parent = nullptr);
    ~ConverterWindow() override;

    Document* document() { return &document_; }
    PictureView* pictureView() const { return view_; }
    PaletteBar* paletteBar() const { return palette_; }
    InterruptsPanel* interruptsPanel() const { return interrupts_; }
    EditTools* editTools() const { return editTools_; }
    AnimationPanel* animationPanel() const { return animation_; }
    QTabWidget* tools() const { return tools_; }

    // Reads a file as a drop on the window does: by its extension, a
    // palette, settings, or a picture.
    void openFile(const QString& path);
    // Asks for a file to save, and saves the picture in the format chosen.
    void save();

    // What a pen of the palette does when clicked, for those who take it
    // over (the picture's editor); null gives it back.
    void setPenHandler(std::function<void(int pen, bool right)> handler);
    // What the picture's view shows instead of the picture as it is, for
    // those who show it enlarged; null gives the picture back.
    void setImageProvider(std::function<QImage()> provider);
    // The box that shows the picture twice its size.
    bool doubled() const;

    // Those who take the picture's mouse over (the editor, the capture of
    // sprites) say so: while one has it, the source is not moved or sized
    // with the mouse. Null gives it back.
    void setMouseOwner(QObject* owner) { mouseOwner_ = owner; }
    QObject* mouseOwner() const { return mouseOwner_; }
    // The settings' side.
    QWidget* settingsPanel() const { return settingsPanel_; }
    // A tool has the settings out of reach while it is at work, and gives
    // them back when it is done: they are within reach again once every
    // tool that held them has let go.
    void holdSettings(QObject* tool, bool hold);

    // Asks the labels a source gives its data and its palette: false if
    // the user gave up.
    bool askLabels(const QString& kind, const QString& name, tuxscr::AsmLabels& labels);
    // One more kind of file "Save" offers, with what writes it.
    void addSaveFormat(const QString& filter, std::function<bool(const QString&)> save) { extraFormats_.append({filter, std::move(save)}); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    tuxape::PluginHost* host_;
    Document document_;
    std::function<void(int, bool)> penHandler_;
    std::function<QImage()> imageProvider_;
    struct SaveFormat {
        QString filter;
        std::function<bool(const QString&)> save;
    };
    QList<SaveFormat> extraFormats_;
    bool loading_ = false;  // the controls are being set from the document
    QPointer<QObject> mouseOwner_;
    QList<QPointer<QObject>> settingsHolders_;
    QWidget* settingsPanel_ = nullptr;
    QScrollArea* settingsScroll_ = nullptr;

    // Files.
    QPushButton* bpLoad_;
    QPushButton* bpCreate_;
    QPushButton* bpImport_;
    QPushButton* bpConvert_;
    QCheckBox* autoRecalc_;
    QPushButton* bpSave_;
    QCheckBox* withCode_;
    QCheckBox* withPalette_;
    QComboBox* comboPack_;
    QCheckBox* chk17K_;
    QCheckBox* chkAllPics_;
    QPushButton* bpShow_;
    QPushButton* bpFromCpc_;
    // Resolution.
    QSpinBox* nbCols_;
    QSpinBox* nbLignes_;
    QComboBox* mode_;
    QSlider* trackModeX_;
    QLabel* lblTrack_;
    QCheckBox* chkSwapEgx_;
    QPushButton* bpEditTrame_;
    QPushButton* bpEditSplit_;
    QPushButton* bpRasterPlus_;
    QPushButton* bpEditSprites_;
    // Source size.
    QRadioButton* radioFit_;
    QRadioButton* radioKeepSmaller_;
    QRadioButton* radioKeepLarger_;
    QRadioButton* radioUserSize_;
    QRadioButton* radioOrigin_;
    QSpinBox* numPosX_;
    QSpinBox* numPosY_;
    QSpinBox* numSizeX_;
    QSpinBox* numSizeY_;
    QWidget* sizeRow_;
    QWidget* scaleRow_;
    // Dithering.
    QComboBox* methode_;
    QSpinBox* pctTrame_;
    QLabel* lblPct_;
    QCheckBox* chkDiffErr_;
    QCheckBox* chkLissage_;
    QCheckBox* chkTrameTC_;
    QCheckBox* chkImpDraw_;
    // Colours.
    QCheckBox* sortPal_;
    QCheckBox* nb_;
    QCheckBox* newMethode_;
    QCheckBox* chkNewReduc_;
    QCheckBox* chkPalCpc_;
    QCheckBox* modePlus_;
    QCheckBox* reducPal_[4];
    QRadioButton* rbBits_[4];
    QCheckBox* chkGauss_;
    QSlider* sliders_[6];  // lumi, sat, contrast, red, green, blue
    // The foot.
    QCheckBox* chkInfo_;
    QCheckBox* chkParamInterne_;
    QLabel* lblInfo_;
    QLabel* lblVersion_;
    QPointer<QDialog> infoWindow_;
    QPlainTextEdit* infoText_ = nullptr;
    QStringList infoLines_;
    // The picture's side.
    PaletteBar* palette_;
    QCheckBox* lockAllPal_;
    QPushButton* bpCopyPal_;
    QPushButton* bpGenPal_;
    QCheckBox* chkX2_;
    PictureView* view_;
    QScrollArea* viewScroll_;
    QTabWidget* tools_;
    InterruptsPanel* interrupts_;
    EditTools* editTools_ = nullptr;
    AnimationPanel* animation_ = nullptr;
    QPointer<InternalSettings> internalSettings_;
    // Moving and sizing the source with the mouse.
    bool movePos_ = false, moveSize_ = false;
    int memoX_ = 0, memoY_ = 0, posX_ = 0, posY_ = 0, sizeX_ = 0, sizeY_ = 0;

    QWidget* buildSettings();
    QWidget* buildPicture();
    void applyTexts();
    void fromDocument();
    void interfaceChange();
    void updateVisibility();
    void fillModes();
    void pictureChanged();
    void penClicked(int pen, bool right);
    void sizeModeChanged();
    void moveOrSize(QPoint at, Qt::MouseButtons buttons);
    void load(bool single);
    void copyPalette();
    void showInTuxApe();
    void fromCpcScreen();
    void info(const QString& text);
    void resetColours(bool all);
    QString settingsFile() const;
};

}  // namespace tuxscr
