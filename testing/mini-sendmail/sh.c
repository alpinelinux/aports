#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXARG 64

int main( int argc, char* const argv[] ) {
    char* args[ MAXARG ] = {};

    if( argc < 3 || strcmp( argv[1], "-c" ) != 0 ) {
        fprintf( stderr, "Usage: %s -c <cmd>\n", argv[0] );
        return 1;
    }

    {
        char* token;
        int i = 0;
        char* argStr = strdup( argv[2] );
        while( ( token = strsep( &argStr, " " ) ) != NULL ) {
            if( token && strlen( token ) )
                args[ i++ ] = token;
            if( i >= MAXARG )
                return 2;
        }
    }

    return execvp( args[0], args );
}

