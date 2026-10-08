# Exemple de plugin de type PLG_DEVICE avec prise en compte de l'horloge système

## Description du plugin

Ce plugin simule une horloge simple et permet de voir l'utilisation de la fonction ticktock().

Il utilise trois adresses consécutives de $332 à $334.

## Fonctions utilisées

Cet exemple ne nécessite que les fonctions:
* create()
* reset()

    Initialise l'horloge à 00:00:00

* read()

    Utilisée pour la lecture des registres **heure**, **minutes** et **secondes**.

* write()

	Utilisée pour l'écriture des registes **heure**, **minutes** et **secondes**.
	L'écriture dans le registre des secondes réinitialise les compteurs **count_us** et **count_ms**

* ticktock()

    Cette fonction est appelée avant l'exécution de chaque instruction du 6502.
    Utilisée pour mettre à jour l'heure.
    Décompte les micro secondes et met à jour les registre **heure**, **minutes**, **secondes**.

* mon_update()

    Affiche une page dans le moniteur de Oricutron indiquant l'adresse de base du périphérique ainsi que la valeur des registres utilisateur et internes.

* mon_store()

    Sauvegarde l'état du périphérique entre deux appels du moniteur

>[!NOTE]
>Ce plugin autorise deux instances, ne pas oublier de donner une adresse différente à chaque instance.

## Constantes

	#define BASE_ADDR 0x332
	#define END_ADDR 0x334

	// Possibilité d'avoir deux horloges
	#define INSTANCE_MAX 2

	struct DATA
	{
	    Uint8 seconds;
	    Uint8 minutes;
	    Uint8 hour;

	    int count_us;   /* Registre interne */
	    int count_ms;   /* Registre interne */
	};

	struct DATA userdata[INSTANCE_MAX];
	struct DATA userdata_old[INSTANCE_MAX];

## Déclaration du plugin:

	struct PLUGIN plugin = { "Clock",
		        BASE_ADDR, END_ADDR-BASE_ADDR+1,
		        PLG_DEVICE,
		        NULL,
		        clock_create,
		        NULL,
		        clock_reset,
		        clock_read,
		        clock_write,
		        clock_ticktock,
		        mon_clock_update,
		        mon_clock_store,
	    };

## Activation du plugin

Ajouter les lignes suivantes dans le fichier plugins.cfg:

    [skel_clock]
    load = yes
    plugin = 'plugins/libskel_clock.so'
    enable = yes

## Utilisation du plugin

L'horloge démarre dès l'allumage de l'Oric.

Pour la mettre à l'heure, par exemple à 16:48:00, il suffit de faire en BASIC:

    POKE #332,16
    POKE #333,48
    POKE #334,0

Pour afficher l'heure:

    H = PEEK(#332)
    M = PEEK(#333)
    S = PEEK(#334)
    PRINT "Il est ";H;":";M;":";S

