// -----------------------------------------------------------------------------
// PLugin CH376
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

#include "plugin.h"
#include "ch376.h"
#include "plugin_ch376.h"

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
// extern struct textzone *tz[];
// extern struct osdmenu menus[];
// Si Oricutron est compilé sans l'option -rdynamic alors il faut passer la
// référence des fonctions tzprintfpos et tzpoutc au plugin.
#ifndef RDYNAMIC
void (*my_tzprintfpos)( struct textzone *ptz, int x, int y, char *fmt, ... );
void (*my_tzputc)( struct textzone *ptz, char c );
void (*mon_periphmod)( int x, int y, int w, struct textzone *vtz );
#endif

// *****************************************************************************
//                                  Datas
// *****************************************************************************
//
// -----------------------------------------------------------------------------
#define BASE_ADDR 0x340
#define END_ADDR 0x341

#define INSTANCE_MAX 1


static struct ch376 * userdata[INSTANCE_MAX];
static struct ch376 * userdata_old[INSTANCE_MAX];

unsigned int plugin_instances = 0;

static char *description = "CH376";

static char *cmd_to_str(CH376_U8 cmd);
static char *status_to_str(CH376_U8 status);
static char *usb_mode_to_str(CH376_U8 mode);

extern void * system_alloc_mem(int size);
extern void system_free_mem(void *ptr);

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---plugin init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
unsigned int plugin_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    userdata[plugin_instances] = ch376_create(NULL);
    userdata_old[plugin_instances] = system_alloc_mem(sizeof(struct ch376));

    ch376_set_sdcard_drive_path(userdata[plugin_instances], "sdcard/");
    ch376_set_usb_drive_path(userdata[plugin_instances], "usbdrive/");

    return ++plugin_instances;
}

    // -----------------------------------------------------------------------------
    //
    // -----------------------------------------------------------------------------
SDL_bool plugin_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    ch376_destroy(userdata[instance]);

    system_free_mem(userdata_old[instance]);

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool plugin_reset(struct expansion_bus *oric_bus, unsigned int instance)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]

    dbg_printf("CH376 reset(%d)\n", instance);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    ch376_reset(userdata[instance]);

    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                          Lecture de la pile (POP)
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
Uint8 plugin_read(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool run)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    switch (addr)
    {
        // CH376_ORIC_EXTENSION_DATA_PORT
        case 0:
            if (run)
            {
                return ch376_read_data_port(userdata[instance]);
            }
            else
                return (Uint8)(0xff);
        // CH376_ORIC_EXTENSION_COMMAND_PORT
        case 1:
            if (run)
                return ch376_read_command_port(userdata[instance]);
            else
                return (Uint8)(userdata_old[instance]->command);

        default:
            dbg_printf("CH376 READ: bad address $%04x\n", addr);
            return (Uint8) 0;
    }
}

    // -------------------------------------------------------------------------
    //                      Ecriture dasn la pile (PUSH)
    // -------------------------------------------------------------------------
    // run: FALSE -> exécution depuis le moniteur
    //
SDL_bool plugin_write(struct expansion_bus *oric_bus, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    oric_bus = oric_bus; // gcc [-Wunused-parameter]
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    switch (addr)
    {
        // return userdata[instance].data[--userdata[instance].ptr];
        case 0:
            ch376_write_data_port(userdata[instance], data);
            break;

        // CH376_ORIC_EXTENSION_COMMAND_PORT
        case 1:
            ch376_write_command_port(userdata[instance], data);
            break;

        default:
            dbg_printf("CH376 WRITE: bad address $%04x\n", addr);
            return (Uint8) 0;
    }
    return SDL_TRUE;
}

    // -------------------------------------------------------------------------
    //                  Mise à jour de la page du moniteur
    // -------------------------------------------------------------------------
void mon_plugin_update(struct textzone *tz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // int i;

    dbg_printf("CH376: mon update\n");

    my_tzprintfpos( tz, 2, 2,  "Base address : %04X", base_addr);

    // Trait de séparation en ligne 4
    tz->px = 0;
    tz->py = 4;
    my_tzputc( tz, 6 );

    for (int i=0; i < tz->w-2; i++)
//        my_tzputc( tz, 2 );
        my_tzputc( tz, 12 );

    my_tzputc( tz, 8 );

    my_tzprintfpos(tz, 2, 6, "Interface status: %02X", userdata[instance]->interface_status);

    my_tzprintfpos(tz, 2, 7, "Command   : %s", cmd_to_str(userdata[instance]->command));
    my_tzprintfpos(tz, 2, 8, "Cmd status: %s", status_to_str(userdata[instance]->command_status));


    my_tzprintfpos(tz, 2, 10, "USB mode    : %s", usb_mode_to_str(userdata[instance]->usb_mode));
    my_tzprintfpos(tz, 2, 11, "Bytes in cmd:   %02X", userdata[instance]->nb_bytes_in_cmd_data);
    my_tzprintfpos(tz, 2, 12, "Pos rw      :   %02X", userdata[instance]->pos_rw_in_cmd_data);
    my_tzprintfpos(tz, 2, 13, "Bytes to RW : %04X", userdata[instance]->bytes_to_read_write);
    my_tzprintfpos(tz, 2, 14, "Is dir      : %s", (userdata[instance]->current_file_is_directory) ? "True" : "False");

    my_tzprintfpos(tz, 2, 16, "Pattern: %-14s", userdata[instance]->dir_pattern);
    my_tzprintfpos(tz, 2, 17, "SDCard : %-14s", userdata[instance]->sdcard_drive_path);
    my_tzprintfpos(tz, 2, 18, "USB    : %-14s", userdata[instance]->usb_drive_path);

    my_tzprintfpos(tz, 2, 19, "File Pos: %10d", userdata[instance]->current_pos);

    if (oldvalid)
    {
        if (userdata[instance]->interface_status != userdata_old[instance]->interface_status)
            mon_periphmod( 20, 6, 2, tz );

        if (userdata[instance]->command != userdata_old[instance]->command)
            mon_periphmod( 14, 7, strlen(cmd_to_str(userdata[instance]->command)), tz );

        if (userdata[instance]->command_status != userdata_old[instance]->command_status)
            mon_periphmod( 14, 8, strlen(status_to_str(userdata[instance]->command_status)), tz );


        if (userdata[instance]->usb_mode != userdata_old[instance]->usb_mode)
            mon_periphmod( 16, 10, strlen(usb_mode_to_str(userdata[instance]->usb_mode)), tz );

        if (userdata[instance]->nb_bytes_in_cmd_data != userdata_old[instance]->nb_bytes_in_cmd_data)
            mon_periphmod( 18, 11, 2, tz );

        if (userdata[instance]->pos_rw_in_cmd_data != userdata_old[instance]->pos_rw_in_cmd_data)
            mon_periphmod( 18, 12, 2, tz );

        if (userdata[instance]->bytes_to_read_write != userdata_old[instance]->bytes_to_read_write)
            mon_periphmod( 16, 13, 4, tz );

        if (userdata[instance]->current_file_is_directory != userdata_old[instance]->current_file_is_directory)
            mon_periphmod( 16, 14, ( userdata[instance]->current_file_is_directory ? 4 : 5), tz );


        if (strcmp(userdata[instance]->dir_pattern, userdata_old[instance]->dir_pattern))
            mon_periphmod( 11, 16, strlen(userdata[instance]->dir_pattern), tz );

        if (strcmp(userdata[instance]->sdcard_drive_path, userdata_old[instance]->sdcard_drive_path))
            mon_periphmod( 11, 17, strlen(userdata[instance]->sdcard_drive_path), tz );

        if (strcmp(userdata[instance]->usb_drive_path, userdata_old[instance]->usb_drive_path))
            mon_periphmod( 11, 18, strlen(userdata[instance]->usb_drive_path), tz );

        if (userdata[instance]->current_pos != userdata_old[instance]->current_pos)
            mon_periphmod( 12, 19, 10, tz );
    }

}

    // -------------------------------------------------------------------------
    //                      Sauvegarde de l'état
    // -------------------------------------------------------------------------
void mon_plugin_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data+ptr
    memcpy(userdata_old[instance], userdata[instance], sizeof(struct ch376));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
static char *cmd_to_str(CH376_U8 cmd)
{
    switch(cmd)
    {
        case CH376_CMD_NONE:
            return "NONE";

        case CH376_CMD_GET_IC_VER:
            return "IC VER";

        case CH376_CMD_CHECK_EXIST:
            return "CHECK EXIST";

        case CH376_CMD_READ_VAR32:
            return "READ VAR32";

        case CH376_CMD_SET_USB_MODE:
            return "SET USB MDDE";

        case CH376_CMD_GET_STATUS:
            return "GET STATUS";

        case CH376_CMD_RD_USB_DATA0:
            return "RD USB DATA0";

        case CH376_CMD_WR_REQ_DATA:
            return "WR REQ DATA";

        case CH376_CMD_SET_FILE_NAME:
            return "SET FILENAME";

        case CH376_CMD_DISK_MOUNT:
            return "DISK MOUNT";

        case CH376_CMD_FILE_OPEN:
            return "FILE OPEN";

        case CH376_CMD_FILE_ENUM_GO:
            return "FILE ENUM GO";

        case CH376_CMD_FILE_CREATE:
            return "FILE CREATE";

        case CH376_CMD_FILE_ERASE:
            return "FILE CREATE";

        case CH376_CMD_FILE_CLOSE:
            return "FILE CLOSE";

        case CH376_CMD_BYTE_LOCATE:
            return "BYTE LOCATE";

        case CH376_CMD_BYTE_READ:
            return "BYTE READ";

        case CH376_CMD_BYTE_RD_GO:
            return "BYTE RD GO";

        case CH376_CMD_BYTE_WRITE:
            return "BYTE WRITE";

        case CH376_CMD_BYTE_WR_GO:
            return "BYTE WR GO";

        case CH376_CMD_DISK_CAPACITY:
            return "DISK CAPACITY";

        case CH376_CMD_DISK_QUERY:
            return "DISK QUERY";

        case CH376_CMD_DIR_CREATE:
            return "DIR CREATE";

        case CH376_CMD_DISK_RD_GO:
            return "DISK RD GO";

        default:
            return "???";
    }
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
static char *status_to_str(CH376_U8 status)
{
    switch(status)
    {
        case CH376_ERR_OPEN_DIR:
            return "ERR OPEN DIR";

        case CH376_ERR_MISS_FILE:
            return "ERR MISS FILE";

        case CH376_ERR_FOUND_NAME:
            return "ERR FOUND NAME";

        case CH376_RET_SUCCESS:
            return "RET SUCCESS";

        case CH376_RET_ABORT:
            return "RET ABORT";

        case CH376_INT_SUCCESS:
            return "INT SUCCESS";

        case CH376_INT_DISK_READ:
            return "INT DISK READ";

        case CH376_INT_DISK_WRITE:
            return "INT DISK WRITE";

        default:
            return "???";
    }
}

    // -------------------------------------------------------------------------
    //
    // -------------------------------------------------------------------------
static char *usb_mode_to_str(CH376_U8 mode)
{
    switch(mode)
    {
        case CH376_ARG_SET_USB_MODE_INVALID:
            return "INVALID";

        case CH376_ARG_SET_USB_MODE_SD_HOST:
            return "SD HOST";

        case CH376_ARG_SET_USB_MODE_USB_HOST:
            return "USB HOST";

        default:
            return "???";
    }
}
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
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
                NULL,
    };

