# Plugin ch376

## Description du plugin

Ce plugin simule une interface à base de ch376 qui permet d'accèder à une carte mémoire SD ou USB.

Son adresse de base est $341 à $342.

>[!NOTE]
>Cette carte est nécessaire pour le fonctionnement d'Orix.

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* shutdown()
* reset()

    Initialisation de la carte.

* read()

    Lecture des registres du ch376.

* write()

    Écriture des registres du ch376.


* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'état du ch376 ainsi que la dernière commande exécutée.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur


## Constantes

    #define BASE_ADDR 0x340
    #define END_ADDR 0x341

    #define INSTANCE_MAX 1


    static struct ch376 * userdata[INSTANCE_MAX];
    static struct ch376 * userdata_old[INSTANCE_MAX];

## Déclaration du plugin:

    struct PLUGIN plugin = { "CH376",
                    BASE_ADDR, END_ADDR-BASE_ADDR+1,
                    PLG_DEVICE,
                    NULL,
                    plugin_create,
                    plugin_shutdown,
                    plugin_reset,
                    plugin_read,
                    plugin_write,
		    NULL,
                    mon_plugin_update,
                    mon_plugin_store,
        };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [debug]
    load = yes
    plugin = 'plugins/libch376.so'
    enable = yes

>[!NOTE]
>Ce plugin n'autorise qu'une instance.

## Utilisation du plugin

Le port de donnée du ch376 est en $340 et son port de commande en $341.

Se reporter à la documentation du ch376 pour plus de détails.

>[!NOTE]
>Il faut également activer le plugin Twilighte si on veut utiliser Orix.

