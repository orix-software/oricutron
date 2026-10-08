# Plugin DEBUG

## Description du plugin

Ce plugin permet d'activer l'affichage des registres du 6502 pour chaque instructions du 6502 (similaire au mode DEBUG de Oricutron)

Son adresse par défaut est $331.

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* reset()

    Désactive le mode trace.

* read()

    Utilisée pour la lecture du registre.

* write()

    Utilisée pour l'écriture du registre.

* ticktock()

    Utilisée pour afficher l'état des registres du 6502 après chaque instruction.

* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'adresse de base du périphérique ainsi la valeur du registre d'activation.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes

    #define BASE_ADDR 0x331
    #define END_ADDR 0x331

    #define INSTANCE_MAX 1

    Uint8 userdata[INSTANCE_MAX];
    Uint8 userdata_old[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "DEBUG",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_DEVICE,
                    NULL,
                    debug_create,
                    NULL,
                    debug_reset,
                    debug_read,
                    debug_write,
                    debug_ticktock,
                    mon_debug_update,
                    mon_debug_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [debug]
    load = yes
    plugin = 'plugins/libdebug.so'
    enable = yes

## Utilisation du plugin
Mettre une valeur nulle en $331 pour désactiver le mode trace et une valeur non nulle pour l'activer.

Pour activer le mode trace:

    POKE #331,1

Pour désactiver le mode trace:

    POKE #331,0

Pour connaitre l'état du mode trace:

    T = PEEK(#331)
    IF T PRINT "Mode trace actif" ELSE PRINT "Mode trace inactif"

