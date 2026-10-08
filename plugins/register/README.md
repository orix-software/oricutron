# Plugin REGISTER

## Description du plugin

Ce plugin simule un registre hardware de 16 bits avec post incrémentation automatique et montre l'utilisation du paramètre *run* de la fonction read().

Son adresse par défaut est $350 à $352.

>[!NOTE]
>Ce type de périphérique serait plus utile avec une adresse en page 0 pour bénéficier du mode indirect du 6502 et d'un temps d'accès plus rapide.
>Ce périphérique est réalisable en réel

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* reset()

    Mise à zéro du registre et de l'incrément

* read()

    Utilisée pour la lecture du registre et de l'incrément.
    La lecture du registre par le 6502 modifie sa valeur après la lecture de son poids fort.
    La lecture du registre par le moniteur d'Oricutron ne modifie pas sa valeur (commande **m**)

* write()

    Utilisée pour l'écriture du registre et de l'incrément.


* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'adresse de base du périphérique ainsi la valeur du registre et de l'incrément.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes

    #define BASE_ADDR 0x350
    #define END_ADDR 0x352

    #define INSTANCE_MAX 2

    struct REG {
      Uint16 data;
      char incr;
      Uint16 old_data;
      char old_incr;
    };

    struct REG register_data[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "REGISTER",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_DEVICE,
                    NULL,
                    register_create,
                    NULL,
                    register_reset,
                    register_read,
                    register_write,
                    NULL,
                    mon_register_update,
                    mon_register_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [debug]
    load = yes
    plugin = 'plugins/libdregister.so'
    enable = yes
    ; base_addr = $350

>[!NOTE]
>Ce plugin autorise deux instances, ne pas oublier de modifier l'adresse des instances.

## Utilisation du plugin

La lecture du registre ($350) incrémente sa valeur de la valeur de l'incrément ($352).
Le poids faible du registre est en $350.
La valeur de l'incrément est signée et vaut 0 au démarrage.

Incrémenter par pas de 2:

    DOKE #350, 0 : REM valeur initiale du registre: 0
    POKE #352, 2 : REM valeur de l'incrément: 2
    PRINT "Registre: ";DEEK(#350) : REM 2
    PRINT "Registre: ";DEEK(#350) : REM 4

Pour décrémenter par pas de 1:

    DOKE #350, 10 : REM valeur initiale du registre: 10
    POKE #352, 256-1 : REM valeur de l'incrément: -1
    PRINT "Registre: ";DEEK(#350) : REM 9
    PRINT "Registre: ";DEEK(#350) : REM 8

Pour connaitre la valeur de l'incrément:

    I = PEEK(#364)
    IF I > 127 THEN I=I-256
    PRINT "Increment: ";I

>[!WARNING]
>L'instruction DEEK() du BASIC lit d'abord le MSB puis le LSB ce qui explique que la première lecture ne donne pas la valeur initiale et que les lectures suivantes peuvent renvoyer une valeur incorrecte en cas de débordement du registre.
>Oricutron fait la même chose pour les instruction avec un mode indexé ou indirect (BUG, correction disponible)
>Le 6502 lit d'abord le LSB puis le MSB pour les instruction avec un mode indexé ou indirect, dans ce cas la première lecture retourne la valeur initialed uregistre.

>[!NOTE]
>Pour avoir un comportement correct avec le BASIC (et en assembleur sans correction de Oricutron), il faut modifier la fonction read() pour faire l'incrément dans le *case 0* au lieu du *case 1*,
>mais le fonctionnement sera différent en réel.
