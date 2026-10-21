# TuxSCR

Convertisseur d'images en écrans pour Amstrad CPC et CPC Plus, sous la forme
d'un plugin de [TuxAPE](../TuxAPE). C'est
[ConvImgCpc](https://github.com/IMPact-Scene/ConvImgCpc) de Ludovic Deplanque
(Demoniak), dans l'état de ses sources du 1er octobre 2026, réécrit en C++ :
mêmes fonctions, mêmes fichiers produits, dans une fenêtre de TuxAPE et dans
un programme en ligne de commande. TuxSCR y ajoute les **interruptions de
palette** du CPC Plus.

## Ce qu'il fait

La fenêtre s'ouvre par le menu **Plugins ▸ TuxSCR – Screen Converter...**
(Ctrl+Maj+F5) ou par son bouton dans le panneau du bas. À gauche, ce que ConvImgCpc
a dans sa fenêtre principale ; à droite, sa fenêtre « Image CPC » : la palette,
l'image, et dessous les outils en onglets.

| Partie | Ce qu'on y fait |
|---|---|
| Files | lire une image (BMP, GIF, PNG, JPEG), un écran `.SCR` (ConvImgCpc, OCP, Impdraw), une animation `.IMP`, une palette `.PAL` ou `.KIT`, des réglages `.xml` ; convertir ; enregistrer |
| CPC resolution | colonnes et lignes (standard 80 × 200, overscan 96 × 272, ou autre), mode : 0, 1, 2, EGX1, EGX2, mode X, split rasters, ASC-UT, ASC0, ASC1, ASC2, capture de sprites |
| Source image size | Fit, KeepSmaller, KeepLarger, taille et position choisies (aussi à la souris sur l'image), taille d'origine |
| Dithering and rendering | vingt matrices de tramage, pourcentage, diffusion d'erreur, lissage, trames [TC], mode ImpDraw |
| Color management | CPC ou CPC+, tri, noir et blanc, réductions, 24/12/9/6 bits, K-means, luminosité, saturation, contraste, rouge, vert, bleu |
| Palette | chaque encre : sa couleur (un clic la change), une case qui la verrouille, une case qui l'écarte ; copie dans le presse-papier, générateur de dégradé (Plus) |
| Palette interrupts (CPC+) | voir plus bas |
| Edit | crayon, loupe, copie de bloc, pipette, remplissage, annuler et refaire, retournements, blocs en fenêtres OCP `.WIN` |
| Tiles and Grids | grille de tuiles et jeu de tuiles en assembleur, grille verticale (NOPs), copie de l'image dans le presse-papier |
| Animation | les images d'un GIF animé ou d'un `.IMP` : durées, suppression, lecture, GIF animé des images converties |
| Sprites, splits, rasters | éditeur de sprites matériels du Plus et capture depuis l'image, éditeur de split rasters, table de rasters du Plus |

**Save** propose les formats de ConvImgCpc : écran `.SCR` (avec ou sans la
routine d'affichage et la palette), PNG, sprite en assembleur (compacté ou
non), écran compacté `.CMP` ou en assembleur avec sa routine, sur une image
disque `.DSK` (nouvelle ou existante), animation « DeltaPack » en assembleur,
animation Impdraw `.IMP`, tuiles, réglages, palettes `.PAL` et `.KIT`, les
deux écrans d'une image EGX, Bump, DiffAnim, Tunnel. Les compactages sont les
siens : Standard, ZX0 (deux formats), ZX1, ZX0Ovs.

Avec TuxAPE : **Show in TuxAPE** assemble et lance sur le CPC émulé le
programme qui affiche l'image (une touche le termine) ; **From the CPC Screen**
reprend ce que le CPC émulé affiche, avec sa palette.

Les textes sont dans la langue de TuxAPE, anglais ou français : ceux de
ConvImgCpc pour ce qui vient de lui (voir « Les langues »).

## Interruptions de palette (CPC+)

Sur un Plus, l'ASIC peut interrompre le Z80 à une ligne choisie de l'écran :
on en profite pour changer les couleurs des encres. Une image en mode 1 n'a
que quatre encres, mais elle peut ainsi avoir quatre couleurs en haut, quatre
autres plus bas, et encore d'autres dessous.

Onglet **Palette interrupts (CPC+)**, sur un CPC+ en mode 0, 1 ou 2 :

1. cocher **Add Palette Interrupt** ;
2. **Add** ajoute une interruption. Son numéro de **ligne** se règle avec les
   flèches ou au clavier : un trait la montre sur l'image et la suit. On peut
   en ajouter autant qu'on veut ;
3. à côté de la ligne, la palette à partir de cette ligne. Elle est héritée de
   celle du dessus (celle de l'image pour la première). Un clic sur une encre
   lui donne une autre couleur, sans toucher à la palette de l'image ni aux
   autres lignes ; un point marque les encres ainsi redéfinies, et un clic
   droit rend à une encre la couleur du dessus. **Auto** cherche dans l'image
   les meilleures couleurs pour les lignes de cette interruption ;
4. l'image montre aussitôt les nouvelles couleurs : redéfinir une encre
   recolore ce que cette encre dessine à partir de la ligne, aucun point ne
   change d'encre, et **Apply to Screen** garde ces couleurs. Le bouton
   **Auto** d'une ligne fait autre chose : il cherche dans l'image les
   couleurs de ses lignes et convertit ces lignes avec elles (il reste
   enfoncé ; relâché, la ligne reprend la palette du dessus) ;
5. **Export as Text...** écrit les numéros de ligne et leur palette, en
   couleurs de l'ASIC (`#GRB`), dans un texte qui s'assemble tel quel :

```
PalIntNb:
	DB	2			; Nombre d'interruptions
PalIntLigne0:				; Ligne 0 : palette de l'image
	DW	#000,#0F0,#F00,#FFF
PalIntLigne1:
	DB	60			; Ligne 60
	DW	#000,#FF0,#F00,#FFF
PalIntLigne2:
	DB	110			; Ligne 110
	DW	#000,#C0F,#206,#FCF
```

Les interruptions sont gardées avec les réglages (`.xml`). L'écran en
assembleur avec sa routine (« Include display code in image ») et **Show in
TuxAPE** contiennent alors la routine d'interruption, qui écrit les couleurs
entre deux lignes :

- jusqu'à trois couleurs changées à une ligne (deux en overscan) sont écrites
  une à une, toutes avant le premier point de la ligne ;
- au-delà, elles sont poussées par la pile, cinq par fin de ligne (quatre en
  overscan) : les quatre encres du mode 1 changent donc ensemble, au point
  près ;
- au-delà encore (mode 0, plus de cinq couleurs à la même ligne), elles sont
  réparties sur les fins des lignes du dessus : les encres à partir de la
  sixième prennent leur couleur une à trois lignes plus tôt, plutôt qu'en
  cours de ligne ;
- le registre de l'ASIC est sur un octet : une interruption après la ligne 255
  (overscan) est attendue par la routine depuis la précédente.

L'écran binaire `.SCR` avec routine incluse n'a pas la place pour cette
routine : il garde la palette de l'image.

Le programme écrit a été vérifié en l'assemblant avec l'assembleur de TuxAPE,
en le lançant sur le Plus de TuxAPE et en relisant l'image du moniteur ligne
par ligne : chaque encre a la bonne couleur du premier au dernier point de
chaque ligne. Ce test se servait du cœur de TuxAPE, qu'un plugin ne prend
pas : il n'est plus dans le dépôt (il reste dans son historique, avant la
mise en conformité avec `PLUGINS.md`).

## Le programme en ligne de commande

`tuxscr` prend la ligne de commande de ConvImgCpc : des options lues dans
l'ordre, `-F` convertissant les fichiers nommés avec ce qui a été réglé avant
lui, et écrivant chaque résultat à côté de son image.

| Option | Ce qu'elle fait |
|---|---|
| `-F<fichiers>` | les images à convertir : un fichier ou un motif (`'pics/*.png'`) |
| `-M<mode>` | 0 à 10 : modes 0, 1, 2, EGX1, EGX2, X, split, ASC-UT, ASC0, ASC1, ASC2 |
| `-Z<0\|1>` | écran standard, ou overscan |
| `-U<0\|1>` | palette d'un CPC, ou d'un Plus |
| `-I`, `-S` | garder les proportions : image entière (keep smaller), écran rempli (keep larger) |
| `-T<n>`, `-P<pct>`, `-X` | tramage (0 : aucun), son pourcentage, diffusion d'erreur |
| `-R<pct>`, `-V<pct>`, `-B<pct>` | rouge, vert, bleu |
| `-C<fichier>` | une palette `.PAL` à utiliser |
| `-L` | la palette de la première image sert aux suivantes |
| `-K<n>` | compactage : 0 aucun, 1 standard, 2 ZX0, 3 ZX1, 4 ZX0 second format |
| `-A` | écrire des sources assembleur |

```sh
tuxscr -M0 -F'images/*.png'
tuxscr -M1 -U1 -Z1 -K3 -Fimage.png
tuxscr -help
```

Il est dans l'archive du plugin, sous `bin/` : une fois le plugin installé,
`plugins/tuxscr/bin/tuxscr`.

## Fidélité à ConvImgCpc

La conversion suit ConvImgCpc instruction par instruction (l'ordre dans lequel
il essaie les couleurs décide de l'image). Ce qui ne se retape pas sans risque
n'a pas été retapé : `tools/make_tables.py` recopie de ses sources les routines
Z80 qu'il met dans les écrans, ses trames et ses textes ; `tools/make_asm.py`
transforme en C++, texte pour texte, les fonctions qui écrivent ses sources
assembleur.

Les programmes qu'il écrit sont vérifiés sur le CPC de TuxAPE : chaque
compactage est décompacté par sa propre routine Z80, en mode 0, 1 et 2, écran
standard et overscan, CPC et Plus, et l'image du moniteur est comparée à
l'image convertie ; les animations sont jouées avec chacune de ses trois
méthodes, et l'écran final comparé à la dernière image.

Là où ConvImgCpc a un défaut (un compactage que sa routine ne relit pas, un
fichier qu'il ne relit pas lui-même...), TuxSCR fait ce que la fonction veut
évidemment dire, et l'écart est noté dans [`notes/ecarts.md`](notes/ecarts.md).

## Compiler

TuxSCR suit les conventions des plugins de TuxAPE (`PLUGINS.md`, à la racine
de TuxAPE) et ne prend de TuxAPE que son dossier `sdk/` : `plugin.h` et le
module CMake `TuxAPEPlugin.cmake`. TuxAPE doit se trouver à côté, dans
`../TuxAPE`, ou ailleurs avec `-DTUXAPE_SDK=/chemin/TuxAPE/sdk`. Il lui faut
la version 4 de l'interface des plugins : la langue de TuxAPE.

```sh
cmake --preset dev          # Qt du système, ou celui du .deps/ de TuxAPE
cmake --build build -j
ctest --preset dev
```

L'archive à installer est alors dans `dist/` :
`tuxscr-<version>-<plateforme>.zip`. Elle contient le dossier du plugin :
`plugin.json`, la bibliothèque, `bin/tuxscr` et `LICENSE`, et
`signature.json` quand la clé de l'auteur est nommée :

```sh
cmake -S . -B build -DTUXAPE_SIGN_KEY=$HOME/.config/tuxape-signing/moi.key
cmake --build build
```

Sans clé nommée, l'archive n'est pas signée : compiler TuxSCR ne le signe
jamais au nom de quelqu'un par mégarde.

## Installer

Dans TuxAPE : **Settings ▸ Plugins ▸ Install from ZIP...** (**Réglages ▸
Plug-ins** en français), choisir l'archive, confirmer. Le menu **Plugins**
reçoit « TuxSCR – Screen Converter... ». Installer une archive plus récente
remplace l'ancienne.

## Les langues

TuxSCR parle la langue de TuxAPE : anglais, la langue de ses sources, ou
français. Il n'a plus de bouton à lui pour en changer : TuxAPE redémarre pour
changer la sienne. Le champ `langue` du fichier de réglages de ConvImgCpc est
lu et réécrit tel quel, mais ne choisit plus rien.

Les textes des fenêtres de ConvImgCpc sont gardés par le nom de leur
contrôle (`Main.bpLoad`) : en anglais dans `src/plugin/windowtexts.h`, que
`tools/make_tables.py` recopie du `Multilingue.cs` de ConvImgCpc, et dans les
tables de TuxSCR pour ce qu'il ajoute. Leur traduction, avec celle de tous
les autres textes, est dans `translations/tuxscr_fr.ts`, compilée dans la
bibliothèque et chargée au démarrage du plugin.

Les sources disent les pluriels avec « (s) » (`%n file(s)`) ; l'anglais a ses
formes, « 1 file », « 3 files », dans `translations/tuxscr_en.ts`, qui ne
contient qu'elles.

Après un changement des textes :

```sh
cmake --build build --target tuxscr_update_translations   # lupdate
tools/make_tables.py --french translations/tuxscr_fr.ts    # le français de ConvImgCpc, pour ses textes
linguist translations/tuxscr_fr.ts                         # traduire le reste de ce qui est nouveau
linguist translations/tuxscr_en.ts                         # et les pluriels anglais
cmake --build build
```

Le moteur n'a pas de texte : il rend des données (les notes d'une
animation, `AnimNote`) que les fenêtres disent dans la langue de TuxAPE. Le
programme en ligne de commande écrit en anglais.

## Tests

```sh
ctest --preset dev
```

Les tests du moteur (`engine_*`) vérifient la conversion, les compactages,
les images disque et les split rasters ; `cli_convert` lance le programme ;
`plugin_window` charge le plugin comme TuxAPE le charge, en anglais, en
français et dans une langue qu'il n'a pas ; les tests de l'interface
(`gui_*`) pilotent les fenêtres sans écran, en anglais puis, pour chaque
fenêtre, dans une fenêtre faite pendant que la traduction française est
installée (`tests/french.h`).

## Organisation

Le projet est écrit en C++20. Le moteur ne dépend ni de Qt, ni de TuxAPE, ni
du système, n'écrit rien sur le disque (il rend des fichiers en mémoire) et
n'a aucun texte pour l'utilisateur.

| Dossier | Contenu |
|---|---|
| `src/engine/base` | version, images |
| `src/engine/convert` | l'écran du CPC et sa palette, l'image en encres, tramage, conversion, interruptions de palette |
| `src/engine/formats` | en-têtes AMSDOS, compactages, image disque, écrans, palettes, réglages, animations, sources assembleur |
| `src/engine/splits` | split rasters et table de rasters du Plus, sans leurs fenêtres |
| `src/io` | les images du PC lues avec Qt, sans fenêtre (`tuxscr_io`) : pour les fenêtres et pour le programme |
| `src/plugin` | les fenêtres (`tuxscr_ui`), leurs textes, la classe chargée (`tuxscr`), tout dans `namespace tuxscr` |
| `src/plugin/sprites` | sprites matériels du Plus : éditeur, capture, affichage sur l'image |
| `src/plugin/splits` | éditeur de splits, table de rasters du Plus, générateur de palette |
| `src/cli` | le programme `tuxscr` : le moteur, et les images du PC lues avec Qt ; aucune fenêtre |
| `translations` | la traduction française |
| `tests` | programmes de test |
| `tools` | ce qui recopie les tables et les textes de ConvImgCpc |
| `notes` | écarts voulus avec ConvImgCpc |
| `ConvImgCpc` | le ConvImgCpc d'origine, cloné ici comme référence (non versionné) |

## Licence

ConvImgCpc est l'œuvre de Ludovic Deplanque (Demoniak) ; ses sources sont
publiques mais sans texte de licence. TuxSCR, qui les suit de près, ne doit
pas être distribué sans l'accord de son auteur. Les compacteurs ZX0 et ZX1
sont d'Einar Saukas, sous licence BSD à trois clauses (voir `LICENSE`).
