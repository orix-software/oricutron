# Plugin STACK

## Description du plugin

Ce plugin simule une pile hardware de 16 octets et montre l'utilisation du paramètre *run* de la fonction read().

Son adresse par défaut est $356 à $357.

>[!NOTE]
>Ce type de périphérique serait plus utile avec une adresse en page 0 pour bénéficier du mode indirect du 6502 et d'un temps d'accès plus rapide.
>Ce périphérique est réalisable en réel

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* reset()

    Mise à zéro de la pile et du pointeur de pile

* read()

    Utilisée pour la lecture de la pile et du pointeur de pile.
    La lecture de la pile par le 6502 pré-décrémente le pointeur de pile.
    La lecture de la pile par le moniteur d'Oricutron ne modifie pas le pointeur de pile (commande **m**)

* write()

    Utilisée pour l'écriture dans la pile et dans son pointeur.


* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'adresse de base du périphérique ainsi la pile et son pointeur.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes
    #define BASE_ADDR 0x356
    #define END_ADDR 0x357

    #define STACK_SIZE 16
    #define INSTANCE_MAX 1

    struct STACK {
      Uint8 data[STACK_SIZE];
      Uint8 ptr;
      Uint8 old_data[STACK_SIZE];
      Uint8 old_ptr;
    };

    struct STACK userdata[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "STACK",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_DEVICE,
                    NULL,
                    stack_create,
                    NULL,
                    stack_reset,
                    stack_read,
                    stack_write,
                    NULL,
                    mon_stack_update,
                    mon_stack_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [debug]
    load = yes
    plugin = 'plugins/libstack.so'
    enable = yes
    ; base_addr = $356

>[!NOTE]
>Ce plugin n'autorise qu'une instance.

## Utilisation du plugin

L'écriure dans la pile est l'équivalent d'un PUSH, la lecture d'un PULL.
La valeur du pointeur de pile peut être modifiée et est limitée modulo 16.

### BASIC

    10 REM Empile les valeurs de 1 à 10
    20 FOR I=1 TO 10
    30 POKE #356, I
    40 NEXT
    50 REM Depile les valeurs
    60 PRINT "Pointeur: ";PEEK(#357)
    70 FOR I=1 TO 10
    80 PRINT PEEK(#356)
    90 NEXT
    100 PRINT "Pointeur: ";PEEK(#357)

    RUN
    Pointeur: 10
    10
    9
    8
    7
    6
    5
    4
    3
    2
    1
    Pointeur: 0

### Assembleur

    ; Sauvegarde des registres du 6502
    sta $356    ; pha
    stx $356    ; phx
    sty $356    ; phy
    ....

    ; Restaure les registres du 6502
    ldy $356    ; ply
    ldx $356    ; plx
    lda $356    ; pla

    ; Echange X et Y
    stx $356
    sty $356
    ldx $356
    ldy $356

    ; Équivalent TSX
    ldx $357    ; Avantage par rapport à TSX: on peut utiliser n'importe quel registre

    ; Équivalent TXS
    stx $357    ; Avantage part rappor à TSX: on peut utiliser n'importe quel registre

>[!NOTE]
>Nombre de cycles identique pour un PUSH ou un PULL.
>Nombre de cycles pour un adressage en dehors de la page 0: 4 cycles
>Nombre de cycles pour un adressage en page 0: 3 cycles (1 cycle de moins que pour un PLA)

