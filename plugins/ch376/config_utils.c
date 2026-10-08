// vim: tabstop=4 expandtab
// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


#if SDL_MAJOR_VERSION == 1
# ifdef __SPECIFY_SDL_DIR__
# include <SDL/SDL.h>
# else
# include <SDL.h>
# endif
#else /* SDL_MAJOR_VERSION == 1 */
# ifdef __SPECIFY_SDL_DIR__
# include <SDL2/SDL.h>
# else
# include <SDL.h>
# endif
#endif

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


SDL_bool isws( char c )
{
  if( ( c == 9 ) || ( c == 32 ) ) return SDL_TRUE;
  return SDL_FALSE;
}


// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
SDL_bool read_config_string( char *buf, char *token, char *dest, int maxlen )
{
  int i, toklen, d;

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

