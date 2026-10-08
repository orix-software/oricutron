// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(__amigaos4__) || defined(__MORPHOS__)
#include <proto/dos.h>
#include <dos/dostags.h>
#include <proto/amigaguide.h>
#endif

#include "../../system.h"
#include "../../6502.h"
#include "../../via.h"
#include "../../8912.h"
#include "../../gui.h"
#include "../../disk.h"
#include "../../monitor.h"
#include "../../6551.h"


#include "../../machine.h"
#include "../../main.h"

// #include "../../plugin.h"
#include "periph.h"

#include <dlfcn.h>

// #define DEBUG_PLUGIN
#ifdef DEBUG_PLUGIN
    // dbg_printf est une fonction déclarée dans monitor.h mais est spécifique au moniteur
    // #define dbg_printf(x...) { printf(x); }
    #define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
    #define dbg_printf(...)
#endif


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
extern struct textzone *tz[];
extern struct osdmenu menus[];

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#define MAX_PERIPH 20

struct DEVICE {
    char osditem;
    char name[PERIPH_NAME_LEN+1];
    Uint16 addr_start;
    Uint16 addr_end;
    Uint16 type;
    unsigned int instance;
    SDL_bool enable;
    struct PLUGIN *device;
};

struct DEVICE devices_table[MAX_PERIPH];
unsigned int nb_periph = 0;
SDL_bool periph_oldvalid = SDL_FALSE;

void *library[MAX_PERIPH];
unsigned int nb_library = 0;
// void *handle;

struct osdmenuitem *periphitems;

struct plugin_opts
{
  char     lctmp[2048];
  char     plugin[1024];
  char     device[10+1];
  SDL_bool enable;
  SDL_bool load;
  int      base_address;
};


static void machine_reset(struct machine *oric);

// -----------------------------------------------------------------------------
// INTERNAL
// -----------------------------------------------------------------------------
SDL_bool periph_add(struct machine *oric, struct PLUGIN *plugin, char *name, Uint16 addr_start, SDL_bool enable)
{

    if (plugin == NULL)
        return SDL_FALSE;

#if SDL_MAJOR_VERSION == 1
    if (plugin->sdl_event != NULL)
    {
        /*
         * En principe si le plugin utlise des fonctions SDL2 il y aura une erreur
         * lors du chargement de la librairie donc on ne devrait même pas arriver
         * ici.
        */
        fprintf(stderr, "SDL2 requis pour ce plugin\n");
        return SDL_FALSE;
    }
#endif

    if (nb_periph == MAX_PERIPH)
        return SDL_FALSE;

    if (name == NULL)
        name = plugin->name;

    int i = periph_find_by_name(name);
    if (i != nb_periph)
    {
        error_printf("Plugin add: duplicate device '%s' (id = %d, %d)", name, i, nb_periph);
        return SDL_FALSE;
    }

    if (addr_start == 0)
        addr_start = plugin->default_addr;

    // plugin->create peut utiliser oric->type et oric->drivetype
    int instance = plugin->create(oric);

    if (!instance)
        return SDL_FALSE;

    memset(&devices_table[i], 0x00, sizeof(struct DEVICE));

    devices_table[i].osditem = (enable ? 14 : 32);

    strncpy(devices_table[i].name, name, PERIPH_NAME_LEN);

    devices_table[i].instance = instance;

    devices_table[i].device = plugin;

    devices_table[i].addr_start = addr_start;

    devices_table[i].addr_end = devices_table[i].addr_start + devices_table[i].device->size - 1;

    devices_table[i].enable = enable;

    devices_table[i].type = devices_table[i].device->type;

    if ( devices_table[i].enable && (devices_table[i].type & PLG_PRINTER) )
        oric->printenable = SDL_FALSE;

    nb_periph++;

    dbg_printf("Instance %d of %s created\n", instance, plugin->name);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_del(char *name)
{
    return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_enable(char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    devices_table[i].enable = SDL_TRUE;

    return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
// INTERNAL (menu)
// -------------------------------------------------------------------------
SDL_bool periph_enable_by_id(int id, SDL_bool enable)
{
    dbg_printf("periph_enable_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    devices_table[id].enable = enable;

    // Mise à jour du menu
    periphitems[id].name[0] = (enable ? 14 : 32);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_disable(char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    devices_table[i].enable = SDL_FALSE;

    return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
/*
SDL_bool periph_reset_by_name(struct machine *oric, char *name)
{
    int i = periph_find_by_name(name);

    if (i != nb_periph) return SDL_FALSE;

    if (devices_table[i].device->reset != NULL)
        return (devices_table[i].device->reset(oric, devices_table[i].instance));

    else
        return SDL_TRUE;
}
*/

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
SDL_bool periph_reset_by_id(struct expansion_bus *oric_bus, int id)
{
    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    // plugin->reset peut utiliser oric_bus->type et oric_bus->drivetype

    if (devices_table[id].device->reset != NULL)
        return (devices_table[id].device->reset(oric_bus, devices_table[id].instance));

    else
        return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool device_reset_all(struct machine *oric)
{
    int i=0;

    struct expansion_bus oric_bus;
    oric_bus.cpu = &oric->cpu;
    oric_bus.romdis =  &oric->romdis;
    oric_bus.irq =  &oric->cpu.irq;
    oric_bus.reset = SDL_TRUE;
    oric_bus.nmi =  SDL_FALSE;

    oric_bus.type =  oric->type;
    oric_bus.drivetype = oric->drivetype;

    while (i != nb_periph)
    {
        if (devices_table[i].enable && (devices_table[i].device->reset != NULL))
        {
            dbg_printf("PERIPH init: %s\n", devices_table[i].name);

            // plugin->reset peut utiliser oric->type et oric->drivetype
            if ( !devices_table[i].device->reset(&oric_bus, devices_table[i].instance) )
                periph_enable_by_id(i, SDL_FALSE);
        }
        i++;
    }
    // oric->romdis = SDL_TRUE;

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool device_ticktock_all(struct machine *oric, int cycles)
{
    int i=0;
    struct expansion_bus oric_bus;

    oric_bus.cpu = &oric->cpu;
    oric_bus.romdis =  &oric->romdis;
    oric_bus.irq =  &oric->cpu.irq;
    oric_bus.reset = SDL_FALSE;
    oric_bus.nmi =  oric->cpu.nmi;

    oric_bus.type =  oric->type;
    oric_bus.drivetype = oric->drivetype;

    while (i != nb_periph)
    {
        if (devices_table[i].enable && (devices_table[i].device->ticktock != NULL))
        {
            // dbg_printf("PERIPH ticktock: %s\n", devices_table[i].name);

            devices_table[i].device->ticktock(&oric_bus, devices_table[i].instance, cycles);
        }
        i++;
    }

    if (oric_bus.reset)
        machine_reset(oric);

    else if ( (oric_bus.nmi) && (oric_bus.nmi != oric_bus.cpu->nmi) )
        softresetoric(oric, NULL, 0);

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
void shut_periph(struct machine *oric)
{
    dbg_printf("*** Shutdown periph\n");

    if (nb_periph)
    {
        for (int id=0; id<nb_periph; id++)
        {
            free(periphitems[id].name);

            periph_shut_by_id(oric, id);
        }

        free(periphitems);
    }

    for (int i=0; i<nb_library; i++)
        dlclose(library[i]);
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
SDL_bool periph_shut_by_id(struct machine *oric, int id)
{
    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    dbg_printf("Shutdown %s\n", devices_table[id].name);

    if (devices_table[id].device->shutdown != NULL)
        return (devices_table[id].device->shutdown(oric, devices_table[id].instance));

    else
        return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
Uint8 device_read(struct machine *oric, Uint16 addr)
{
    SDL_bool fBank = SDL_FALSE;

    int i=periph_find_by_addr(oric, addr);

    struct expansion_bus oric_bus;
    oric_bus.cpu = &oric->cpu;
    oric_bus.address = addr;
    oric_bus.romdis =  &oric->romdis;
    oric_bus.irq =  &oric->cpu.irq;
    oric_bus.io = ((addr >= 0x300) && (addr <= 0x3ff));
    oric_bus.reset = SDL_FALSE;
    oric_bus.nmi =  oric->cpu.nmi;

    oric_bus.type =  oric->type;
    oric_bus.drivetype = oric->drivetype;

    Uint8 data = 0;

    if (i < nb_periph)
    {
        // dbg_printf("PERIPH READ: %s ($%04x): (from: $%04x)", devices_table[i].name, addr, oric->cpu.lastpc);

        // return devices_table[i].device->read(oric, devices_table[i].instance, addr - devices_table[i].addr_start);

        if (addr >= 0xc000)
        {
            fBank = SDL_TRUE;
            addr = addr - 0xc000;
        }
        else
            addr = addr - devices_table[i].addr_start;

        if (devices_table[i].device->read != NULL)
        {
            data = devices_table[i].device->read(&oric_bus, fBank, devices_table[i].instance, addr, SDL_TRUE);

            if (oric_bus.reset)
                machine_reset(oric);

            else if ( (oric_bus.nmi) && (oric_bus.nmi != oric_bus.cpu->nmi) )
                softresetoric(oric, NULL, 0);
        }

        // dbg_printf(" -> $%02x\n", data);
        return data;
    }
    // ERREUR: pas de périphérique pour l'adresse demandée
    return (Uint8) 0;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool device_write(struct machine *oric, Uint16 addr, Uint8 data)
{
    int i=periph_find_by_addr(oric, addr);
    SDL_bool fbank = (addr >= 0xc000);

    struct expansion_bus oric_bus;
    oric_bus.cpu = &oric->cpu;
    oric_bus.address = addr;
    oric_bus.romdis =  &oric->romdis;
    oric_bus.irq =  &oric->cpu.irq;
    oric_bus.io = ((addr >= 0x300) && (addr <= 0x3ff));
    oric_bus.reset = SDL_FALSE;
    oric_bus.nmi =  oric->cpu.nmi;

    oric_bus.type =  oric->type;
    oric_bus.drivetype = oric->drivetype;

    if (i < nb_periph)
    {
        // dbg_printf("PERIPH WRITE: %s ($%04x): $%02x (from $%04x)\n", devices_table[i].name, addr, data, oric->cpu.lastpc);

        if (fbank)
            addr = addr - 0xc000;
        else
            addr = addr - devices_table[i].addr_start;

        if (devices_table[i].device->write != NULL)
        {
            SDL_bool ret =  devices_table[i].device->write(&oric_bus, fbank, devices_table[i].instance, addr, data);

            if (oric_bus.reset)
                machine_reset(oric);

            else if ( (oric_bus.nmi) && (oric_bus.nmi != oric_bus.cpu->nmi) )
                softresetoric(oric, NULL, 0);

            return ret;
        }
    }

    // Pas de périphérique pour l'adresse demandée
    return SDL_FALSE;
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
int periph_find_by_name(char *name)
{
    int i = 0;

    while ((i < nb_periph) && (strncasecmp(name, devices_table[i].name, PERIPH_NAME_LEN))) i++;

    return i;
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
int periph_find_by_addr(struct machine *oric, Uint16 addr)
{
    int i = 0;

    // Si accès à la rom interne -> fin
    if ((addr >= 0xc000) && (!oric->romdis))
        return nb_periph;

    dbg_printf("periph_find_by_addr(0x%04x)... ", addr);

    // while ((i < nb_periph) && ((devices_table[i].enable == SDL_FALSE) || (addr < devices_table[i].addr_start) || (addr > devices_table[i].addr_end))) i++;

    if (addr >= 0xc000)
        while (
            (i < nb_periph) &&
                (
                    ( devices_table[i].enable == SDL_FALSE ) ||
                    ( !(devices_table[i].type & PLG_BANK) )
                )
            ) i++;
/*
        while (
            (i < nb_periph) &&
                (
                    ( devices_table[i].enable == SDL_FALSE ) ||
                    ( !(devices_table[i].type & PLG_BANK) ) ||
                    ( (devices_table[i].device->addresses == NULL) && ((addr < devices_table[i].addr_start) || (addr > devices_table[i].addr_end)) ) ||
                    ( (devices_table[i].device->addresses != NULL) && !devices_table[i].device->addresses(devices_table[i].instance, addr - devices_table[i].addr_start) )
                )
            ) i++;
*/
    else
        while (
            (i < nb_periph) &&
                (
                    ( devices_table[i].enable == SDL_FALSE) ||
                    ( !(devices_table[i].type & PLG_DEVICE) ) ||
                    ( (devices_table[i].device->addresses == NULL) && ((addr < devices_table[i].addr_start) || (addr > devices_table[i].addr_end)) ) ||
                    ( (devices_table[i].device->addresses != NULL) && !devices_table[i].device->addresses(devices_table[i].instance, addr - devices_table[i].addr_start) )
                )
            ) i++;

/*
        while (
            (i < nb_periph) &&
                (
                    (devices_table[i].enable == SDL_FALSE) ||
                    ( !(devices_table[i].type & PLG_DEVICE) ) ||
                    ( addr < devices_table[i].addr_start ) ||
                    ( (devices_table[i].device->addresses == NULL) && (addr > devices_table[i].addr_end) ) ||
                    ( (devices_table[i].device->addresses != NULL) && !devices_table[i].device->addresses(devices_table[i].instance, addr) )
                )
            ) i++;
*/

    dbg_printf("%d\n", i);

    return i;
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
int periph_find_by_type(Uint16 type)
{
    int i = 0;

    while ( (i < nb_periph) && !(devices_table[i].type & type) ) i++;

    return i;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool device_printer(Uint8 data, SDL_bool rw)
{
    int i = periph_find_by_type(PLG_PRINTER);
    if ( i >= nb_periph)
        return SDL_FALSE;

    if (rw)
    {
        // Read
        /*
        error_printf("/// printer -> %02X", data);
        */
    }
    else
    {
        // Write
        /*
        error_printf("/// %02X -> printer", data);
        */
        devices_table[i].device->write(NULL, SDL_FALSE, devices_table[i].instance, 0, data);
    }

    return SDL_TRUE;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool device_present(struct machine *oric, Uint16 addr)
{
    return (periph_find_by_addr(oric, addr) != nb_periph);
}

// -------------------------------------------------------------------------
// INTERNAL (menu)
// -------------------------------------------------------------------------
SDL_bool device_enabled_by_id(int id)
{
    dbg_printf("device_enabled_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    return (devices_table[id].enable);
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
void device_list()
{
    if (nb_periph == 0)
    {
        dbg_printf("Periph: empty list\n");
    }
    else
    {
        int i=0;

        while(i < nb_periph)
            periph_display(i++);
    }
}

// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
void periph_display(int i)
{
    if ( (i < 0) || (i >= nb_periph) )
    {
        error_printf("Periph out of range: %d", i);
    }
    else
    {
        error_printf("Periph name     : %s", devices_table[i].name);
        error_printf("Periph instance : %d", devices_table[i].instance);
        error_printf("Periph addresses: [%04X, %04X]", devices_table[i].addr_start, devices_table[i].addr_end);
        error_printf("Periph enable   : %s", (devices_table[i].enable ? "yes" : "no"));
        error_printf("Periph type     : 0x%02X", devices_table[i].type);
        error_printf("");
    }
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
// Set the title of a textzone
void my_tzsettitle( struct textzone *ptz, char *title )
{
    int ox, oy;
    // makebox( ptz, 0, 0, ptz->w, ptz->h, menufc(SDL_FALSE), menubc(SDL_FALSE));
    makebox( ptz, 0, 0, ptz->w, ptz->h, 2, 3);
    if( !title ) return;

    // tzsetcol( ptz, menufc(SDL_FALSE), menubc(SDL_FALSE));
    tzsetcol( ptz, 2, 3);
    ox = ptz->px;
    oy = ptz->py;
    ptz->px = 3;
    ptz->py = 0;
    tzstr( ptz, "[ " );
    tzstr( ptz, title );
    tzstr( ptz, " ]" );
    ptz->px = ox;
    ptz->py = oy;
}

// -------------------------------------------------------------------------
// INTERNAL (plugin)
// -------------------------------------------------------------------------
// Copie de mon_viamod
void mon_periphmod( int x, int y, int w, struct textzone *vtz )
{
    int offs, i;

    offs = y*vtz->w+x;
    for( i=0; i<w; i++, offs++ )
    {
        vtz->fc[offs] = 1;
        vtz->bc[offs] = 8;
    }
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool mon_device_enabled_by_id(int id)
{
    dbg_printf("mon_device_enabled_by_id(%d)\n", id);

    if ( (id < 0) || (id >= nb_periph) )
        return SDL_FALSE;

    // Si on veut ne prendre en compte que les extensions qui ont une page pour
    // le moniteur.
    // return ( devices_table[id].enable && (devices_table[id].mon_update != NULL) );

    // Sinon
    return ( devices_table[id].enable );
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
Uint8 device_mon_read(struct machine *oric, Uint16 addr)
{
    int i=periph_find_by_addr(oric, addr);

    struct expansion_bus oric_bus;
    oric_bus.cpu = &oric->cpu;
    oric_bus.address = addr;
    oric_bus.romdis =  &oric->romdis;
    oric_bus.irq =  &oric->cpu.irq;
    oric_bus.io = ((addr >= 0x300) && (addr <= 0x3ff));
    oric_bus.reset = SDL_FALSE;
    oric_bus.nmi =  oric->cpu.nmi;

    oric_bus.type =  oric->type;
    oric_bus.drivetype = oric->drivetype;

    if (i < nb_periph)
    {
        Uint8 data = 0;

        dbg_printf("PERIPH MON READ: %s ($%04x): (from: $%04x)", devices_table[i].name, addr, oric->cpu.lastpc);
        // return devices_table[i].device->read(oric, devices_table[i].instance, addr - devices_table[i].addr_start);

        if (devices_table[i].device->read != NULL)
        {
            if (addr >= 0xc000)
                addr = addr - 0xc000;
            else
                addr = addr - devices_table[i].addr_start;

            data = devices_table[i].device->read(&oric_bus, (addr >= 0xc000), devices_table[i].instance, addr, SDL_FALSE);

            if (oric_bus.reset)
                machine_reset(oric);

            else if ( (oric_bus.nmi) && (oric_bus.nmi != oric_bus.cpu->nmi) )
                softresetoric(oric, NULL, 0);

            dbg_printf(" -> $%02x\n", data);
        }
        else
            dbg_printf("WRITE ONLY\n");

        return data;
    }
    // ERREUR: pas de périphérique pour l'adresse demandée
    return (Uint8) 0;
}

// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
// void clear_textzone( struct machine *oric, int i );

void mon_update_periph( struct machine *oric, int id )
{
    struct textzone *ptz = tz[TZ_PERIPH];

    dbg_printf("*** MON_UPDATE_PERIPH: view = %d\n", id);

    // dbg_printf("W=%d, H=%d, X=%d, Y=%d\n", ptz->w, ptz->h, ptz->x, ptz->y);
    /*
    if (view == 0)
    {
        my_tzsettitle(ptz, "Periph List");
        clear_textzone(oric, TZ_PERIPH);

        // Ligne: 0 -> titre
        //        1->19 texte
        //        20: cadre bas
        // Colonne: 1 -> 28

        for (int i=0; i<16; i++)
        {
            tzsetcol(ptz, i, 3);
            tzprintfpos(ptz, 2, i+1, "Couleur: %d", i);
    }
    tzstrpos(ptz, 1, 18, "123456789.123456789.12345678");

    return;
    */


    if (nb_periph == 0)
    {
        tzprintfpos( ptz, 2, 2,  "No extensions");
        return;

    }
    /*
    else
    {
        int i=0;

        while(i < nb_periph)
        {
            tzprintfpos(ptz, 2, i*4+1, "Name     : %s\n", devices_table[i].name);
            tzprintfpos(ptz, 2, i*4+2, "Addresses: %04X...%04X\n", devices_table[i].addr_start, devices_table[i].addr_end);
            tzprintfpos(ptz, 2, i*4+3, "Enable   : %s\n", (devices_table[i].enable ? "yes" : "no"));
            i++;
        }
    }

    return;
    }
    */
    // int view = periph_find_by_name("STACK");

    if (id == nb_periph) return;

    if (!devices_table[id].enable) return;


    my_tzsettitle(ptz, devices_table[id].name);
    clear_textzone(oric, TZ_PERIPH);

    if (devices_table[id].device->mon_update == NULL)
    {
        dbg_printf("PERIPH: mon_update(%d) == NULL", id);

        tzprintfpos(ptz, 2, 2, "Name     : %s\n", devices_table[id].name);
        tzprintfpos(ptz, 2, 3, "Addresses: $%04X -> $%04X\n", devices_table[id].addr_start, devices_table[id].addr_end);
        tzprintfpos(ptz, 2, 4, "Enable   : %s\n", (devices_table[id].enable ? "yes" : "no"));
        tzprintfpos(ptz, 2, 5, "Type     : $%02X\n", devices_table[id].type);

        // Trait de séparation en ligne 6
        ptz->px = 0;
        ptz->py = 6;
        tzputc( ptz, 6 );

        for (int i=0; i<ptz->w-2; i++)
            tzputc( ptz, 2 );

        tzputc( ptz, 8 );

        return;
    }

    // my_tzsettitle(ptz, devices_table[id].name);
    // clear_textzone(oric, TZ_PERIPH);

    devices_table[id].device->mon_update(ptz, devices_table[id].instance, devices_table[id].addr_start, periph_oldvalid);


}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
int mon_device_count()
{
    return nb_periph;
}


// -------------------------------------------------------------------------
//
// -------------------------------------------------------------------------
void mon_store_state_periph(struct machine *oric, SDL_bool oldvalid)
{
    dbg_printf("mon_store_state_periph(%d)\n", oldvalid);

    for (int id=0; id < nb_periph; id++)
    {
        if (devices_table[id].device->mon_store_state != NULL)
            devices_table[id].device->mon_store_state(oric, devices_table[id].instance);
    }
    periph_oldvalid = oldvalid;
}


// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
void mon_device_oldvalid(SDL_bool oldvalid)
{
    dbg_printf("mon_device_oldvalid(%d)\n", oldvalid);

    periph_oldvalid = oldvalid;
}

// -------------------------------------------------------------------------
// INTERNAL (menu)
// -------------------------------------------------------------------------
// Toggle extension on/off
void toggleperiph( struct machine *oric, struct osdmenuitem *mitem, int id )
{
    struct expansion_bus oric_bus;
    oric_bus.cpu = &oric->cpu;
    oric_bus.romdis =  &oric->romdis;
    oric_bus.irq =  &oric->cpu.irq;
    oric_bus.reset = SDL_FALSE;
    oric_bus.nmi =  SDL_FALSE;

    oric_bus.type =  oric->type;
    oric_bus.drivetype = oric->drivetype;

    if( device_enabled_by_id(id) )
    {
        periph_enable_by_id(id, SDL_FALSE);

        // Mise à jour du menu OSD
        mitem->name[0] = 32;

        return;
    }

    // Le périphérique était désactivé, on l'initialise...
    // À voir si on conserve l'initialisation dans ce cas
    if (periph_reset_by_id(&oric_bus, id))
    {
        // .. et on l'ective
        periph_enable_by_id(id, SDL_TRUE);

        // Mise à jour du menu OSD
        mitem->name[0] = 14;

        if (oric_bus.reset)
            machine_reset(oric);

        else if (oric_bus.nmi)
            softresetoric(oric, NULL, 0);
    }
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
#if SDL_MAJOR_VERSION == 1
void device_sdl_event(SDL_Event *event)
{
    fprintf(stderr, "device_sdl_event, event type: %d\n", event->type);
}

#else
void EventDecode(SDL_Event *pEvent)
{
    switch (pEvent->type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            fprintf(stderr, "KEYDOWN/KEYUP: windowID = %d, state = %d, repeat = %d, scancode = %d, keysym = %d, mod = %d",
            pEvent->key.windowID,
            pEvent->key.state,
            pEvent->key.repeat,
            pEvent->key.keysym.scancode,
            pEvent->key.keysym.sym,
            pEvent->key.keysym.mod
            );
            break;

        case SDL_MOUSEMOTION:
            fprintf(stderr, "MOUSEMOTION: windowID = %d, which = %d, state = %d, x= %d, y= %d, xrel = %d, yrel = %d",
            pEvent->motion.windowID,
            pEvent->motion.which,
            pEvent->motion.state,
            pEvent->motion.x,
            pEvent->motion.y,
            pEvent->motion.xrel,
            pEvent->motion.yrel
            );
            break;

        case SDL_MOUSEWHEEL:
            // fprintf(stderr, "MOUSEWHEEL: windowID = %d, which = %d, x= %d, y= %d, direction = %d, preciseX = %d, preciseY = %d, mouseX = %d, mouseY = %d",
            fprintf(stderr, "MOUSEWHEEL: windowID = %d, which = %d, x= %d, y= %d, direction = %d",
            pEvent->wheel.windowID,
            pEvent->wheel.which,
            pEvent->wheel.x,
            pEvent->wheel.y,
            pEvent->wheel.direction
            /*
            pEvent->wheel.preciseX,
            pEvent->wheel.preciseY,
            pEvent->wheel.mouseX,
            pEvent->wheel.mouseY
            */
            );
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            fprintf(stderr, "MOUSEBUTTONDOWN/UP: windowID = %d, which = %d, button = %d, state = %d, clicks = %d, x= %d, y= %d",
            pEvent->button.windowID,
            pEvent->button.which,
            pEvent->button.button,
            pEvent->button.state,
            pEvent->button.clicks,
            pEvent->button.x,
            pEvent->button.y
            );
            break;

        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:

        case SDL_QUIT:
            fprintf(stderr, "QUIT");
            break;

        case SDL_WINDOWEVENT:
        {
            switch (pEvent->window.event)
            {
                case SDL_WINDOWEVENT_NONE:           /**< Never used */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_NONE");
                    break;

                case SDL_WINDOWEVENT_SHOWN:          /**< Window has been shown */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_SHOWN");
                    break;

                case SDL_WINDOWEVENT_HIDDEN:         /**< Window has been hidden */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_HIDDEN");
                    break;

                case SDL_WINDOWEVENT_EXPOSED:        /**< Window has been exposed and should be
                                                 redrawn */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_EXPOSED");
                    break;

                case SDL_WINDOWEVENT_MOVED:          /**< Window has been moved to data1, data2
                                             */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_MOVED");
                    break;

                case SDL_WINDOWEVENT_RESIZED:        /**< Window has been resized to data1xdata2 */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_RESIZED");
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:   /**< The window size has changed, either as
                                                 a result of an API call or through the
                                                 system or user changing the window size. */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_SIZE_CHANGED");
                    break;

                case SDL_WINDOWEVENT_MINIMIZED:      /**< Window has been minimized */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_MINIMIZED");
                    break;

                case SDL_WINDOWEVENT_MAXIMIZED:      /**< Window has been maximized */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_MAXIMIZED");
                    break;

                case SDL_WINDOWEVENT_RESTORED:       /**< Window has been restored to normal size
                                                 and position */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_RESTORED");
                    break;

                case SDL_WINDOWEVENT_ENTER:          /**< Window has gained mouse focus */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_ENTER");
                    break;

                case SDL_WINDOWEVENT_LEAVE:          /**< Window has lost mouse focus */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_LEAVE");
                    break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:   /**< Window has gained keyboard focus */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_FOCUS_GAINED");
                    break;

                case SDL_WINDOWEVENT_FOCUS_LOST:     /**< Window has lost keyboard focus */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_FOCUS_LOST");
                    break;

                case SDL_WINDOWEVENT_CLOSE:          /**< The window manager requests that the window be closed */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_CLOSE");
                    break;

                case SDL_WINDOWEVENT_TAKE_FOCUS:     /**< Window is being offered a focus (should SetWindowInputFocus() on itself or a subwindow, or ignore) */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_TAKE_FOCUS");
                    break;

                case SDL_WINDOWEVENT_HIT_TEST:       /**< Window had a hit test that wasn't SDL_HITTEST_NORMAL. */
                    fprintf(stderr, "eventID = SDL_WINDOWEVENT_HIT_TEST");
                    break;

                default:
                    fprintf(stderr, "eventID = %d", pEvent->window.event);
            }
            break;
        }

        default:
            fprintf(stderr, "event.type = %d", pEvent->type);
    }
}

void device_sdl_event(SDL_Event *event)
{
    int i = 0;

    while ( (i < nb_periph) &&
            ( (devices_table[i].device->sdl_event == NULL) ||
                ((devices_table[i].device->sdl_event != NULL) &&
                !devices_table[i].device->sdl_event(event))
            )
          ) i++;
/*
    for (i=0; i<nb_periph; i++)
    {
        if (devices_table[i].device->sdl_event != NULL)
            devices_table[i].device->sdl_event(event);
    }
*/
/*
    if (event->type == SDL_COMPAT_ACTIVEEVENT)
    {
        fprintf(stderr, "Device WindowID = %d ", event->window.windowID);
        EventDecode(&event->window);
        fprintf(stderr, "\n");
    }
    else
    {
        fprintf(stderr, "Event type = %d\n", event->type);
    }
*/
}
#endif

// *****************************************************************************
//                      Déclaratoin des extensions
// INTERNAL
// *****************************************************************************
struct PLUGIN * load_plugin(char *library_name)
{
    SDL_bool (*plugin_init)(void *tzprintfpos, void *tzputc, void *mon_periphmod);
    void *handle;

    if (library == NULL)
        return NULL;

    dbg_printf("load_plugin: %s\n", library_name);

    // if ( (handle = dlopen("/home/hcl/devel/GIT-WC/oricutron-hcl/plugins/stack/libstack.so", RTLD_NOW)) == NULL)
    if ( (handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL)) == NULL)
    {
        error_printf("=== ERREUR DE CHARGEMENT DE LA DLL: %s\n", dlerror());
        return NULL;
    }

    struct PLUGIN *plugin = dlsym(handle, "plugin");
    if (dlerror() != NULL)
    {
        error_printf("=== ERREUR DE CHARGEMENT DE LA DLL: symbol 'plugin'\n", dlerror());
        dlclose(handle);
        return NULL;
    }

    plugin_init = dlsym(handle, "plugin_init");
    if (dlerror() != NULL)
    {
        error_printf("=== ERREUR DE CHARGEMENT DE LA DLL: symbol 'plugin_init'%s\n", dlerror());
        dlclose(handle);
        return NULL;
    }

    library[nb_library++] = handle;

    if (!plugin_init(tzprintfpos, tzputc, mon_periphmod))
        return NULL;

    return plugin;
}

// -------------------------------------------------------------------------
// USED
// -------------------------------------------------------------------------
SDL_bool device_test(struct machine *oric)
{
    // struct PLUGIN *plugin;

    if (!nb_periph)
    {
        load_devices_config(oric);

        // Création du menu OSD
        periphitems = calloc(nb_periph+3, sizeof(struct osdmenuitem));

        if (periphitems == NULL)
        {
            error_printf("*** ERROR CALLOC\n");
            return SDL_FALSE;
        }

        for (int i=0; i<nb_periph; i++)
        {
            dbg_printf("Initialisation %d\n", i);

            memset(&periphitems[i], 0x00, sizeof(struct osdmenuitem));

            // periphitems[i].name = strndup(&devices_table[i].osditem, PERIPH_NAME_LEN+1);
            periphitems[i].name = malloc(PERIPH_NAME_LEN+11);
            if (periphitems[i].name)
                sprintf(periphitems[i].name, "%c%-*s    $%04X", (devices_table[i].enable ? 14 : 32), PERIPH_NAME_LEN, devices_table[i].name, devices_table[i].addr_start);

            else
            {
                error_printf("*** MALLOC ERROR ***\n");
                return SDL_FALSE;
            }

            periphitems[i].func = toggleperiph;
            periphitems[i].arg = i;
        }

        // AJout de l'option pour retour vers le menu principal
        periphitems[nb_periph].name = OSDMENUBAR;

        periphitems[nb_periph+1].name   = "Back";
        periphitems[nb_periph+1].key    = "\x17";
        periphitems[nb_periph+1].sdlkey = SDLK_BACKSPACE;
        periphitems[nb_periph+1].func   = gotomenu;
        periphitems[nb_periph+1].arg    = 1;            // Menu Hardware

        // Intégration du menu dans le menu principal
        // 8: indice du menu dans le tableau menus[] de gui.c
        menus[8].items = periphitems;
    }

#ifdef __OPENGL_AVAILABLE__
#if SDL_MAJOR_VERSION == 1
#else
    // Ré-active la fenêtre principale au cas où...
    SDL_COMPAT_MakeCurrent(NULL, NULL);
    SDL_COMPAT_RaiseWindow(NULL);
#endif
#endif
    return SDL_TRUE;
}


// -------------------------------------------------------------------------
// INTERNAL
// -------------------------------------------------------------------------
SDL_bool load_devices_config(struct machine *oric)
{
    struct PLUGIN *plugin;

    struct plugin_opts *sto;
    sto = malloc(sizeof(struct plugin_opts));

    if (!sto) return SDL_FALSE;

    FILE *f;
    Sint32 i;
    char config_path[4096];

    char *device;

    strcpy(config_path, "plugins.cfg");
    add_fileprefix(config_path, 4096);
    dbg_printf("Open plugins file: %s\n", config_path);

    f = fopen(config_path, "r");
    if (!f) return SDL_FALSE;

    while ( !feof(f) )
    {
        if (!fgets(sto->lctmp, 2048, f)) break;

        for (i=0; isws(sto->lctmp[i]); i++);

        dbg_printf("[1]: %s", sto->lctmp);

        while (sto->lctmp[i] == '[')
        {
            // On a un début de bloc

            i++;
            for (; isws(sto->lctmp[i]); i++);

            dbg_printf("Found paragraph: %s", sto->lctmp+i);

            sto->enable = SDL_FALSE;
            sto->load = SDL_FALSE;
            sto->plugin[0] = '\0';
            sto->device[0] = '\0';
            sto->base_address = 0;

            do
            {
                if (fgets(sto->lctmp, 2048, f))
                {
                    for (i=0; isws(sto->lctmp[i]); i++);

                    if ((sto->lctmp[i] != '\n') && (sto->lctmp[i] != ';'))
                    {
                        for (i=0; isws(sto->lctmp[i]); i++);

                        if (read_config_bool(&sto->lctmp[i]  , "enable"      , &sto->enable)) continue;
                        if (read_config_bool(&sto->lctmp[i]  , "load"        , &sto->load)) continue;
                        if (read_config_path(&sto->lctmp[i]  , "plugin"      , sto->plugin, 1024)) continue;
                        if (read_config_string(&sto->lctmp[i], "device"      , sto->device, 10+1)) continue;
                        if (read_config_int(&sto->lctmp[i]   , "base_address", &sto->base_address, 0x0300-1, 0x03ff+1)) continue;

                        // Si on arrive ici, on est soit au début d'un nouveau paragraphe, soit avec une option inconnue
                        if (sto->lctmp[0] != '[')
                            dbg_printf("\t\t[3]%s", sto->lctmp+i);
                    }
                }
            } while (!feof(f) && sto->lctmp[i] != '[');

            dbg_printf("\t[2]: loop\n");

            dbg_printf("device: %s\n", sto->device);
            dbg_printf("plugin: %s\n", sto->plugin);
            dbg_printf("load: %s\n", (sto->load ? "yes" : "no"));
            dbg_printf("enable: %s\n", (sto->enable ? "yes" : "no"));
            dbg_printf("base address: 0x%x\n", sto->base_address);

            if (sto->load && (sto->plugin[0] != '\0') && (((sto->base_address >= 0x300) && (sto->base_address <= 0x3ff)) || sto->base_address == 0))
            {
                plugin=load_plugin(sto->plugin);
                if (plugin != NULL)
                {
                    device =  (sto->device[0] != '\0' ? sto->device : NULL);

                    if (!periph_add(oric, plugin, device, sto->base_address, sto->enable))
                        error_printf("load_device_config: erreur lors de l'ajout du périphérique: %s (%s)", sto->device, sto->plugin);
                }
            }
        }
        dbg_printf("[1]: loop\n");
    }
    free(sto);
    fclose(f);

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
// INTERNAL
// -----------------------------------------------------------------------------
static void machine_reset(struct machine *oric)
{
    #ifndef WWW_NO_MONITOR
      mon_state_reset( oric );
    #endif
    if( !init_machine( oric, oric->type, SDL_FALSE ) )
    {
        shut( oric );
    #ifdef __ANDROID__
        error_printf("'init_machine' failed");
    #endif
        exit( EXIT_FAILURE );
    }
}
