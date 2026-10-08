struct PLUGIN {
    char name[PERIPH_NAME_LEN+1];
    Uint16 default_addr;
    Uint16 size;
    Uint16 type;
    SDL_bool  (*addresses)(unsigned int instance, Uint16 offset);

    unsigned int (*create)(struct machine *oric);
    SDL_bool (*shutdown)(struct machine *oric, unsigned int instance);

    SDL_bool (*reset)(struct expansion_bus *oric_bus, unsigned int instance);
    Uint8 (*read)(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool fexec);
    SDL_bool (*write)(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data);

    void (*ticktock)(struct expansion_bus *oric_bus, unsigned int instance, int cycles);

    void (*mon_update)(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid);
    void (*mon_store_state)(struct machine *oric, unsigned int instance);
};

Membres de la structure PLUGIN:

	char name[PERIPH_NAME_LEN+1]:
		nom du périphérique (*)

	Uint16 default_addr:
		adresse de base du périphérique (*)

	Uint16 size:
		nombre d'octets occupés par le périphérique (*)
		doit être >= 1

	Uint16 type:
		type du plugin: PLG_DEVICE, PLG_BANK, PLG_BANK (*)


	SDL_bool  (*addresses)(unsigned int instance, Uint16 offset):
		valide une adresse, utilisé dans le cas où le périphérique
		possède plusieurs adresses non consécutives (doit avoir
		un flag PLG_MULTI)

	unsigned int create(struct machine *oric):
		appelé lors de la création du périphérique (*)
		retourne un numéro d'instance (>=1 ou 0 si erreur)

	SDL_bool shutdown(struct machine *oric, unsigned int instance):
		appelé lors de la destruction du périphérique (shut())
		retourne un booléen indiquant si tout s'est bien passé

	SDL_bool reset(struct expansion_bus *bus, unsigned int instance):
		appelé lors d'un reset de la machine (init_machine() et [F4])
		retourne un booléen indiquant si tout s'est bien passé


	Uint8 read(struct expansion_bus *oric_bus, unsigned int instance, Uint16 offset, SDL_bool fexec):
		appelé lorsqu'un programme lit un octet du périphérique (mon_read(), oric_atmosread())
		retourne l'octet demandé

	SDL_bool write(struct expansion_bus *oric_bus, unsigned int instance, Uint16 offset, Uint8 data):
		appelé lorsqu'un programme écrit un octet vers le périphérique (oric_atmoswrite(),
		retourne DSL_TRUE si ok, SDL_FALSE en cas d'erreur

	ticktock(struct expansion_bus *oric_bus, unsigned int instance, int icycles):
		appelé à chaque début d'une instruction (frameloop_overclock(), frameloop_normal(), steppy_step(), [F2])


	void mon_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid):
		appelé lors de l'affichage de la page du périphérique par le moniteur (mon_update(MSHOW_PERIPH))

	void mon_store_state(struct machine *oric, unsigned int instance):
		appelé à la sortie du moniteur (mon_store_state())


(*): obligatoire

instance: valeur retournée par create()
offset  : offset par rapport à l'adresse de base du périphérique
data    : octet à écrire
icycles : nombre de cycles de l'instruction qui va être exécutée

fexec   : SDL_TRUE si exécution normale, SDL_FALSE si exécution depuis le moniteur

oric    : pointeur vers la structure machine
ptz     : pointeur vers la structure tz de la page du moniteur pour ce périphérique
bus	: pointeur vers la structure expansion_bus

Autres fonctions:
	SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod):
		appelé uniquement au chargement du plugin

