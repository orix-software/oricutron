# Exemple de plugin simple de type PLG_BANK

## Description du plugin

Ce plugin simule une cartouche de ROM qui remplace la ROM interne de l'Oric
La rom à mettre dans la cartouche est indiquée dans un fichier de configuration: myplugin.cfg

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* reset()

    Lit le fichier de configuration et charge le fichier *.rom* et active le signal **romdis**

* read()

    Est appelée pour toutes les adresses entre $C000 et $FFFF

* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant le nom du fichier *rom*

>[!TIP]
>Si la cartouche est de type RAM, il faudra ajouter une focntion **write()**
>
>On peut également avoir une cartouche avec une partie ROM et une partie RAM, dans ce cas la fonction **write()** devra vérifier l'adresse d'écriture
>afin de ne permetrre celle-ci que dans la plage désirée.

## Constantes

    //Plage d'adresses de la rom: $C000-$FFFF
    #define BASE_ADDR 0xC000
    #define END_ADDR 0xFFFF

    // Une seule cartouche possible
    #define INSTANCE_MAX 1

    // Cartridge memory
    Uint8 rombank[16384];

    // Configuration file
    #define CONFIG_FILE "plugins/myplugin.cfg"

## Déclaration du plugin:

    struct PLUGIN plugin = { "My cartridge",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_BANK,
                    NULL,
                    cartridge_create,
                    NULL,
                    cartridge_reset,
                    cartridge_read,
                    NULL,
                    NULL,
                    mon_cartridge_update,
                    NULL,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [skel_bank]
    load = yes
    plugin = 'plugins/libskel_bank.so'
    enable = yes

## Configuration du plugin
Créer le fichier plugins/myplugin.cfg:

    filename = 'roms/comal'

L'extension *.rom* est automatiquement ajoutée au nom de fichier.

