// Made by tools/make_asm.py from ConvImgCpc's sources: not to be changed by hand.

// SaveAsm.cs
void WriteDatas(AsmText& sw, const int* tabCol, int start, int end, int nbMotsLigne, int ligneSepa = 0, const Text& labelSepa = Text());
Text GenereDatas(const int* tabCol, int start, int end, int nbMotsLigne, int ligneSepa = 0, const Text& labelSepa = Text(), bool exclTaille = false);
void GenereDatas(AsmText& sw, const uint8_t* tabByte, int length, int nbOctetsLigne, int ligneSepa = 0, const Text& labelSepa = Text(), bool exclTaille = false);
void GenereDepack(AsmText& sw, PackMethod pkMethode, const Text& jumpLabel = Text());
void GenereDZX0(AsmText& sw, bool v2, const Text& jumpLabel);
void GenereDZX1(AsmText& sw, const Text& jumpLabel);
void GenereDStd(AsmText& sw, const Text& jumpLabel = Text());
void GenereFormatEcran(AsmText& sw, bool noChangeAdr = false);
void GenereInitOld(AsmText& sw, const Text& labelPalette);
void GenereInitPlus(AsmText& sw, const Text& labelPalette);
void GenerePalette(AsmText& sw, const Param& p, bool withUnlockAsic, bool withMode, const Text& label);
void GenereEntete(AsmText& sw, int adr);
void GenereAffichage(AsmText& sw, bool delai, bool reboucle, bool gest128K, bool imageMode, PackMethod pkMethode);
void GenereDrawDC(AsmText& sw, bool delai, bool modeCol, bool gest128K, int addBC26, bool optimSpeed);
void GenereDrawDirect(AsmText& sw, bool gest128K);
void GenereDrawBlock(AsmText& sw, int height, bool delai, bool gest128K);
void GenereDrawAscii(AsmText& sw, bool frameFull, bool frameO, bool frameD, bool gest128K, bool imageMode, bool withSpeed);
void GenereFin(AsmText& sw, int ltot, bool force8000, bool withCode);
void GenerePointeurs(AsmText& sw, int nbImages, const int* bank, const int* speed, bool gest128K);
void GenereTspace(AsmText& sw, bool wait, bool withoutRet = false);
void GenereAfficheStd(AsmText& sw, bool overscan, PackMethod pkMethode, const Text& labelMedia, const Text& labelPalette);
void GenereAfficheModeX(AsmText& sw, const LineTable& colMode5, bool isOverscan, PackMethod pkMethode, const Text& labelMedia);
void GenereAfficheModeEgx(AsmText& sw, const int* palette, bool overscan, PackMethod pkMethode, const Text& labelMedia, const Text& labelPalette);

// GenSplitAsm.cs
namespace split {
void WriteDebFile(AsmText& wr);
bool GenereRetard(AsmText& wr, int nbPixels, bool crashBC = false, bool canUseHL = false);
void GenereDatas(AsmText& sw, const uint8_t* tabByte, int length, int nbOctetsLigne, int ligneSepa = 0, const Text& labelSepa = Text());
void WriteEndFile(AsmText& wr, const SplitPalette& palette, PackMethod pkMethode);
}  // namespace split
