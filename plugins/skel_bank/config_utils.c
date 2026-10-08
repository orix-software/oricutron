// vim: tabstop=4 expandtab
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

// Pour les fonction de lecture du fichier de configuration
// #include "../../main.h"

#include "plugin.h"

// Configuration file
#include "config_utils.h"
// #define CONFIG_FILE "plugins/myplugin.cfg"


// #define DEBUG_PLUGIN
#ifdef DEBUG_PLUGIN
    // dbg_printf est une fonction déclarée dans monitor.h mais est spécifique au moniteur
    // #define dbg_printf(x...) { printf(x); }
    #define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
    #define dbg_printf(...)
#endif

// From monitor.c

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool isws( char c )
{
  if( ( c == 9 ) || ( c == 32 ) ) return SDL_TRUE;
  return SDL_FALSE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool isnum( char c )
{
  if( ( c >= '0' ) && ( c <= '9' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
int hexit( char c )
{
  if( isnum( c ) ) return c-'0';
  if( ( c >= 'a' ) && ( c <= 'f' ) ) return c-('a'-10);
  if( ( c >= 'A' ) && ( c <= 'F' ) ) return c-('A'-10);
  return -1;
}


// From main.C

// -----------------------------------------------------------------------------
// Print a formatted string into a textzone
// -----------------------------------------------------------------------------
#ifndef __ANDROID__
void error_printf( char *fmt, ... )
{
  static char str[256];  // Stupid MinGW32 not having vasprintf...

  va_list ap;
  va_start( ap, fmt );
  if( vsnprintf( str, 256, fmt, ap ) != -1 )
  {
    str[255] = 0;
#ifdef WIN32
    MessageBoxA( NULL, str, "Oricutron", MB_OK );
#else
    fprintf( stderr, "%s\n", str );
#endif
  }
  va_end( ap );
}
#endif

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool read_config_int( char *buf, char *token, int *dest, int min, int max )
{
  Sint32 i, toklen;
  int val, hv;

  // Get the token length
  toklen = (int)strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;

  val = 0;
  if( buf[i] == '$' )
  {
    i++;
    while( (hv=hexit(buf[i])) != -1 )
    {
      val = (val<<4) + hv;
      i++;
    }
  } else {
    val = atoi( &buf[i] );
  }

  if( val < min ) val = min;
  if( val > max ) val = max;

  (*dest) = val;
  return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool read_config_bool( char *buf, char *token, SDL_bool *dest )
{
  Sint32 i, toklen;

  // Get the token length
  toklen = (int)strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace, single quote
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;

  (*dest) = SDL_FALSE;
  if( strncasecmp( &buf[i], "true", 4 ) == 0 ) (*dest) = SDL_TRUE;
  if( strncasecmp( &buf[i], "yes", 3 ) == 0 )  (*dest) = SDL_TRUE;
  return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen )
{
  Sint32 i, toklen, d;

  // Get the token length
  toklen = (int)strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace, single quote
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '\'' ) return SDL_TRUE;
  i++;

  // Copy and un-escape the string
  d=0;
  while( buf[i] != '\'' )
  {
    if( d >= (maxlen-1) ) break;
    if( !buf[i] ) break;

    if( ( buf[i] == '\\' ) && ( buf[i+1] == '\'' ) )
    {
      dest[d++] = '\'';
      i+=2;
      continue;
    }

    if( ( buf[i] == '\\' ) && ( buf[i+1] == '\\' ) )
    {
      dest[d++] = '\\';
      i+=2;
      continue;
    }

    dest[d++] = buf[i++];
  }

  dest[d] = 0;
  return SDL_TRUE;
}

// Specific cartridge.c

// -------------------------------------------------------------------------
//                              Load config & banks
// -------------------------------------------------------------------------
SDL_bool config_load(char *config_file, Uint8 *bank, char *bank_name)
{
    FILE* f;
    char line[1024];
    char bankfile[1024];

    SDL_bool retval = SDL_FALSE;

    f = fopen(config_file, "r");
    if (!f)
    {
        error_printf("%s not found\n", config_file);
        return SDL_FALSE;
    }

    while (!feof(f))
    {
        if ( fgets(line, 1024, f) != NULL)
        {

            if (read_config_string(line, "filename", bankfile, 1024))
            {
                // Chargement de la banque
                dbg_printf("Loading bank %s...", bankfile);

                if (!bank_load(bankfile, -16384, bank))
                {
                    dbg_printf(" failed.\n");

                    error_printf("Cannot load %s", bankfile);

                    retval |= SDL_FALSE;
                }
                else
                {
                    dbg_printf(" done.\n");
                    strcpy(bank_name, bankfile);
                    retval = SDL_TRUE;
                }
            }
        }
    }

    fclose(f);

    return retval;
}

// -----------------------------------------------------------------------------
//                                  Load bank
// -----------------------------------------------------------------------------
SDL_bool bank_load(char* fname, int size, Uint8 where[])
{
    SDL_RWops* f;
    char* tmpname;

    // MinGW doesn't have asprintf :-(
    tmpname = malloc(strlen(fname) + 10);
    if (!tmpname) {

        return SDL_FALSE;
    }

    sprintf(tmpname, "%s.rom", fname);
    f = SDL_RWFromFile(tmpname, "rb");
    if (!f)
    {
        error_printf("Unable to open '%s'\n", tmpname);
        free(tmpname);
        return SDL_FALSE;
    }

    if (size < 0)
    {
        int filesize;
        SDL_RWseek(f, 0, SEEK_END);
        filesize = SDL_RWtell(f);
        SDL_RWseek(f, 0, SEEK_SET);

        if (filesize > -size)
        {
            error_printf("ROM '%s' exceeds %d bytes.\n", fname, -size);
            SDL_RWclose(f);
            free(tmpname);
            return SDL_FALSE;
        }

        where += (-size) - filesize;
        size = filesize;
    }

    if (SDL_RWread(f, where, size, 1) != 1)
    {
        error_printf("Unable to read '%s'\n", tmpname);
        SDL_RWclose(f);
        free(tmpname);
        return SDL_FALSE;
    }

    SDL_RWclose(f);
    free(tmpname);

    return SDL_TRUE;
}

