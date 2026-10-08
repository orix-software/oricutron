# Plugin Twilighte

## Description du plugin

Ce plugin simule la carte Twilighte qui permet, entre autre, l'utilisation de 32 banques de ROM et 32 banques de RAM.

Il s'agit d'un périphérique composite, son adresse par défaut est $320 à $327 pour le VIA2 et $342 à $343 pour les registres internes de la carte

>[!NOTE]
>Cette carte est nécessaire pour le fonctionnement d'Orix.

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* addresses()

    Nécessaire car la carte possède pludieurs plages d'adresses.

* create()
* reset()

    Initialisation de la carte, activation de la banque ROM n°7.

* read()

    Lecture des registres et de la banque active.

* write()

    Écriture des registres et dans la banque active si il s'agit d'une banque RAM.


* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'état des registres de la carte ainsi que la banque active et son type.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes
    #define BASE_ADDR 0x0320
    #define END_ADDR 0x0343

    // On ne peut instancier qu'un seul périphérique
    #define INSTANCE_MAX 1

    // Taille de la pile
    #define DATA_SIZE 16

    #define CONFIG_FILE "plugins/twilighte.cfg"

    // Structure de données pour une instance du périphérique
    struct BOARD {
        int firmware_version;
        SDL_bool microdisc;                         // Inutilisé pour le moment

        unsigned int bank;

        // 0x0320 - 0x032f
        unsigned char DDRA;
        unsigned char IORAh;

        unsigned char DDRB;
        unsigned char IORB;

        unsigned char T1CL;
        unsigned char TiCH;
        unsigned char T1LL;
        unsigned char TILH;
        unsigned char T2CL;
        unsigned char T2CH;
        unsigned char SR;
        unsigned char ACR;
        unsigned char PCR;
        unsigned char IFR;
        unsigned char IER;
        unsigned char IORA;

        // 0x0342 - 0x0343
        unsigned char t_register;                   // ?xRx xVVV
        unsigned char t_banking_register;
    };

    // Tableau: instances
    struct BOARD userdata[INSTANCE_MAX];

    // Copie de userdata[] pour comparaison entre deux appels au moniteur
    // (pour pouvoir afficher les différences)
    struct BOARD userdata_old[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "Twilghte",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_DEVICE | PLG_MULTI | PLG_BANK,
                    twilighte_addresses,
                    twilighte_create,
                    twilighte_shutdown,
                    twilighte_reset,
                    twilighte_read,
                    twilighte_write,
                    NULL,
                    mon_twilighte_update,
                    mon_twilighte_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [debug]
    load = yes
    plugin = 'plugins/libtwilighte.so'
    enable = yes

>[!NOTE]
>Ce plugin n'autorise qu'une instance.

## Utilisation du plugin

L'activation d'une banque se fait de façon similaire au Telestrat.
Cette carte est prévue pour être utilisée avec le système d'exploitation Orix.

>[!NOTE]
>Il faut également activer le plugin ch376 si on veut utiliser Orix.

