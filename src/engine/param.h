#pragma once

#include <array>
#include <string>

namespace tuxscr {

// How a picture is to be converted: what ConvImgCpc's window is set to. It is
// its Param, field for field, and is what a settings file (.xml) holds.
struct Param {
    enum class SizeMode { Fit, KeepSmaller, KeepLarger, UserSize, Origin };

    SizeMode sizeMode = SizeMode::Fit;
    // Size and place of the source picture when the user sets them.
    int sizeX = 0, sizeY = 0, posX = 0, posY = 0;
    // The dithering: its matrix by name, and how much of it, in percent.
    std::string method = "Aucun";
    int pct = 0;
    std::array<int, 16> lockState{};
    int pctLumi = 100;
    int pctSat = 100;
    int pctContrast = 100;
    bool cpcPlus = false;
    bool newMethode = false;
    bool reductPal1 = false;
    bool reductPal2 = false;
    bool reductPal3 = false;
    bool reductPal4 = false;
    int newSortPal = 0;
    int nbCols = 80, nbLignes = 200;
    int modeVirtuel = 1;
    bool withCode = true;
    bool withPalette = true;
    bool setPalCpc = false;
    bool lissage = false;
    int trackModeX = 1;
    int pctRed = 100, pctGreen = 100, pctBlue = 100;
    bool trameTc = false;
    std::string lastReadPath;
    std::string lastSavePath;
    // The weights of red, green and blue when two colours are compared.
    int coefR = 9798;
    int coefV = 19235;
    int coefB = 3735;
    // Where a component goes from one of the CPC's three levels to the next.
    int cstR1 = 85, cstR2 = 170, cstR3 = 255, cstR4 = 340;
    int cstV1 = 85, cstV2 = 170, cstV3 = 255, cstV4 = 340;
    int cstB1 = 85, cstB2 = 170, cstB3 = 255, cstB4 = 340;
    bool newReduc = false;
    int bitsRVB = 24;
    bool diffErr = false;
    std::string langue = "FR";
    bool modeImpDraw = false;
    std::array<int, 16> disableState{};
    bool filtre = false;
    int kMeansDist = 0;
    int kMeansColor = 16;
    int kMeansPass = 16;
    bool autoRecalc = false;
};

}  // namespace tuxscr
