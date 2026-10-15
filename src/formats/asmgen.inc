// Made by tools/make_asm.py from ConvImgCpc's sources: not to be changed by hand.

// ---- SaveAsm.cs ----

void WriteDatas(AsmText& sw, const int* tabCol, int start, int end, int nbMotsLigne, int ligneSepa, const Text& labelSepa)
{
			sw.Write(GenereDatas(tabCol, start, end, nbMotsLigne, ligneSepa, labelSepa));
}

Text GenereDatas(const int* tabCol, int start, int end, int nbMotsLigne, int ligneSepa, const Text& labelSepa, bool exclTaille)
{
			Text ret = S("");
			Text line = S("\tDW\t");
			int nbOctets = 0, nbLigne = 0, indiceLabel = 0;
			if (!labelSepa.isNull()) {
				ret += labelSepa + dec(indiceLabel, 2) + NL;
				indiceLabel++;
			}
			for (int i = start; i <= end; i++) {
				line += S("#") + hex(tabCol[i], 3) + S(",");
				if (++nbOctets >= std::min(nbMotsLigne, 16)) {
					ret += line.Substring(0, line.Length() - 1) + NL;
					line = S("\tDW\t");
					nbOctets = 0;
					if (i < end - 1 && ++nbLigne >= ligneSepa && ligneSepa > 0) {
						nbLigne = 0;
						if (!labelSepa.isNull()) {
							ret += labelSepa + dec(indiceLabel, 2) + NL;
							indiceLabel++;
						}
						else
							line += NL;
					}
				}
			}
			if (nbOctets > 0)
				ret += line.Substring(0, line.Length() - 1) + NL;

			if (!exclTaille)
				ret += S("; Taille totale ") + dec((end - start + 1)) + S(" mots\r\n");

			return ret;
}

void GenereDatas(AsmText& sw, const uint8_t* tabByte, int length, int nbOctetsLigne, int ligneSepa, const Text& labelSepa, bool exclTaille)
{
			Text line = S("\tDB\t");
			int nbOctets = 0, nbLigne = 0, indiceLabel = 0;
			if (!labelSepa.empty()) {
				sw.WriteLine(labelSepa + (ligneSepa != 0 ? dec(indiceLabel, 2) : S("")));
				indiceLabel++;
			}

			uint8_t firstVal = tabByte[0];
			bool sameVal = true;
			for (int i = 1; i < length; i++)
				if (tabByte[i] != firstVal) {
					sameVal = false;
					break;
				}
			if (sameVal)
				sw.WriteLine(S("\tDS\t") + dec(length) + S(",#") + hex(firstVal, 2));
			else {
				for (int i = 0; i < length; i++) {
					line += S("#") + hex(tabByte[i], 2) + S(",");
					if (++nbOctets >= std::min(nbOctetsLigne, sw.Cpc.NbCol)) {
						sw.WriteLine(line.Substring(0, line.Length() - 1));
						line = S("\tDB\t");
						nbOctets = 0;
						if (i < length - 1 && ++nbLigne >= ligneSepa && ligneSepa > 0) {
							nbLigne = 0;
							if (!labelSepa.isNull()) {
								sw.WriteLine(labelSepa + dec(indiceLabel, 2));
								indiceLabel++;
							}
							else
								line += NL;
						}
					}
				}
			}
			if (nbOctets > 0)
				sw.WriteLine(line.Substring(0, line.Length() - 1));

			if (!exclTaille)
				sw.WriteLine(S("; Taille totale ") + dec(length) + S(" octets"));
}

void GenereDepack(AsmText& sw, PackMethod pkMethode, const Text& jumpLabel)
{
			switch (pkMethode) {
				case PackMethod::Standard:
					GenereDStd(sw, jumpLabel);
					break;

				case PackMethod::ZX0:
				case PackMethod::ZX0Ovs:
				case PackMethod::ZX0_V2:
					GenereDZX0(sw, pkMethode != PackMethod::ZX0, jumpLabel);
					break;

				case PackMethod::ZX1:
					GenereDZX1(sw, jumpLabel);
					break;
			}
}

void GenereDZX0(AsmText& sw, bool v2, const Text& jumpLabel)
{
			sw.WriteLine(S("; Decompactage"));
			sw.WriteLine(S("Depack"));
			sw.WriteLine(S("	ld	bc,#ffff			; preserve default offset 1"));
			sw.WriteLine(S("	push	bc"));
			sw.WriteLine(S("	inc	bc"));
			sw.WriteLine(S("	ld	a,#80"));
			sw.WriteLine(S("dzx0s_literals"));
			sw.WriteLine(S("	call	dzx0s_elias		; obtain length"));
			sw.WriteLine(S("	ldir					; copy literals"));
			sw.WriteLine(S("	add	a,a					; copy from last offset or new offset?"));
			sw.WriteLine(S("	jr	c,dzx0s_new_offset"));
			sw.WriteLine(S("	call	dzx0s_elias		; obtain length"));
			sw.WriteLine(S("dzx0s_copy"));
			sw.WriteLine(S("	ex	(sp),hl				; preserve source,restore offset"));
			sw.WriteLine(S("	push	hl				; preserve offset"));
			sw.WriteLine(S("	add	hl,de				; calculate destination - offset"));
			sw.WriteLine(S("	ldir					; copy from offset"));
			sw.WriteLine(S("	pop	hl					; restore offset"));
			sw.WriteLine(S("	ex	(sp),hl				; preserve offset,restore source"));
			sw.WriteLine(S("	add	a,a					; copy from literals or new offset?"));
			sw.WriteLine(S("	jr	nc,dzx0s_literals"));
			sw.WriteLine(S("dzx0s_new_offset"));
			if (v2) {
				sw.WriteLine(S("	pop	bc					;discard last offset"));
				sw.WriteLine(S("	ld	c,#fe				; prepare negative offset"));
				sw.WriteLine(S("	call dzx0s_elias_loop	;obtain offset MSB"));
				sw.WriteLine(S("	inc	c"));
			}
			else {
				sw.WriteLine(S("	call	dzx0s_elias		; obtain offset MSB"));
				sw.WriteLine(S("	ld b,a"));
				sw.WriteLine(S("	pop	af					; discard last offset"));
				sw.WriteLine(S("	xor	a					; adjust for negative offset"));
				sw.WriteLine(S("	sub	c"));
			}
			sw.WriteLine((!jumpLabel.isNull() ? (S("	JP	Z,") + jumpLabel) : S("	RET	Z")) + S("		; Plus d'octets à traiter = fini") + NL);
			if (!v2) {
				sw.WriteLine(S("	ld	c,a"));
				sw.WriteLine(S("	ld	a,b"));
			}
			sw.WriteLine(S("	ld	b,c"));
			sw.WriteLine(S("	ld	c,(hl)				; obtain offset LSB"));
			sw.WriteLine(S("	inc	hl"));
			sw.WriteLine(S("	rr	b					; last offset bit becomes first length bit"));
			sw.WriteLine(S("	rr	c"));
			sw.WriteLine(S("	push	bc				; preserve new offset"));
			sw.WriteLine(S("	ld	bc,1				; obtain length"));
			sw.WriteLine(S("	call	nc,dzx0s_elias_backtrack"));
			sw.WriteLine(S("	inc	bc"));
			sw.WriteLine(S("	jr	dzx0s_copy"));
			sw.WriteLine(S("dzx0s_elias"));
			sw.WriteLine(S("	inc	c					; interlaced Elias gamma coding"));
			sw.WriteLine(S("dzx0s_elias_loop"));
			sw.WriteLine(S("	add	a,a"));
			sw.WriteLine(S("	jr	nz,dzx0s_elias_skip"));
			sw.WriteLine(S("	ld	a,(hl)				; load another group of 8 bits"));
			sw.WriteLine(S("	inc	hl"));
			sw.WriteLine(S("	rla"));
			sw.WriteLine(S("dzx0s_elias_skip"));
			sw.WriteLine(S("	ret 	c"));
			sw.WriteLine(S("dzx0s_elias_backtrack"));
			sw.WriteLine(S("	add	a,a"));
			sw.WriteLine(S("	rl	c"));
			sw.WriteLine(S("	rl	b"));
			sw.WriteLine(S("	jr	dzx0s_elias_loop"));
}

void GenereDZX1(AsmText& sw, const Text& jumpLabel)
{
			sw.WriteLine(S("; Decompactage"));
			sw.WriteLine(S("Depack:"));
			sw.WriteLine(S("	ld	bc,#ffff			; preserve default offset 1"));
			sw.WriteLine(S("	push	bc"));
			sw.WriteLine(S("	ld	a,#80"));
			sw.WriteLine(S("dzx1s_literals:"));
			sw.WriteLine(S("	call	dzx1s_elias		; obtain length"));
			sw.WriteLine(S("	ldir					; copy literals"));
			sw.WriteLine(S("	add	a,a					; copy from last offset or new offset?"));
			sw.WriteLine(S("	jr	c,dzx1s_new_offset"));
			sw.WriteLine(S("	call	dzx1s_elias		; obtain length"));
			sw.WriteLine(S("dzx1s_copy:"));
			sw.WriteLine(S("	ex	(sp),hl				; preserve source,restore offset"));
			sw.WriteLine(S("	push	hl				; preserve offset"));
			sw.WriteLine(S("	add	hl,de				; calculate destination - offset"));
			sw.WriteLine(S("	ldir					; copy from offset"));
			sw.WriteLine(S("	pop	hl					; restore offset"));
			sw.WriteLine(S("	ex	(sp),hl				; preserve offset,restore source"));
			sw.WriteLine(S("	add	a,a					; copy from literals or new offset?"));
			sw.WriteLine(S("	jr	nc,dzx1s_literals"));
			sw.WriteLine(S("dzx1s_new_offset:"));
			sw.WriteLine(S("	inc	sp					; discard last offset"));
			sw.WriteLine(S("	inc	sp"));
			sw.WriteLine(S("	dec	b"));
			sw.WriteLine(S("	ld	c,(hl)				; obtain offset LSB"));
			sw.WriteLine(S("	inc	hl"));
			sw.WriteLine(S("	rr	c					; single byte offset?"));
			sw.WriteLine(S("	jr	nc,dzx1s_msb_skip"));
			sw.WriteLine(S("	ld	b,(hl)				; obtain offset MSB"));
			sw.WriteLine(S("	inc	hl"));
			sw.WriteLine(S("	rr	b					; replace last LSB bit with last MSB bit"));
			sw.WriteLine(S("	inc	b"));
			sw.WriteLine((!jumpLabel.isNull() ? (S("	JP	Z,") + jumpLabel) : S("	RET	Z")) + S("		; Plus d'octets à traiter = fini") + NL);
			sw.WriteLine(S("	rl	c"));
			sw.WriteLine(S("dzx1s_msb_skip:"));
			sw.WriteLine(S("	push	bc				; preserve new offset"));
			sw.WriteLine(S("	call	dzx1s_elias		; obtain length"));
			sw.WriteLine(S("	inc	bc"));
			sw.WriteLine(S("	jr	dzx1s_copy"));
			sw.WriteLine(S("dzx1s_elias:"));
			sw.WriteLine(S("	ld	bc,1				; interlaced Elias gamma coding"));
			sw.WriteLine(S("dzx1s_elias_loop:"));
			sw.WriteLine(S("	add	a,a"));
			sw.WriteLine(S("	jr	nz,dzx1s_elias_skip"));
			sw.WriteLine(S("	ld	a,(hl)				; load another group of 8 bits"));
			sw.WriteLine(S("	inc	hl"));
			sw.WriteLine(S("	rla"));
			sw.WriteLine(S("dzx1s_elias_skip:"));
			sw.WriteLine(S("	ret	nc"));
			sw.WriteLine(S("	add	a,a"));
			sw.WriteLine(S("	rl	c"));
			sw.WriteLine(S("	rl	b"));
			sw.WriteLine(S("	jr	dzx1s_elias_loop"));
}

void GenereDStd(AsmText& sw, const Text& jumpLabel)
{
			sw.WriteLine(S("; Decompactage"));
			sw.WriteLine(S("Depack:"));
			sw.WriteLine(S("	LD	A,(HL)			; DepackBits = InBuf[ InBytes++ ]"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	RRA				; Rotation rapide calcul seulement flag C"));
			sw.WriteLine(S("	SET	7,A			; Positionne bit 7 en gardant flag C"));
			sw.WriteLine(S("	LD	(BclLzw+1),A"));
			sw.WriteLine(S("	JR	C,TstCodeLzw"));
			sw.WriteLine(S("CopByteLzw:"));
			sw.WriteLine(S("	LDI				; OutBuf[ OutBytes++ ] = InBuf[ InBytes++ ]") + NL);
			sw.WriteLine(S("BclLzw:"));
			sw.WriteLine(S("	LD	A,0"));
			sw.WriteLine(S("	RR	A			; Rotation avec calcul Flags C et Z"));
			sw.WriteLine(S("	LD	(BclLzw+1),A"));
			sw.WriteLine(S("	JR	NC,CopByteLzw"));
			sw.WriteLine(S("	JR	Z,Depack") + NL);
			sw.WriteLine(S("TstCodeLzw:"));
			sw.WriteLine(S("	LD	A,(HL)			; A = InBuf[ InBytes ];"));
			sw.WriteLine(S("	AND	A"));
			sw.WriteLine((!jumpLabel.isNull() ? (S("	JP	Z,") + jumpLabel) : S("	RET	Z")) + S("		; Plus d'octets à traiter = fini") + NL);
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	B,A			; B = InBuf[ InBytes ]"));
			sw.WriteLine(S("	RLCA				; A & #80 ?"));
			sw.WriteLine(S("	JR	NC,TstLzw40") + NL);
			sw.WriteLine(S("	RLCA"));
			sw.WriteLine(S("	RLCA"));
			sw.WriteLine(S("	RLCA"));
			sw.WriteLine(S("	AND	7"));
			sw.WriteLine(S("	ADD	A,3			; Longueur = 3 + ( ( InBuf[ InBytes ] >> 4 ) & 7 );"));
			sw.WriteLine(S("	LD	C,A			; C = Longueur"));
			sw.WriteLine(S("	LD	A,B			; B = InBuf[InBytes]"));
			sw.WriteLine(S("	AND	#0F			; Delta = ( InBuf[ InBytes++ ] & 15 ) << 8"));
			sw.WriteLine(S("	LD	B,A			; B = poids fort Delta"));
			sw.WriteLine(S("	LD	A,C			; A = Length"));
			sw.WriteLine(S("	SCF				; Repositionner flag C (pour Delta++)"));
			sw.WriteLine(S("CopyBytes0:"));
			sw.WriteLine(S("	LD	C,(HL)			; C = poids faible Delta (Delta |= InBuf[ InBytes++ ]);"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	PUSH	HL"));
			sw.WriteLine(S("	LD	H,D"));
			sw.WriteLine(S("	LD	L,E"));
			sw.WriteLine(S("	SBC	HL,BC			; HL=HL-(BC+1)"));
			sw.WriteLine(S("	LD	B,0"));
			sw.WriteLine(S("CopyBytes1:"));
			sw.WriteLine(S("	LD	C,A"));
			sw.WriteLine(S("CopyBytes2:"));
			sw.WriteLine(S("	LDIR"));
			sw.WriteLine(S("CopyBytes3:"));
			sw.WriteLine(S("	POP	HL"));
			sw.WriteLine(S("	JR	BclLzw") + NL);
			sw.WriteLine(S("TstLzw40:"));
			sw.WriteLine(S("	RLCA				; A & #40 ?"));
			sw.WriteLine(S("	JR	NC,TstLzw20") + NL);
			sw.WriteLine(S("	LD	C,B"));
			sw.WriteLine(S("	RES	6,C			; Delta = 1 + InBuf[ InBytes++ ] & #3f;"));
			sw.WriteLine(S("	LD	B,0			; BC = Delta + 1 car flag C = 1"));
			sw.WriteLine(S("	PUSH	HL"));
			sw.WriteLine(S("	LD	H,D"));
			sw.WriteLine(S("	LD	L,E"));
			sw.WriteLine(S("	SBC	HL,BC"));
			sw.WriteLine(S("	LDI"));
			sw.WriteLine(S("	LDI				; Longueur = 2"));
			sw.WriteLine(S("	JR	CopyBytes3") + NL);
			sw.WriteLine(S("TstLzw20:"));
			sw.WriteLine(S("	RLCA				; A & #20 ?"));
			sw.WriteLine(S("	JR	NC,TstLzw10") + NL);
			sw.WriteLine(S("	LD	A,B			; B compris entre #20 et #3F"));
			sw.WriteLine(S("	ADD	A,#E2			; = ( A AND #1F ) + 2,et positionne carry"));
			sw.WriteLine(S("	LD	B,0			; Longueur = 2 + ( InBuf[ InBytes++ ] & 31 );"));
			sw.WriteLine(S("	JR	CopyBytes0") + NL);
			sw.WriteLine(S("CodeLzw0F:"));
			sw.WriteLine(S("	LD	C,(HL)"));
			sw.WriteLine(S("	PUSH	HL"));
			sw.WriteLine(S("	LD	H,D"));
			sw.WriteLine(S("	LD	L,E"));
			sw.WriteLine(S("	CP	#F0"));
			sw.WriteLine(S("	JR	NZ,CodeLzw02") + NL);
			sw.WriteLine(S("	XOR	A"));
			sw.WriteLine(S("	LD	B,A"));
			sw.WriteLine(S("	INC	BC			; Longueur = Delta = InBuf[ InBytes + 1 ] + 1;"));
			sw.WriteLine(S("	SBC	HL,BC"));
			sw.WriteLine(S("	LDIR"));
			sw.WriteLine(S("	POP	HL"));
			sw.WriteLine(S("	INC	HL			; Inbytes += 2"));
			sw.WriteLine(S("	JR	BclLzw") + NL);
			sw.WriteLine(S("CodeLzw02:"));
			sw.WriteLine(S("	CP	#20"));
			sw.WriteLine(S("	JR	C,CodeLzw01") + NL);
			sw.WriteLine(S("	LD	C,B			; Longueur = Delta = InBuf[ InBytes ];"));
			sw.WriteLine(S("	LD	B,0"));
			sw.WriteLine(S("	SBC	HL,BC"));
			sw.WriteLine(S("	JR	CopyBytes2") + NL);
			sw.WriteLine(S("CodeLzw01:				; Ici,B = 1"));
			sw.WriteLine(S("	XOR	A			; Carry a zéro"));
			sw.WriteLine(S("	DEC	H			; Longueur = Delta = 256"));
			sw.WriteLine(S("	JR	CopyBytes1") + NL);
			sw.WriteLine(S("TstLzw10:"));
			sw.WriteLine(S("	RLCA				; A & #10 ?"));
			sw.WriteLine(S("	JR	NC,CodeLzw0F") + NL);
			sw.WriteLine(S("	RES	4,B			; B = Delta(high) -> ( InBuf[ InBytes++ ] & 15 ) << 8;"));
			sw.WriteLine(S("	LD	C,(HL)			; C = Delta(low)  -> InBuf[ InBytes++ ];"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	A,(HL)			; A = Longueur - 1"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	PUSH	HL"));
			sw.WriteLine(S("	LD	H,D"));
			sw.WriteLine(S("	LD	L,E"));
			sw.WriteLine(S("	SBC	HL,BC			; Flag C=1 -> hl=hl-(bc+1) (Delta+1)"));
			sw.WriteLine(S("	LD	B,0"));
			sw.WriteLine(S("	LD	C,A"));
			sw.WriteLine(S("	INC	BC			; BC =  Longueur = InBuf[ InBytes++ ] + 1;"));
			sw.WriteLine(S("	JR	CopyBytes2") + NL);
}

void GenereFormatEcran(AsmText& sw, bool noChangeAdr)
{
			if (sw.Cpc.NbCol != 80) {
				sw.WriteLine(S("	LD	HL,#") + hex(((sw.Cpc.NbCol + 1) >> 1), 2) + hex((26 + (sw.Cpc.NbCol >> 2)), 2));
				sw.WriteLine(S("	LD	BC,#BC01"));
				sw.WriteLine(S("	OUT	(C),C"));
				sw.WriteLine(S("	INC	B"));
				sw.WriteLine(S("	OUT	(C),H"));
				sw.WriteLine(S("	DEC	B"));
				sw.WriteLine(S("	INC	C"));
				sw.WriteLine(S("	OUT	(C),C"));
				sw.WriteLine(S("	INC	B"));
				sw.WriteLine(S("	OUT	(C),L"));
			}
			if (sw.Cpc.NbLig != 200) {
				sw.WriteLine(S("	LD	HL,#") + hex(((sw.Cpc.NbLig + 7) >> 3), 2) + hex((18 + (sw.Cpc.NbLig >> 4)), 2));
				sw.WriteLine(S("	LD	BC,#BC06"));
				sw.WriteLine(S("	OUT	(C),C"));
				sw.WriteLine(S("	INC	B"));
				sw.WriteLine(S("	OUT	(C),H"));
				sw.WriteLine(S("	DEC	B"));
				sw.WriteLine(S("	INC	C"));
				sw.WriteLine(S("	OUT	(C),C"));
				sw.WriteLine(S("	INC	B"));
				sw.WriteLine(S("	OUT	(C),L"));
			}
			if (sw.Cpc.NbLig * sw.Cpc.NbCol > 0x4000 && !noChangeAdr) {
				sw.WriteLine(S("	LD	BC,#BC0C"));
				sw.WriteLine(S("	OUT	(C),C"));
				sw.WriteLine(S("	INC	B"));
				sw.WriteLine(S("	INC	C"));
				sw.WriteLine(S("	OUT	(C),C"));
			}
}

void GenereInitOld(AsmText& sw, const Text& labelPalette)
{
			sw.WriteLine(S("	LD	HL,") + labelPalette);
			sw.WriteLine(S("	LD	B,#7F"));
			sw.WriteLine(S("	XOR	A"));
			sw.WriteLine(S("SetPal:"));
			sw.WriteLine(S("	OUT	(C),A"));
			sw.WriteLine(S("	INC	B"));
			sw.WriteLine(S("	OUTI"));
			sw.WriteLine(S("	INC	A"));
			sw.WriteLine(S("	CP	18"));
			sw.WriteLine(S("	JR	C,SetPal"));
}

void GenereInitPlus(AsmText& sw, const Text& labelPalette)
{
			sw.WriteLine(S("	LD	BC,#BC11"));
			sw.WriteLine(S("	LD	HL,UnlockAsic"));
			sw.WriteLine(S("Unlock:"));
			sw.WriteLine(S("	LD	A,(HL)"));
			sw.WriteLine(S("	OUT	(C),A"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	DEC	C"));
			sw.WriteLine(S("	JR	NZ,Unlock"));
			sw.WriteLine(S("	LD	BC,#7FB8"));
			sw.WriteLine(S("	LD	A,(") + labelPalette + S(")"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	OUT	(C),A"));
			sw.WriteLine(S("	LD	HL,") + labelPalette + S("+1"));
			sw.WriteLine(S("	LD	DE,#6400"));
			sw.WriteLine(S("	LD	BC,#0022"));
			sw.WriteLine(S("	LDIR"));
			sw.WriteLine(S("	LD	BC,#7FA0"));
			sw.WriteLine(S("	OUT	(C),C"));
}

void GenerePalette(AsmText& sw, const Param& p, bool withUnlockAsic, bool withMode, const Text& label)
{
			if (sw.Cpc.cpcPlus) {
				if (withUnlockAsic) {
					sw.WriteLine(S("UnlockAsic:"));
					sw.WriteLine(S("	DB	#FF,#00,#FF,#77,#B3,#51,#A8,#D4,#62,#39,#9C,#46,#2B,#15,#8A,#CD,#EE"));
				}
				if (!label.empty())
					sw.WriteLine(label);

				if (withMode)
					sw.WriteLine(S("\tDB\t#") + hex(((sw.Cpc.modeVirtuel == 7 ? 1 : sw.Cpc.modeVirtuel & 3) | 0x8C), 2));

				Text line = S("	DW	");
				for (int i = 0; i < 17; i++) {
					int c = i >= 16 || p.disableState[i] == 0 ? sw.Cpc.Palette[i < sw.Cpc.MaxPen() ? i : 0] : 0xFFFF;
					line += S("#") + hex(((uint8_t)(c >> 8)), 1) + hex(((uint8_t)(((c >> 4) & 0x0F) | (c << 4))), 2) + S(",");
				}
				sw.WriteLine(line.Substring(0, line.Length() - 1));
			}
			else {
				if (!label.empty())
					sw.WriteLine(label);

				Text line = S("\tDB\t");
				for (int i = 0; i < 17; i++) {
					int k = i >= 16 || p.disableState[i] == 0 ? sw.Cpc.Palette[i < sw.Cpc.MaxPen() ? i : 0] : -1;
					line += S("#") + (k == -1 ? S("FF") : hex(((int)sw.Cpc.CpcVGA[k < 27 ? k : 0]), 2)) + S(",");
				}
				if (withMode)
					line += S("#") + hex(((sw.Cpc.modeVirtuel == 7 ? 1 : sw.Cpc.modeVirtuel & 3) | 0x8C), 2);
				else
					line = line.Substring(0, line.Length() - 1);

				sw.WriteLine(line);
			}
}

void GenereEntete(AsmText& sw, int adr)
{
			if (sw.Cpc.NbLig * sw.Cpc.NbCol > 0x4000)
				adr = 0x8000;

			sw.WriteLine(S("	ORG	#") + hex(adr, 4));
			sw.WriteLine(S("	RUN	$") + NL);
			sw.WriteLine(S("	DI"));
			GenereFormatEcran(sw);
}

void GenereAffichage(AsmText& sw, bool delai, bool reboucle, bool gest128K, bool imageMode, PackMethod pkMethode)
{
			if (delai) {
				sw.WriteLine(S("	LD	HL,NewIrq"));
				sw.WriteLine(S("	LD	(#39),HL"));
				sw.WriteLine(S("	EI"));
			}
			if (sw.Cpc.modeVirtuel > 7) {
				sw.WriteLine(S("	LD	BC,#F00"));
				sw.WriteLine(S("	LD	DE,Fonte"));
				sw.WriteLine(S("SetFonte:"));
				sw.WriteLine(S("	LD	HL,DataFnt"));
				sw.WriteLine(S("	LD	A,C"));
				sw.WriteLine(S("	AND	B"));
				sw.WriteLine(S("	ADD	A,L"));
				sw.WriteLine(S("	LD	L,A"));
				sw.WriteLine(S("	LD	A,(HL)"));
				sw.WriteLine(S("	LD	(DE),A"));
				sw.WriteLine(S("	INC	DE"));
				sw.WriteLine(S("	LD	HL,DataFnt"));
				sw.WriteLine(S("	LD	A,C"));
				sw.WriteLine(S("	RRCA"));
				sw.WriteLine(S("	RRCA"));
				sw.WriteLine(S("	RRCA"));
				sw.WriteLine(S("	RRCA"));
				sw.WriteLine(S("	AND	B"));
				sw.WriteLine(S("	ADD	A,L"));
				sw.WriteLine(S("	LD	L,A"));
				sw.WriteLine(S("	LD	A,(HL)"));
				sw.WriteLine(S("	LD	(DE),A"));
				sw.WriteLine(S("	INC	DE"));
				sw.WriteLine(S("	INC	C"));
				sw.WriteLine(S("	JR	NZ,SetFonte"));
			}
			sw.WriteLine(S("Debut"));
			if (imageMode)
				sw.WriteLine(S("LD	HL,Delta0"));
			else {
				sw.WriteLine(S("	LD	IX,AnimDelta"));
				sw.WriteLine(S("Boucle:"));
				if (gest128K) {
					sw.WriteLine(S("	LD	BC,#7FC0"));
					sw.WriteLine(S("	OUT	(C),C"));
				}
				sw.WriteLine(S("	LD	H,(IX+1)"));
				sw.WriteLine(S("	LD	L,(IX+0)"));
				sw.WriteLine(S("	LD	A,H"));
				sw.WriteLine(S("	OR	H"));
				if (reboucle) {
					sw.WriteLine(S("	JR	NZ,Boucle2"));
					sw.WriteLine(S("	LD	IX,AnimDelta+") + dec((2 + (gest128K ? 1 : 0) + (delai ? 1 : 0))));
					sw.WriteLine(S("	JR	Boucle"));

					sw.WriteLine(S("Boucle2:"));
				}
				else
					sw.WriteLine(S("	RET	Z"));
			}
			if (delai) {
				sw.WriteLine(S("	LD	A,(IX+2)"));
				sw.WriteLine(S("	LD	(SyncFrame+3),A"));
			}
			if (gest128K) {
				sw.WriteLine(S("	LD	B,#7F"));
				sw.WriteLine(S("	LD	A,(IX+") + (delai ? S("3") : S("2")) + S(")"));
				sw.WriteLine(S("	OUT	(C),A"));
			}
			sw.WriteLine(S("	LD	DE,Buffer"));
			GenereDepack(sw, pkMethode, S("InitDraw"));
			if (delai) {
				sw.WriteLine(S("NewIrq:"));
				sw.WriteLine(S("	PUSH	AF"));
				sw.WriteLine(S("	LD	A,(SyncFrame+1)"));
				sw.WriteLine(S("	INC	A"));
				sw.WriteLine(S("	LD	(SyncFrame+1),A"));
				sw.WriteLine(S("	POP	AF"));
				sw.WriteLine(S("	EI"));
				sw.WriteLine(S("	RET"));
			}
			sw.WriteLine(S("InitDraw:"));
			if (delai) {
				sw.WriteLine(S("SyncFrame:"));
				sw.WriteLine(S("	LD	A,0"));
				sw.WriteLine(S("	CP	1"));
				sw.WriteLine(S("	JR	C,SyncFrame"));
				sw.WriteLine(S("	XOR	A"));
				sw.WriteLine(S("	LD	(SyncFrame+1),A"));
			}
}

void GenereDrawDC(AsmText& sw, bool delai, bool modeCol, bool gest128K, int addBC26, bool optimSpeed)
{
			sw.WriteLine(S("	LD	HL,buffer"));
			if (modeCol) {
				sw.WriteLine(S("	LD	B,(HL)		; Taille en X"));
				sw.WriteLine(S("	INC	HL"));
				sw.WriteLine(S("	LD	A,(HL)"));
				sw.WriteLine(S("	INC	HL"));
				sw.WriteLine(S("	LD	(NbLig+1),A		; Taille en Y"));
				sw.WriteLine(S("	LD	E,(HL)"));
				sw.WriteLine(S("	INC	HL"));
				sw.WriteLine(S("	LD	D,(HL)		; Adresse écran"));
				sw.WriteLine(S("	INC	HL"));
				sw.WriteLine(S("	EX	DE,HL"));
				if (delai)
					sw.WriteLine(S("	DI"));

				sw.WriteLine(S("	LD	(SauvSp+1),SP"));
				sw.WriteLine(S("	LD	SP,#C0") + hex(sw.Cpc.NbCol, 2));
				sw.WriteLine(S("Draw1:"));
				sw.WriteLine(S("	LD	(RecupHL+1),HL"));
				sw.WriteLine(S("NbLig:"));
				sw.WriteLine(S("	LD	C,0"));
				sw.WriteLine(S("Draw2:"));
				sw.WriteLine(S("	LD	A,(DE)"));
				sw.WriteLine(S("	INC	DE"));
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	LD	A,H"));
				sw.WriteLine(S("	ADD	A,#") + hex((addBC26 + 1), 2));
				sw.WriteLine(S("	LD	H,A"));
				sw.WriteLine(S("	JR	NC,Draw3"));
				sw.WriteLine(S("	ADD	HL,SP"));
				sw.WriteLine(S("Draw3:"));
				sw.WriteLine(S("	DEC	C"));
				sw.WriteLine(S("	JR	NZ,Draw2"));
				sw.WriteLine(S("RecupHL:"));
				sw.WriteLine(S("	LD	HL,0"));
				sw.WriteLine(S("	INC	HL"));
				sw.WriteLine(S("	DJNZ	Draw1"));
				sw.WriteLine(S("SauvSp:"));
				sw.WriteLine(S("	LD	SP,0"));
				if (delai)
					sw.WriteLine(S("	EI"));
			}
			else {
				sw.WriteLine(S("	LD	A,(HL)"));
				sw.WriteLine(S("	INC	HL"));
				sw.WriteLine(S("	LD	(Nbx+1),A		; Taille en X"));
				if (!optimSpeed) {
					sw.WriteLine(S("	NEG"));
					sw.WriteLine(S("	LD	(NbDec+1),A		; Ajustement pour 'BC26'"));
					sw.WriteLine(S("	LD	A,(HL)			; Taille en Y"));
					sw.WriteLine(S("	INC	HL"));
					sw.WriteLine(S("	LD	E,(HL)"));
					sw.WriteLine(S("	INC	HL"));
					sw.WriteLine(S("	LD	D,(HL)			; Adresse ecran"));
					sw.WriteLine(S("	INC	HL"));
				}
				else {
					sw.WriteLine(S("	LD	A,(HL)			; Taille en Y"));
					sw.WriteLine(S("	INC	HL"));
				}
				sw.WriteLine(S("Nbx:"));
				sw.WriteLine(S("	LD	BC,0"));
				if (optimSpeed) {
					sw.WriteLine(S("	LD	E,(HL)"));
					sw.WriteLine(S("	INC	HL"));
					sw.WriteLine(S("	LD	D,(HL)			; Adresse ecran"));
					sw.WriteLine(S("	INC	HL"));
				}
				sw.WriteLine(S("	LDIR"));
				if (!optimSpeed) {
					sw.WriteLine(S("	EX	DE,HL"));
					sw.WriteLine(S("NbDec:"));
					sw.WriteLine(S("	LD	BC,#") + hex(addBC26, 2) + S("FF			; 'BC26'"));
					sw.WriteLine(S("	ADD	HL,BC"));
					sw.WriteLine(S("	JR	NC,Suite"));
					sw.WriteLine(S("	LD	BC,#C0") + hex(sw.Cpc.NbCol, 2));
					sw.WriteLine(S("	ADD	HL,BC"));
					sw.WriteLine(S("Suite:"));
					sw.WriteLine(S("	EX	DE,HL"));
				}
				sw.WriteLine(S("	DEC	A"));
				sw.WriteLine(S("	JR	NZ,Nbx"));
			}
			sw.WriteLine(S("	INC	IX"));
			sw.WriteLine(S("	INC	IX"));
			if (delai)
				sw.WriteLine(S("	INC	IX"));

			if (gest128K)
				sw.WriteLine(S("	INC	IX"));

			sw.WriteLine(S("	JP	Boucle") + NL);
			sw.WriteLine(S("	Nolist"));
}

void GenereDrawDirect(AsmText& sw, bool gest128K)
{
			sw.WriteLine(S("	LD	HL,buffer"));
			sw.WriteLine(S("	LD	DE,#C000"));
			sw.WriteLine(S("	LD	C,(HL)"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	B,(HL)"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("DrawImgD1:"));
			sw.WriteLine(S("	LD	A,(HL)			; Deplacement"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	BIT	7,A"));
			sw.WriteLine(S("	JR	NZ,DrawImgD4"));
			sw.WriteLine(S("	ADD	A,E"));
			sw.WriteLine(S("	JR	NC,DrawImgD2"));
			sw.WriteLine(S("	INC	D"));
			sw.WriteLine(S("DrawImgD2:"));
			sw.WriteLine(S("	LD	E,A"));
			sw.WriteLine(S("	LDI				; Octet a copier"));
			sw.WriteLine(S("DrawImgD3:"));
			sw.WriteLine(S("	JP	PE,DrawImgD1"));
			sw.WriteLine(S("	INC	IX"));
			sw.WriteLine(S("	INC	IX"));
			sw.WriteLine(S("	INC	IX"));
			if (gest128K)
				sw.WriteLine(S("	INC	IX"));

			sw.WriteLine(S("	JP	Boucle") + NL);
			sw.WriteLine(S("DrawImgD4:"));
			sw.WriteLine(S("	AND	#7F"));
			sw.WriteLine(S("	ADD	A,E"));
			sw.WriteLine(S("	JR	NC,DrawImgD5"));
			sw.WriteLine(S("	INC	D"));
			sw.WriteLine(S("DrawImgD5:"));
			sw.WriteLine(S("	LD	E,A"));
			sw.WriteLine(S("	PUSH	BC"));
			sw.WriteLine(S("	LD	B,(HL)"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	A,(HL)"));
			sw.WriteLine(S("DrawImgD6:"));
			sw.WriteLine(S("	LD	(DE),A"));
			sw.WriteLine(S("	INC	DE"));
			sw.WriteLine(S("	DJNZ	DrawImgD6"));
			sw.WriteLine(S("	POP	BC"));
			sw.WriteLine(S("	CPI"));
			sw.WriteLine(S("	JR	DrawImgD3") + NL);
			sw.WriteLine(S("	Nolist"));
}

void GenereDrawBlock(AsmText& sw, int height, bool delai, bool gest128K)
{
			sw.WriteLine(S("	LD	HL,buffer"));
			sw.WriteLine(S("	LD	A,(HL)"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	(TailleBlock1+1),A"));
			sw.WriteLine(S("	LD	C,A"));
			sw.WriteLine(S("	XOR	A"));
			sw.WriteLine(S("	SUB	C"));
			sw.WriteLine(S("	LD	(TailleBlock2+1),A"));
			sw.WriteLine(S("LoopBlock:"));
			sw.WriteLine(S("	LD	A,(HL)"));
			sw.WriteLine(S("	AND	A"));
			sw.WriteLine(S("	JR	Z,FinBlock"));
			sw.WriteLine(S("	LD	D,A"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	E,(HL)		; Adresse écran"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	LD	A,") + dec(height));
			sw.WriteLine(S("TailleBlock1:"));
			sw.WriteLine(S("	LD	BC,4"));
			sw.WriteLine(S("	LDIR"));
			sw.WriteLine(S("	EX	DE,HL"));
			sw.WriteLine(S("TailleBlock2:"));
			sw.WriteLine(S("	LD	BC,#7FF"));
			sw.WriteLine(S("	ADD	HL,BC"));
			if (height != 8) {
				sw.WriteLine(S("	JR	NC,BlockOK"));
				sw.WriteLine(S("	LD	BC,#C0") + hex(sw.Cpc.NbCol, 2));
				sw.WriteLine(S("	ADD	HL,BC"));
				sw.WriteLine(S("BlockOk:"));
			}
			sw.WriteLine(S("	EX	DE,HL"));
			sw.WriteLine(S("	DEC	A"));
			sw.WriteLine(S("	JR	NZ,TailleBlock1"));
			sw.WriteLine(S("	JR	LoopBlock"));
			sw.WriteLine(S("FinBlock:"));
			sw.WriteLine(S("	INC	IX"));
			sw.WriteLine(S("	INC	IX"));
			if (delai)
				sw.WriteLine(S("	INC	IX"));

			if (gest128K)
				sw.WriteLine(S("	INC	IX"));

			sw.WriteLine(S("	JP	Boucle") + NL);
			sw.WriteLine(S("	Nolist"));
}

void GenereDrawAscii(AsmText& sw, bool frameFull, bool frameO, bool frameD, bool gest128K, bool imageMode, bool withSpeed)
{
			bool overscan = sw.Cpc.NbLig * sw.Cpc.NbCol > 0x4000;
			if (frameFull && !imageMode) {
				sw.WriteLine(S("	LD	HL,Buffer"));
				sw.WriteLine(S("	LD	A,(HL)"));
				sw.WriteLine(S("	CP	'D'"));
				sw.WriteLine(S("	JR	Z,DrawImgD"));
				sw.WriteLine(S("	CP	'I'"));
				sw.WriteLine(S("	JR	Z,DrawImgI"));
			}
			else
				if (frameO)
					sw.WriteLine(S("	LD	HL,Buffer"));

			if (!frameD) {
				sw.WriteLine(S("	LD	BC,") + (overscan ? S("#0200") : S("#C000")));
				if (sw.Cpc.modeVirtuel == 7)
					sw.WriteLine(S("	LD	D,Fonte/256"));

				sw.WriteLine(S("DrawImgO:"));
				if (sw.Cpc.modeVirtuel > 7) {
					sw.WriteLine(S("	LD	D,Fonte/512"));
					if (!frameO)
						sw.WriteLine(S("	INC	HL"));

					sw.WriteLine(S("	LD	E,(HL)			; Code ASCII"));
					sw.WriteLine(S("	EX	DE,HL"));
					sw.WriteLine(S("	ADD	HL,HL"));
				}
				else {
					if (!frameO)
						sw.WriteLine(S("	INC	HL"));

					sw.WriteLine(S("	LD	A,(HL)			; Code ASCII"));
					sw.WriteLine(S("	AND	#0F"));
					sw.WriteLine(S("	RLCA"));
					sw.WriteLine(S("	RLCA"));
					sw.WriteLine(S("	LD	E,A"));
					sw.WriteLine(S("	EX	DE,HL"));
				}
				sw.WriteLine(S("	LD	A,(HL)"));
				sw.WriteLine(S("	INC	L"));
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	SET	3,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(HL)"));
					sw.WriteLine(S("	INC	L"));
				}
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	SET	4,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(HL)"));
					sw.WriteLine(S("	INC	L"));
				}
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	RES	3,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(HL)"));
					sw.WriteLine(S("	INC	L"));
				}
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	SET	5,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)			; Code ASCII"));
					sw.WriteLine(S("	AND	#F0"));
					sw.WriteLine(S("	RRCA"));
					sw.WriteLine(S("	RRCA"));
					sw.WriteLine(S("	ADD	A,3"));
					sw.WriteLine(S("	LD	L,A"));
				}
				sw.WriteLine(S("	LD	A,(HL)"));
				if (sw.Cpc.modeVirtuel == 7)
					sw.WriteLine(S("	DEC	L"));
				else
					sw.WriteLine(S("	INC	HL"));

				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	SET	3,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(HL)"));
					sw.WriteLine(S("	DEC	L"));
				}
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	RES	4,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(HL)"));
					sw.WriteLine(S("	DEC	L"));
				}
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	RES	3,B"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(HL)"));
				}
				sw.WriteLine(S("	LD	(BC),A"));
				sw.WriteLine(S("	RES	5,B"));
				sw.WriteLine(S("	EX	DE,HL"));
				if (frameO)
					sw.WriteLine(S("	INC	HL"));

				sw.WriteLine(S("	INC	BC"));
				sw.WriteLine(S("	BIT	3,B"));
				sw.WriteLine(S("	JR	Z,DrawImgO"));
				if (overscan) {
					sw.WriteLine(S("	LD	A,B"));
					sw.WriteLine(S("	ADD	A,#40"));
					sw.WriteLine(S("	LD	B,A"));
					sw.WriteLine(S("	JP	P,DrawImgO"));
				}
			}
			if (!frameD) {
				if (!imageMode) {
					sw.WriteLine(S("DrawImgI:"));
					sw.WriteLine(S("	INC	IX"));
					sw.WriteLine(S("	INC	IX"));
					if (withSpeed)
						sw.WriteLine(S("	INC	IX"));

					if (gest128K)
						sw.WriteLine(S("	INC	IX"));

					sw.WriteLine(S("	JP	Boucle"));
				}
				else
					GenereTspace(sw, true);
			}
			if (!frameO) {
				if (!frameD)
					sw.WriteLine(S("DrawImgD:"));

				sw.WriteLine(S("	LD	HL,#C000"));
				if (frameD)
					sw.WriteLine(S("	LD	IY,Buffer"));
				else
					sw.WriteLine(S("	LD	IY,Buffer+1"));

				sw.WriteLine(S("	LD	C,(IY+0)"));
				sw.WriteLine(S("	LD	B,(IY+1)"));
				if (frameD) {
					sw.WriteLine(S("	LD	A,B"));
					sw.WriteLine(S("	OR	C"));
					sw.WriteLine(S("	JR	Z,EndDraw"));
				}
				sw.WriteLine(S("DrawImgD1:"));
				sw.WriteLine(S("	LD	D,0"));
				sw.WriteLine(S("	LD	E,(IY+2)			; Déplacement"));
				sw.WriteLine(S("	ADD	HL,DE			; Ajouter à aresse écran"));
				if (sw.Cpc.modeVirtuel > 7) {
					sw.WriteLine(S("	EX	DE,HL"));
					sw.WriteLine(S("	LD	H,Fonte/512"));
					sw.WriteLine(S("	LD	L,(IY+3)			; Code ASCII"));
					sw.WriteLine(S("	ADD	HL,HL"));
					sw.WriteLine(S("	EX	DE,HL"));
				}
				else {
					sw.WriteLine(S("	LD	D,Fonte/256"));
					sw.WriteLine(S("	LD	A,(IY+3)			; Code ASCII"));
					sw.WriteLine(S("	AND	#0F"));
					sw.WriteLine(S("	RLCA"));
					sw.WriteLine(S("	RLCA"));
					sw.WriteLine(S("	LD	E,A"));
				}
				sw.WriteLine(S("	LD	A,(DE)"));
				sw.WriteLine(S("	INC	E"));
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	SET	3,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)"));
					sw.WriteLine(S("	INC	E"));
				}
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	SET	4,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)"));
					sw.WriteLine(S("	INC	E"));
				}
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	RES	3,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)"));
					sw.WriteLine(S("	INC	E"));
				}
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	SET	5,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(IY+3)			; Code ASCII"));
					sw.WriteLine(S("	AND	#F0"));
					sw.WriteLine(S("	RRCA"));
					sw.WriteLine(S("	RRCA"));
					sw.WriteLine(S("	ADD	A,3"));
					sw.WriteLine(S("	LD	E,A"));
				}
				sw.WriteLine(S("	LD	A,(DE)"));
				if (sw.Cpc.modeVirtuel == 7)
					sw.WriteLine(S("	DEC	E"));

				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	SET	3,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)"));
					sw.WriteLine(S("	DEC E"));
				}
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	RES	4,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)"));
					sw.WriteLine(S("	DEC	E"));
				}
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	RES	3,H"));
				if (sw.Cpc.modeVirtuel == 7) {
					sw.WriteLine(S("	LD	A,(DE)"));
				}
				sw.WriteLine(S("	LD	(HL),A"));
				sw.WriteLine(S("	RES	5,H"));
				sw.WriteLine(S("	INC	IY"));
				sw.WriteLine(S("	INC	IY"));
				sw.WriteLine(S("	CPI"));
				sw.WriteLine(S("	JP	PE,DrawImgD1"));
				if (frameD) {
					if (!imageMode) {
						sw.WriteLine(S("EndDraw"));
						sw.WriteLine(S("	INC	IX"));
						sw.WriteLine(S("	INC	IX"));
						if (withSpeed)
							sw.WriteLine(S("	INC	IX"));

						if (gest128K)
							sw.WriteLine(S("	INC	IX"));

						sw.WriteLine(S("	JP	Boucle"));
					}
					else
						GenereTspace(sw, true);
				}
				else
					sw.WriteLine(S("	JR	DrawImgI"));
			}
			sw.WriteLine(S("	Nolist"));
}

void GenereFin(AsmText& sw, int ltot, bool force8000, bool withCode)
{
			sw.WriteLine(S("; Taille totale animation = ") + dec(ltot) + S(" (#") + hex(ltot, 4) + S(")"));
			sw.WriteLine(S("_EndCode:"));
			sw.WriteLine(S("	List"));
			if (sw.Cpc.modeVirtuel >= 7) {
				int align = sw.Cpc.modeVirtuel == 7 ? 256 : 512;
				sw.WriteLine(S("	Align	") + dec(align) + S("		; Doit être un multiple de ") + dec(align));
				sw.WriteLine(S("Fonte:"));
				if (sw.Cpc.modeVirtuel == 7) {
					const int ordreAdr[4] = { 0, 1, 3, 2 };
					for (int i = 0; i < 16; i++) {
						Text str = S("	DB	");
						for (int ym = 0; ym < 4; ym++) {
							uint8_t octet = 0;
							for (int xm = 0; xm < 4; xm++)
								octet |= (uint8_t)(sw.Cpc.tabOctetMode[sw.Cpc.trameM1[i][xm][ordreAdr[ym]]] >> xm);

							str += S("#") + hex(octet, 2) + S(",");
						}
						sw.WriteLine(str.Substring(0, str.Length() - 1));
					}
				}
				else
					sw.WriteLine(S("	DS	") + dec(align) + NL);
			}

			if (withCode && sw.Cpc.modeVirtuel >= 7 && force8000)
				sw.WriteLine(S("	Org	#8000"));

			sw.WriteLine(S("buffer") + (force8000 ? S("	equ	#8000") : S(":")));
			if (withCode && sw.Cpc.modeVirtuel > 7) {
				sw.WriteLine(S("DataFnt:"));
				sw.WriteLine(S("	DB	#00,#C0,#0C,#CC,#30,#F0,#3C,#FC,#03,#C3,#0F,#CF,#33,#F3,#3F,#FF"));
			}
}

void GenerePointeurs(AsmText& sw, int nbImages, const int* bank, const int* speed, bool gest128K)
{
			if (gest128K) {
				sw.WriteLine(S("	ORG EndBank0"));
				sw.WriteLine(S("	Write Direct -1,-1,#C0"));
			}
			sw.WriteLine(S("AnimDelta:"));
			for (int i = 0; i < nbImages; i++) {
				sw.WriteLine(S("\tDW\tDelta") + dec(i));
				if (speed != nullptr)
					sw.WriteLine(S("\tDB\t#") + hex(roundToInt(std::max(1.0, std::min(255.0, speed[i] / 3.333333))), 2));

				if (gest128K)
					sw.WriteLine(S("\tDB\t#") + hex(bank[i], 2));
			}
			sw.WriteLine(S("\tDW\t#0"));
}

void GenereTspace(AsmText& sw, bool wait, bool withoutRet)
{
			sw.WriteLine(S("TstSpace:"));
			sw.WriteLine(S("	LD	BC,#F40E"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	LD	BC,#F6C0"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	XOR	A"));
			sw.WriteLine(S("	OUT	(C),A"));
			sw.WriteLine(S("	LD	BC,#F792"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	LD	BC,#F645"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	LD	B,#F4"));
			sw.WriteLine(S("	IN	A,(C)"));
			sw.WriteLine(S("	LD	BC,#F782"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	LD	BC,#F600"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	INC	A"));
			if (wait)
				sw.WriteLine(S("	JR	Z,TstSpace"));

			if (!withoutRet)
				sw.WriteLine(S("	RET"));
}

void GenereAfficheStd(AsmText& sw, bool overscan, PackMethod pkMethode, const Text& labelMedia, const Text& labelPalette)
{
			sw.WriteLine(S("	DI"));
			if (sw.Cpc.cpcPlus)
				GenereInitPlus(sw, labelPalette);
			else
				GenereInitOld(sw, labelPalette);

			GenereFormatEcran(sw);
			if (pkMethode != PackMethod::None) {
				sw.WriteLine(S("	LD	HL,") + labelMedia);
				sw.WriteLine(S("	LD	DE,#") + (overscan ? S("0200") : S("C000")));
				sw.WriteLine(S("	CALL	Depack"));
			}
			if (pkMethode == PackMethod::ZX0Ovs) {
				sw.WriteLine(S("	EX	DE,HL"));
				sw.WriteLine(S("	DEC	HL"));
				sw.WriteLine(S("	LD	IX,Zones"));
				sw.WriteLine(S("LoopDepack:"));
				sw.WriteLine(S("	LD	A,(IX+1)"));
				sw.WriteLine(S("	AND	A"));
				sw.WriteLine(S("	JR	Z,TstSpace"));
				sw.WriteLine(S("	LD	D,A"));
				sw.WriteLine(S("	LD	E,(IX+0)"));
				sw.WriteLine(S("	LD	B,(IX+3)"));
				sw.WriteLine(S("	LD	C,(IX+2)"));
				sw.WriteLine(S("	LDDR"));
				sw.WriteLine(S("	INC	IX"));
				sw.WriteLine(S("	INC	IX"));
				sw.WriteLine(S("	INC	IX"));
				sw.WriteLine(S("	INC	IX"));
				sw.WriteLine(S("	JR	LoopDepack"));
			}
			GenereTspace(sw, true, true);
			sw.WriteLine(S("	LD	BC,#BC0C"));
			sw.WriteLine(S("	LD	A,#30"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	INC	B"));
			sw.WriteLine(S("	OUT	(C),A"));
			sw.WriteLine(S("	EI"));
			sw.WriteLine(S("	RET"));
			GenereDepack(sw, pkMethode);
			if (pkMethode == PackMethod::ZX0Ovs) {
				sw.WriteLine(S("Zones:"));
				sw.WriteLine(S("	DW	#7EBF,#6C0,#76BF,#6C0,#6EBF,#6C0,#66BF,#6C0,#5EBF,#6C0,#56BF,#6C0,#4EBF,#6C0,#46BF,#CC0"));
				sw.WriteLine(S("	DW	#37FF,#600,#2FFF,#600,#27FF,#600,#1FFF,#600,#17FF,#600,#0FFF,#600 ;,#07FF,#600	; Si decompactage en #200, pas necessaire"));
				sw.WriteLine(S("	DW	#0"));
			}
}

void GenereAfficheModeX(AsmText& sw, const LineTable& colMode5, bool isOverscan, PackMethod pkMethode, const Text& labelMedia)
{
			GenereFormatEcran(sw, true);
			sw.WriteLine(S("	LD	HL,") + labelMedia);
			sw.WriteLine(S("	LD	DE,#") + (isOverscan ? S("0200") : S("C000")));
			sw.WriteLine(S("	CALL	Depack"));
			sw.WriteLine(S("	DI"));
			sw.WriteLine(S("	LD	HL,#C9FB"));
			sw.WriteLine(S("	LD	(#38),HL"));
			sw.WriteLine(S("	EI"));
			sw.WriteLine(S("WaitVbl:"));
			sw.WriteLine(S("	LD	B,#F5"));
			sw.WriteLine(S("	IN	A,(C)"));
			sw.WriteLine(S("	RRA"));
			sw.WriteLine(S("	JR	NC,WaitVbl"));
			sw.WriteLine(S("WaitEnd:"));
			sw.WriteLine(S("	IN	A,(C)"));
			sw.WriteLine(S("	RRA"));
			sw.WriteLine(S("	JR	C,WaitEnd"));
			sw.WriteLine(S("	LD	HL,Color01			; Initialiser les couleurs fixes de l'image (0 et 1)"));
			sw.WriteLine(S("	LD	BC,#7F00"));
			sw.WriteLine(S("	OUT	(C),C			; Sélection pen 0"));
			sw.WriteLine(S("	OUTI				; Mise à jour couleur"));
			sw.WriteLine(S("	INC	C"));
			sw.WriteLine(S("	OUT	(C),C			; Sélection pen 1"));
			sw.WriteLine(S("	OUTI				; Mise à jour couleur"));
			if (isOverscan) {
				sw.WriteLine(S("	LD	BC,#BC0C"));
				sw.WriteLine(S("	OUT	(C),C"));
				sw.WriteLine(S("	INC	B"));
				sw.WriteLine(S("	INC	C"));
				sw.WriteLine(S("	OUT	(C),C"));
			}
			sw.WriteLine(S("	HALT"));
			sw.WriteLine(S("	HALT"));
			sw.WriteLine(S("	HALT"));
			sw.WriteLine(S("	HALT"));
			sw.WriteLine(S("	HALT"));
			sw.WriteLine(S("	HALT"));
			sw.WriteLine(S("	DI"));
			sw.WriteLine(S("	LD	BC,") + dec((sw.Cpc.NbLig == 200 ? 634 : 268)));
			sw.WriteLine(S("	DEC	BC"));
			sw.WriteLine(S("	LD	A,B"));
			sw.WriteLine(S("	OR	C"));
			sw.WriteLine(S("	JR	NZ,$-3			; Attendre première ligne visible de l'écran"));
			sw.WriteLine(S("Boucle:"));
			sw.WriteLine(S("	LD	HL,ColorModeX		; Initaliser les couleurs variables"));
			sw.WriteLine(S("	LD	DE,") + sw.Cpc.NbLig + S("			; par lignes de l'image (2 et 3)"));
			sw.WriteLine(S("LoopLineX:"));
			sw.WriteLine(S("	LD	BC,#7F02"));
			sw.WriteLine(S("	OUT	(C),C			; Sélection pen 2"));
			sw.WriteLine(S("	OUTI				; Mise à jour couleur"));
			sw.WriteLine(S("	INC C"));
			sw.WriteLine(S("	OUT	(C),C			; Sélection pen 3"));
			sw.WriteLine(S("	OUTI				; Mise à jour couleur"));
			sw.WriteLine(S("	LD	B,8"));
			sw.WriteLine(S("	DJNZ	$"));
			sw.WriteLine(S("	CP	(HL)"));
			sw.WriteLine(S("	DEC	DE"));
			sw.WriteLine(S("	LD	A,D"));
			sw.WriteLine(S("	OR	E"));
			sw.WriteLine(S("	JR	NZ,LoopLineX"));

			sw.WriteLine(S("	LD	BC,") + dec((sw.Cpc.NbLig == 200 ? 1022 : 364)));
			sw.WriteLine(S("	DEC	BC"));
			sw.WriteLine(S("	LD	A,B"));
			sw.WriteLine(S("	OR	C"));
			sw.WriteLine(S("	JR	NZ,$-3"));
			sw.WriteLine(S("	CP	(HL)"));
			if (sw.Cpc.NbLig == 200)
				sw.WriteLine(S("	CP	(HL)"));

			sw.WriteLine(S("	JR	Boucle"));


			GenereDepack(sw, pkMethode);
			sw.WriteLine(S("Color01:"));
			sw.WriteLine(S("	DB	'") + chr(sw.Cpc.CpcVGA[colMode5[0][0]]) + chr(sw.Cpc.CpcVGA[colMode5[0][1]]) + S("'"));
			sw.WriteLine(S("ColorModeX:"));
			Text line = S("\tDB\t'");
			int nbOctets = 0;
			for (int y = 0; y < 272; y++) {
				line += chr(sw.Cpc.CpcVGA[colMode5[y][2]]) + chr(sw.Cpc.CpcVGA[colMode5[y][3]]);
				if (++nbOctets >= 16) {
					sw.WriteLine(line + S("'"));
					line = S("\tDB\t'");
					nbOctets = 0;
				}
			}
			if (nbOctets > 0)
				sw.WriteLine(line + S("'"));
}

void GenereAfficheModeEgx(AsmText& sw, const int* palette, bool overscan, PackMethod pkMethode, const Text& labelMedia, const Text& labelPalette)
{
			sw.WriteLine(S("	LD	HL,") + labelPalette);
			sw.WriteLine(S("	LD	B,(HL)"));
			sw.WriteLine(S("	LD	C,B"));
			sw.WriteLine(S("	CALL	#BC38"));
			sw.WriteLine(S("	XOR	A"));
			sw.WriteLine(S("	LD	HL,") + labelPalette);
			sw.WriteLine(S("SetPalette:"));
			sw.WriteLine(S("	LD	B,(HL)"));
			sw.WriteLine(S("	LD	C,B"));
			sw.WriteLine(S("	PUSH	AF"));
			sw.WriteLine(S("	PUSH	HL"));
			sw.WriteLine(S("	CALL	#BC32"));
			sw.WriteLine(S("	POP	HL"));
			sw.WriteLine(S("	POP	AF"));
			sw.WriteLine(S("	INC	HL"));
			sw.WriteLine(S("	INC	A"));
			sw.WriteLine(S("	CP	#10"));
			sw.WriteLine(S("	JR	NZ,SetPalette"));

			GenereFormatEcran(sw);
			sw.WriteLine(S("	LD	HL,") + labelMedia);
			sw.WriteLine(S("	LD	DE,#") + (overscan ? S("0200") : S("C000")));
			sw.WriteLine(S("	CALL	Depack"));

			sw.WriteLine(S("	DI"));
			sw.WriteLine(S("WaitVbl:"));
			sw.WriteLine(S("	LD	B,#F5"));
			sw.WriteLine(S("	IN	A,(C)"));
			sw.WriteLine(S("	RRA"));
			sw.WriteLine(S("	JR	NC,WaitVbl"));
			sw.WriteLine(S("WaitEnd:"));
			sw.WriteLine(S("	IN	A,(C)"));
			sw.WriteLine(S("	RRA"));
			sw.WriteLine(S("	JR	C,WaitEnd"));
			sw.WriteLine(S("	LD	HL,#012F"));
			int mode = 0x8C01;
			if (sw.Cpc.modeVirtuel == 4)
				mode = 0x8D03;

			if (sw.Cpc.yEgx == 2)
				mode += 0x100;

			sw.WriteLine(S("	LD	DE,#") + hex(mode, 4));
			sw.WriteLine(S("SetMode:"));
			sw.WriteLine(S("	LD	B,#7F"));
			sw.WriteLine(S("	OUT	(C),D			; Premier mode pour les lignes paires"));
			sw.WriteLine(S("	LD	A,D"));
			sw.WriteLine(S("	XOR	E				; Inversion du mode pour la prochaine ligne"));
			sw.WriteLine(S("	LD	D,A"));
			sw.WriteLine(S("	LD	B,11"));
			sw.WriteLine(S("WaitNextLine:"));
			sw.WriteLine(S("	DJNZ	WaitNextLine"));
			sw.WriteLine(S("	BIT	0,(HL)			; + 4 Nops"));
			sw.WriteLine(S("	DEC	HL"));
			sw.WriteLine(S("	LD	A,H"));
			sw.WriteLine(S("	OR	L"));
			sw.WriteLine(S("	JR	NZ,SetMode"));
			sw.WriteLine(S("	CALL	TstSpace"));
			sw.WriteLine(S("	JR	Z,WaitVbl"));

			sw.WriteLine(S("	LD	BC,#BC0C"));
			sw.WriteLine(S("	LD	A,#30"));
			sw.WriteLine(S("	OUT	(C),C"));
			sw.WriteLine(S("	INC	B"));
			sw.WriteLine(S("	OUT	(C),A"));
			sw.WriteLine(S("	EI"));
			sw.WriteLine(S("	RET"));

			GenereTspace(sw, false);
			GenereDepack(sw, pkMethode);

			Text line = S("\tDB\t");
			for (int y = 0; y < 16; y++)
				line += dec(palette[y]) + S(",");

			sw.WriteLine(labelPalette);
			sw.WriteLine(line.Substring(0, line.Length() - 1));
}

// ---- GenSplitAsm.cs ----

namespace split {

void WriteDebFile(AsmText& wr)
{
			wr.WriteLine(S("	RUN $"));
			wr.WriteLine(S(""));
			wr.WriteLine(S("	nolist"));
			wr.WriteLine(S("	DI"));
			wr.WriteLine(S("	LD	HL,ImageCmp"));
			wr.WriteLine(S("	LD	DE,#0200"));
			wr.WriteLine(S("	CALL	Depack"));
			wr.WriteLine(S("	LD	HL,#C9FB"));
			wr.WriteLine(S("	LD	(#38),HL"));
			wr.WriteLine(S("	LD	HL,Overscan"));
			wr.WriteLine(S("	LD	B,#BC"));
			wr.WriteLine(S("BclCrtc"));
			wr.WriteLine(S("	LD	A,(HL)"));
			wr.WriteLine(S("	INC	HL"));
			wr.WriteLine(S("	AND	A"));
			wr.WriteLine(S("	JR	Z,SetPalette"));
			wr.WriteLine(S("	OUT	(C),A"));
			wr.WriteLine(S("	INC	B"));
			wr.WriteLine(S("	INC	B"));
			wr.WriteLine(S("	OUTI"));
			wr.WriteLine(S("	DEC	B"));
			wr.WriteLine(S("	JR	BclCrtc"));
			wr.WriteLine(S("SetPalette"));
			wr.WriteLine(S("	LD	B,#7F"));
			wr.WriteLine(S("BclPalette"));
			wr.WriteLine(S("	OUT	(C),A"));
			wr.WriteLine(S("	INC	B"));
			wr.WriteLine(S("	OUTI"));
			wr.WriteLine(S("	INC	A"));
			wr.WriteLine(S("	CP	16"));
			wr.WriteLine(S("	JR	NZ,BclPalette"));
			wr.WriteLine(S("Boucle"));
			wr.WriteLine(S("	EI"));
			wr.WriteLine(S("	LD	B,#F5"));
			wr.WriteLine(S("WaitVbl"));
			wr.WriteLine(S("	IN	A,(C)"));
			wr.WriteLine(S("	RRA"));
			wr.WriteLine(S("	JR	NC,WaitVbl"));
			wr.WriteLine(S("	HALT"));
			wr.WriteLine(S("	DI"));
}

bool GenereRetard(AsmText& wr, int nbPixels, bool crashBC, bool canUseHL)
{
			bool ret = false;
			int nbNops = nbPixels >> 3;
			if (nbNops >= 1024) {
				int bc = (nbNops - (crashBC ? 2 : 4)) / 7;
				int delai = ((bc * 7) + (crashBC ? 2 : 4));
				wr.WriteLine(S("	LD	BC,") + dec(bc) + S("			; Attendre ") + dec(delai) + S(" NOPs (7*") + dec(bc) + S("+") + dec((crashBC ? 2 : 4)) + S(")"));
				wr.WriteLine(S("	DEC	BC"));
				wr.WriteLine(S("	LD	A,B"));
				wr.WriteLine(S("	OR	C"));
				wr.WriteLine(S("	JR	NZ,$-3"));
				if (!crashBC)
					wr.WriteLine(S("	LD	B,#7F"));

				nbNops -= ((bc * 7) + (crashBC ? 2 : 4));
			}
			if (nbNops > 11 && nbNops < 17) {
				wr.WriteLine(S("	EX	(SP),HL			; Attendre 6 NOPs"));
				wr.WriteLine(S("	EX	(SP),HL			; Attendre 6 NOPs"));
				nbNops -= 12;
			}
			if (nbNops > 8 && nbNops < 17) {
				wr.WriteLine(S("	PUSH	IX			; Attendre 5 NOPs"));
				wr.WriteLine(S("	POP	IX			; Attendre 4 NOPs"));
				nbNops -= 9;
			}
			if (nbNops > 7 && nbNops < 1024) {
				int b = (nbNops - (crashBC ? 1 : 3)) >> 2;
				int delai = ((b << 2) + (crashBC ? 1 : 3));
				wr.WriteLine(S("	LD	B,") + dec(b) + S("			; Attendre ") + dec(delai) + S(" NOPs (4*") + dec(b) + S("+") + dec((crashBC ? 1 : 3)) + S(")"));
				wr.WriteLine(S("	DJNZ	$-0"));
				if (!crashBC)
					wr.WriteLine(S("	LD	B,#7F"));

				nbNops -= delai;
			}
			if (nbNops >= 4) {
				wr.WriteLine(S("	ADD	IX,BC			; Attendre 4 NOPs"));
				nbNops -= 4;
			}
			if (nbNops >= 3 && canUseHL) {
				wr.WriteLine(S("	ADD	HL,BC			; Attendre 3 NOPs"));
				nbNops -= 3;
				ret = true;
			}
			if (nbNops >= 2) {
				wr.WriteLine(S("	CP	(HL)			; Attendre 2 NOPs"));
				nbNops -= 2;
			}
			for (; nbNops-- > 0;)
				wr.WriteLine(S("	NOP				; Attendre 1 NOP"));

			return ret;
}

void GenereDatas(AsmText& sw, const uint8_t* tabByte, int length, int nbOctetsLigne, int ligneSepa, const Text& labelSepa)
{
			Text line = S("\tDB\t");
			int nbOctets = 0, nbLigne = 0, indiceLabel = 0;
			if (!labelSepa.isNull()) {
				sw.WriteLine(labelSepa + dec(indiceLabel, 2));
				indiceLabel++;
			}
			for (int i = 0; i < length; i++) {
				line += S("#") + hex(tabByte[i], 2) + S(",");
				if (++nbOctets >= nbOctetsLigne) {
					sw.WriteLine(line.Substring(0, line.Length() - 1));
					line = S("\tDB\t");
					nbOctets = 0;
					if (i < length - 1 && ++nbLigne >= ligneSepa && ligneSepa > 0) {
						nbLigne = 0;
						if (!labelSepa.isNull()) {
							sw.WriteLine(labelSepa + dec(indiceLabel, 2));
							indiceLabel++;
						}
						else
							line += NL;
					}
				}
			}
			if (nbOctets > 0)
				sw.WriteLine(line.Substring(0, line.Length() - 1));

			sw.WriteLine(S("; Taille totale ") + dec(length) + S(" octets"));
}

void WriteEndFile(AsmText& wr, const SplitPalette& palette, PackMethod pkMethode)
{
			wr.WriteLine(S("	JP	Boucle"));
			wr.WriteLine(S(""));
			GenereDepack(wr, pkMethode);
			wr.WriteLine(S(""));
			wr.WriteLine(S("	List"));
			wr.WriteLine(S(""));
			wr.WriteLine(S("Overscan"));
			wr.WriteLine(S("	DB	1,48,2,50,3,#8E,6,34,7,35,12,13,13,0,0"));
			wr.WriteLine(S("Palette"));
			wr.Write(S("	DB	"));
			for (int i = 0; i < 16; i++) {
				int col = wr.Cpc.CpcVGA[palette[0][0][i] != 0xFFFF ? palette[0][0][i] : 0];
				wr.Write(S("#") + hex(col, 2));
				if (i < 15)
					wr.Write(S(","));
			}
			wr.WriteLine(S(""));
}

}  // namespace split
