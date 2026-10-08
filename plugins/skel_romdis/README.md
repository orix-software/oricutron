# Exemple de plugin de type PLG_DEVICE avec modification du signal **romdis**

## Description du plugin

Ce plugin permet l'activation et la désactivation de la mémoire overlay de la même façon qu'avec un lecteur de disquettes.

Son adresse par défaut est $330.

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* reset()

    Active la ROM interne.

* read()

    Utilisée pour la lecture du registre.

* write()

    Utilisée pour l'écriture du registre.


* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'adresse de base du périphérique ainsi le type de mémoire activée.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes

    #define BASE_ADDR 0x0330
    #define END_ADDR 0x0330

    // On ne peut instancier qu'un seul périphérique
    #define INSTANCE_MAX 1

    // Registre pour chaque instance
    Uint8 userdata[INSTANCE_MAX];

    // Copie de userdata[] pour comparaison entre deux appels au moniteur
    // (pour pouvoir afficher les différences)
    Uint8 userdata_old[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "ROMDIS",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                romdis_create,
                NULL,
                romdis_reset,
                romdis_read,
                romdis_write,
                NULL,
                mon_romdis_update,
                mon_romdis_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [skel_romdis]
    load = yes
    plugin = 'plugins/libskel_romdis.so'
    enable = yes

## Utilisation du plugin
Mettre une valeur nulle en $330 pour activer la ROM interne et une valeur non nulle pour activer la RAM Overlay.

Pour activer la RAM overlay:

    sei         ; Interdire les interruptions si la RAM overlay ne contient pas les vecteurs 6502
    lda #$ff
    sta $330

Pour activer la ROM interne:

    lda #$00
    sta $330
    cli         ; Autorise à nouveau les interruptions

>[!CAUTION]
>L'activation et la désactivation ne doivent se faire qu'a partir de la RAM "normale" (ie < $C000) sous peine de plantage du programme.
