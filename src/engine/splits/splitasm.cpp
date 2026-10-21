#include "splits/splitasm.h"

#include "formats/asmgen.h"

namespace tuxscr {

using namespace asmgen;

std::string splitProgram(SplitScreen& bmpCpc, PackMethod pkMethode, const PackProgress& progress)
{
    bmpCpc.calcPalette();
    const Screen cpc;
    AsmText wr(cpc);
    const VgaLetters& CpcVGA = wr.Cpc.CpcVGA;

    int nbLigneVide = 0;
    int reste = 0, oldc1 = 0, oldc2 = 0, oldc3 = 0, oldc4 = 0, oldc5 = 0, oldPen = 0;
    int oldRetPrec = SplitScreen::kMinDelay + 15136;

    // The picture is packed without what an overscan screen leaves unused
    // between its lines in memory.
    std::vector<uint8_t> bmp(bmpCpc.memory.begin(), bmpCpc.memory.begin() + 0x8000);
    for (size_t i = 0x600; i < 0x800; i++)
        bmp[i] = bmp[i + 0x800] = bmp[i + 0x1000] = bmp[i + 0x1800] = bmp[i + 0x2000] = bmp[i + 0x2800] = bmp[i + 0x3000] = 0;

    for (size_t i = 0x44C0; i < 0x4600; i++)
        bmp[i] = bmp[i + 0x800] = bmp[i + 0x1000] = bmp[i + 0x1800] = bmp[i + 0x2000] = bmp[i + 0x2800] = bmp[i + 0x3000] = bmp[i + 0x3800] = 0;

    bmp.resize(0x7CC0);
    const std::vector<uint8_t> bufPack = pack(bmp, pkMethode, progress);
    const int lgPack = static_cast<int>(bufPack.size());

    wr.WriteLine(S("	ORG	#") + hex(0x8400 - lgPack, 4));
    wr.WriteLine(S("	Nolist"));
    wr.WriteLine(S("ImageCmp"));
    split::GenereDatas(wr, bufPack.data(), lgPack, 16);
    split::WriteDebFile(wr);
    for (int y = 0; y < SplitScreen::kProgramLines; y++) {
        const SplitLine& lSpl = bmpCpc.lines[static_cast<size_t>(y)];
        const std::array<Split, kSplitsPerLine>& ListeSplit = lSpl.splits;
        // Split 1
        if (ListeSplit[0].enabled) {
            bool hlMod = false;
            int retPrec = oldRetPrec;
            oldRetPrec = 0;
            if (nbLigneVide > 0 || reste > 0) {
                retPrec += (reste << 3) + (nbLigneVide * 512);
                nbLigneVide = 0;
            }
            int retard = lSpl.delay - SplitScreen::kMinDelay;
            hlMod |= split::GenereRetard(wr, retard + retPrec, false, true);
            int retSameCol = 0;
            wr.WriteLine(S("; ---- Ligne ") + dec(y) + S(" ----"));
            if (reste < 0 && lSpl.pen == oldPen)
                hlMod |= split::GenereRetard(wr, (6 + reste) << 3, false, true);
            else {
                wr.WriteLine(S("	LD	C,") + dec(lSpl.pen) + S("			; (2 NOPs)"));
                wr.WriteLine(S("	OUT	(C),C			; (4 NOPs)"));
                oldPen = lSpl.pen;
            }
            int c0 = CpcVGA[ListeSplit[0].colour];
            wr.WriteLine(S("	LD	C,#") + hex(c0, 2) + S("			; (2 NOPs)"));
            int c1 = CpcVGA[ListeSplit[1].colour];
            int c2 = CpcVGA[ListeSplit[2].colour];
            if ((c1 != oldc1 || c2 != oldc2) && (ListeSplit[1].enabled || ListeSplit[2].enabled))
                wr.WriteLine(S("	LD	DE,#") + hex(c1, 2) + hex(c2, 2) + S("		; (3 NOPs)"));
            else
                retSameCol += 24;

            int c3 = CpcVGA[ListeSplit[3].colour];
            int c4 = CpcVGA[ListeSplit[4].colour];
            if (hlMod || ((c3 != oldc3 || c4 != oldc4) && (ListeSplit[3].enabled || ListeSplit[4].enabled))) {
                wr.WriteLine(S("	LD	HL,#") + hex(c3, 2) + hex(c4, 2) + S("		; (3 NOPs)"));
            }
            else
                retSameCol += 24;

            int c5 = CpcVGA[ListeSplit[5].colour];
            if (c5 != oldc5 && ListeSplit[5].enabled)
                wr.WriteLine(S("	LD	A,#") + hex(c5, 2) + S("			; (2 NOPs)"));
            else
                retSameCol += 16;

            if (retSameCol > 0)
                split::GenereRetard(wr, retSameCol, false, !ListeSplit[3].enabled && !ListeSplit[4].enabled);

            wr.WriteLine(S("	OUT	(C),C			; (4 NOPs)"));
            int tpsLine = (retard >> 3) + 20;
            int lg = ListeSplit[0].length - 32;
            tpsLine += lg >> 3;
            // Split 2
            if (ListeSplit[1].enabled) {
                int lgs = ListeSplit[0].length - 32;
                split::GenereRetard(wr, lgs);
                wr.WriteLine(S("	OUT	(C),D			; (4 NOPs)"));
                tpsLine += 4 + (lgs >> 3);
                // Split 3
                if (ListeSplit[2].enabled) {
                    lgs = ListeSplit[1].length - 32;
                    split::GenereRetard(wr, lgs, false, !ListeSplit[3].enabled && !ListeSplit[4].enabled);
                    wr.WriteLine(S("	OUT	(C),E			; (4 NOPs)"));
                    tpsLine += 4 + (lgs >> 3);
                    // Split 4
                    if (ListeSplit[3].enabled) {
                        lgs = ListeSplit[2].length - 32;
                        split::GenereRetard(wr, lgs);
                        wr.WriteLine(S("	OUT	(C),H			; (4 NOPs)"));
                        tpsLine += 4 + (lgs >> 3);
                        // Split 5
                        if (ListeSplit[4].enabled) {
                            lgs = ListeSplit[3].length - 32;
                            split::GenereRetard(wr, lgs);
                            wr.WriteLine(S("	OUT	(C),L			; (4 NOPs)"));
                            tpsLine += 4 + (lgs >> 3);
                            // Split 6
                            if (ListeSplit[5].enabled) {
                                lgs = ListeSplit[4].length - 32;
                                split::GenereRetard(wr, lgs);
                                wr.WriteLine(S("	OUT	(C),A			; (4 NOPs)"));
                                tpsLine += 4 + (lgs >> 3);
                            }
                            if (ListeSplit[6].enabled) {
                                int c6 = CpcVGA[ListeSplit[6].colour];
                                int sameColor = 6;
                                const char* const tabRegs = "CDEHLAA";
                                for (int i = 0; i < 6; i++)
                                    if (c6 == CpcVGA[ListeSplit[static_cast<size_t>(i)].colour])
                                        sameColor = i;

                                // The register that already has the colour.
                                char reg = tabRegs[sameColor];
                                if (sameColor == 6) {
                                    wr.WriteLine(S("	LD	A,#") + hex(c6, 2) + S("			; (2 NOPs)"));
                                    lgs = ListeSplit[5].length - 48;
                                    tpsLine += 2;
                                    c5 = c6;
                                }
                                else
                                    lgs = ListeSplit[5].length - 32;

                                split::GenereRetard(wr, lgs);
                                wr.WriteLine(S("	OUT	(C),") + chr(reg) + S("			; (4 NOPs)"));
                                tpsLine += 4 + (lgs >> 3);
                            }
                        }
                    }
                }
            }
            oldc1 = c1;
            oldc2 = c2;
            oldc3 = c3;
            oldc4 = c4;
            oldc5 = c5;
            reste = 64 - tpsLine + (lg >> 3);
        }
        else {
            nbLigneVide++;
            wr.WriteLine(S("; ---- Ligne ") + dec(y) + S(" (vide) ----"));
        }
    }
    // The palette written is the one of the first byte of the first line,
    // the splits done: a pen that line changes at once has the colour it
    // is given there.
    split::WriteEndFile(wr, bmpCpc.palette, pkMethode);
    return wr.text();
}

}  // namespace tuxscr
