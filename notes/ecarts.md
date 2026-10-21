# Écarts avec ConvImgCpc

TuxSCR suit les sources de ConvImgCpc (dépôt IMPact-Scene/ConvImgCpc, état du
1er octobre 2026). Ce fichier dit où il s'en écarte, et pourquoi. Trois sortes
d'écarts : ce que ConvImgCpc fait mal et que TuxSCR fait comme la fonction le
veut évidemment ; ce qui tient à la machine (Windows et GDI+ d'un côté, Qt de
l'autre) ; ce que TuxSCR ajoute.

## L'image

- **Une image est gardée en encres, pas en couleurs.** ConvImgCpc garde
  l'image convertie comme un bitmap de couleurs, et retrouve l'encre de chaque
  point en cherchant sa couleur dans la palette au moment d'écrire un fichier.
  TuxSCR garde l'encre de chaque point. C'est ce qui permet une palette par
  ligne (interruptions de palette), et cela corrige au passage : deux encres
  de la même couleur ne sont plus confondues ; changer la couleur d'une encre
  recolore l'image tout de suite, sans attendre une conversion ; sur un Plus,
  une encre sans couleur (palette incomplète) n'est plus écrite en encre 0.
- **Redimensionnement** : `Graphics.DrawImage` de GDI+ chez ConvImgCpc,
  `QPainter` avec lissage bilinéaire ici. Les images réduites diffèrent de
  quelques niveaux par point ; une image déjà à la taille de l'écran est
  identique.
- **Split rasters en mémoire écran** : les encres au-dessus de 3 (les couleurs
  que donnent les rasters) sont écrites en encre 3, comme le fait
  `CreeBmpCpcForceMode1`. La sauvegarde `.SCR` ordinaire de ConvImgCpc écrit
  dans ce mode des octets où les encres se recouvrent.

## Compactages

- **ZX1** : le portage C# de ConvImgCpc n'enregistre jamais la longueur des
  blocs (`Block.length` reste à zéro) ; ce qu'il écrit n'est pas décompactable.
  TuxSCR écrit du ZX1 juste : sa routine Z80 (celle de ConvImgCpc) le
  décompacte, vérifié sur le CPC de TuxAPE.
- **ZX0Ovs** : ConvImgCpc compacte au premier format de ZX0 mais écrit, pour
  ce compactage, la routine du second format, qui ne relit pas ses données.
  TuxSCR compacte au second format : la routine écrite décompacte l'écran
  (vérifié de même).
- Le tampon de sortie n'est plus limité à 32 Ko.

## Écrans lus

- **Écrans compactés OCP (« MJH »)** : ConvImgCpc lève une exception avant de
  décompacter (copie hors du tampon) ; TuxSCR les lit.
- **Mode 2** : à la relecture d'un `.SCR` en mode 2, ConvImgCpc décale
  l'image d'un point vers la droite ; pas TuxSCR.
- **Animations `.IMP`** : les trois derniers octets sont la largeur, la
  hauteur et le nombre d'images (c'est ce que ConvImgCpc écrit, et Impdraw) ;
  ConvImgCpc les relit dans un autre ordre. TuxSCR relit ce qu'il écrit.
- Un fichier trop court pour être un écran, que ConvImgCpc tente de
  décompacter sans garde, est refusé avec un message.

## Fichiers écrits

- **Sources assembleur** : fins de ligne du système (ConvImgCpc écrit celles
  de Windows) ; première ligne « Généré par TuxSCR » avec sa version.
- **Écran en assembleur avec routine** : le label de la palette est toujours
  écrit quand la routine le nomme, même si on a décoché ce label (ConvImgCpc
  écrit alors une source qui ne s'assemble pas). Sans compactage, les données
  sont celles de l'écran (ConvImgCpc écrit un tampon vide).
- **Image disque `.DSK`** : le nom du créateur dans l'en-tête est « TuxSCR ».
  Une image à deux faces garde sa seconde face (ConvImgCpc ne la réécrit pas
  toujours) ; une piste ajoutée à une image étendue a sa taille dans la table.
- **Palette `.KIT`** : inchangée, y compris l'en-tête AMSDOS qui annonce 30
  octets pour une palette d'image qui en a 32.
- **Jeu de tuiles compacté** : ConvImgCpc écrit le compactage à la suite d'un
  tampon vide de la taille des tuiles ; TuxSCR écrit le compactage seul.
- **GIF animé** : écrit par un encodeur propre (Qt n'en a pas). Chaque image
  a sa durée ; ConvImgCpc donne à toutes la moitié de celle de la première.
- **DiffAnim** : proposé dans « Save » (ConvImgCpc l'a dans sa liste mais ne
  l'atteint jamais) ; les lignes de données ne sont pas coupées par une ligne
  vide qui empêcherait d'assembler. **Tunnel** y est aussi (bouton caché chez
  ConvImgCpc).
- **Bump** : chez ConvImgCpc, le choix « Bump » enregistre selon le cas une
  palette `.KIT` ou les deux écrans EGX (décalage dans la liste des formats) ;
  ici chaque format fait ce que dit son nom.

## Animations (« DeltaPack »)

- Les images pas encore converties le sont avant la sauvegarde (ConvImgCpc
  appelle sa conversion avec un drapeau qui l'empêche de convertir).
- L'adresse de début accepte un nombre décimal entier (ConvImgCpc en perd le
  premier chiffre).
- Modes ASCII, image seule : l'octet en trop que ConvImgCpc compacte après les
  différences n'est pas écrit.
- **Deux zones** : ConvImgCpc oublie ce qui est à l'écran une fois par zone
  sur la première image ; la zone du haut de la deuxième image est alors
  comparée à un écran vide, et ce qui devait redevenir un octet nul reste
  affiché. TuxSCR ne l'oublie qu'une fois.
- **DirectMemory sans délai** : la routine de ConvImgCpc avance de trois
  octets dans la table des images, qui n'en ont que deux sans délai ; dès la
  deuxième image elle lit n'importe quoi. TuxSCR retire le pas en trop.
- Les trois méthodes, avec et sans délai, par colonnes et en deux zones, ont
  été jouées sur le CPC de TuxAPE (par un test qui se servait du cœur de
  TuxAPE, retiré depuis : un plugin n'en prend rien) : l'écran final était la
  dernière image.

## Édition

- Remplissage : remplit ce qui a la couleur du point de départ, comme
  ConvImgCpc, mais sans s'arrêter si l'encre choisie a déjà cette couleur.
- « Keep retouchings » (rejouer les retouches après une conversion) ne rejoue
  pas ce qui a été annulé.
- Retournement gauche-droite : les retouches mémorisées sont retournées avec
  leur largeur (ConvImgCpc les décale d'un point en modes 0 et 1).
- Les barres de défilement de l'image agrandie sont dans l'onglet « Edit ».

## Split rasters

- **Palette de départ** : gardée à part. Chez ConvImgCpc, un premier split au
  retard le plus court de la ligne 0 l'écrase, ce qui change « Palette
  initiale » et les aperçus suivants. La ligne `Palette` du programme est
  écrite comme il l'écrit.
- **Modes des lignes** : suivent l'image à chaque mise à jour, et un
  changement de mode décoché ne reste pas. ConvImgCpc fige le mode à
  l'ouverture et ne dessine rien pour les modes au-dessus de 3.
- Ouvrir l'éditeur ne force plus l'écran principal à 96 × 272 ; ce que l'image
  ne couvre pas est en encre 0.
- Un clic sous la ligne 261 de l'aperçu, un fichier `.xml` incomplet ou qui
  n'en est pas un : bornés ou refusés avec un message (exceptions chez
  ConvImgCpc).
- Sur un Plus, la palette est ramenée aux couleurs du CPC les plus proches.
- Gardé tel quel bien qu'étrange : le programme ne remet pas la palette entre
  deux images (le haut de l'écran a, dès la deuxième, la dernière couleur de
  la précédente) ; ZX0Ovs avec ce programme donne une image brouillée ;
  l'import d'image prend avec une ligne de retard un changement d'une seule
  couleur.
- Le programme écrit a été assemblé avec l'assembleur de TuxAPE et lancé sur
  son CPC : l'image du moniteur est celle de l'aperçu, au point près.

## Table de rasters du Plus et générateur de palette

- « Undo » après un ajout vers le haut efface les lignes ajoutées (ConvImgCpc
  en efface d'autres, ou lève une exception) ; les lignes hors de l'écran sont
  refusées avec un message au lieu d'être ignorées en silence.
- Un fichier `.xml` court est complété par du noir.
- Générateur de palette : la dernière encre est limitée à 15 (ConvImgCpc
  accepte jusqu'à 271 sur une palette de 17) ; pour les rasters, « Get color »
  lit la couleur dans le bon ordre (ConvImgCpc échange le rouge et le bleu),
  et « Generate » avec des nombres inutilisables laisse la fenêtre ouverte
  avec un message.

## Sprites matériels du Plus

Éditeur :

- « All banks » parcourt les huit banques (ConvImgCpc quatre) et les compte
  d'après le dernier sprite dessiné ; son calcul perd, ou lève une exception,
  pour un sprite seul au début d'une banque.
- Enregistrer une autre banque que la première en assembleur marche
  (ConvImgCpc prend le numéro global du sprite et sort du tableau).
- Un label de sprites décoché n'écrit pas de lignes « 01 » orphelines, ni de
  table de pointeurs.
- ZX0Ovs compacte les sprites en ZX0 (exception chez ConvImgCpc).
- Un fichier sans en-tête AMSDOS donne un message. Le `.kit` à côté d'un
  `.spr` est trouvé quelle que soit la casse ; un `.kit` d'image à seize
  couleurs donne à l'encre n sa n-ième couleur (ConvImgCpc les décale d'une).
- L'outil ligne garde sa propre copie du sprite (ConvImgCpc écrase le sprite
  copié).
- Le nombre de banques (`numBanks`), que le code de ConvImgCpc ne lit jamais,
  n'y est pas.
- La barre du sprite courant ne clignote pas ; les couleurs utilisées ont une
  coche plutôt qu'un carré rouge ou vert.

Capture :

- L'aperçu garde des encres ; une zone pas encore capturée donne l'encre 0.
- Au-delà de la huitième banque, les sprites en trop sont refusés avec un
  message (exception chez ConvImgCpc), et la huitième banque est utilisable
  pour les tailles libres.
- Les flèches déplacent le rectangle d'un point de mode 1 ou d'une ligne sans
  déplacer le pointeur de la souris.

Sprites sur l'image :

- **Le zoom 3 est dessiné quatre fois plus grand**, comme le fait l'ASIC et
  comme le fait le test de l'éditeur de ConvImgCpc ; son affichage sur
  l'image dessine trois fois. Il en va de même dans la table de rasters.
- Les valeurs lues d'un `.xml` sont bornées à celles des contrôles.

## Ligne de commande

- `-K3` est ZX1, comme le dit le manuel (dans le code de ConvImgCpc, 3 est
  devenu le second format de ZX0 quand celui-ci a été ajouté) ; `-K4` est ce
  second format.
- `-T<n>` choisit le tramage (ConvImgCpc l'ignore : il affecte un nombre à une
  liste de textes).
- Les noms de fichiers gardent leur casse (ConvImgCpc met ses arguments en
  majuscules), et les fichiers donnés après `-F` sont convertis aussi : un
  shell Unix développe lui-même les motifs.
- Pas de fenêtre : les labels des sources sont ceux que sa fenêtre propose (le
  nom du fichier).

## Ce qui n'y est pas

- « Vérifier mise à jour » (télécharge ConvImgCpc.exe).

## Ce que TuxSCR ajoute

- **Interruptions de palette du CPC Plus** (voir le README) : onglet, marque
  sur l'image, palette par ligne, « Auto », export texte, routine
  d'interruption dans l'écran en assembleur. Une encre redéfinie à une
  ligne recolore ce que l'encre dessine : la conversion donne les encres
  avec la palette de l'image sur toutes les lignes, sauf sous une ligne
  dont « Auto » a cherché les couleurs, convertie avec celles-là. Les interruptions sont
  écrites à la fin du fichier de réglages `.xml`, dans un élément que
  ConvImgCpc ignore.
- **Show in TuxAPE** et **From the CPC Screen**.
- Les réglages sont gardés d'une fois sur l'autre (`TuxSCR.xml` dans le
  dossier que TuxAPE donne au plugin) ; ConvImgCpc relit `ConvImgCpc.xml`
  s'il est à côté de lui, sans jamais l'écrire.
- La langue de départ est l'anglais, comme TuxAPE ; le français de ConvImgCpc
  est à un bouton.
