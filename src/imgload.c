#include "console.h"
#include "graphics.h"
#include "files.h"
#include "utility.h"
#include "netimage.h"
#include "app_key.h"
#include "settings.h"
#include "version.h"
#include "utility.h"

#include <atari.h>
#include <conio.h>
#include <peekpoke.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//
char version[] = "YAIL (Yet Another Image Loader) v" TOSTR(MAJOR_VERSION) "." TOSTR(MINOR_VERSION) "." TOSTR(BUILD_VERSION);

char buff[256]; // A block of memory to be used by all.
bool done = false;
Settings settings;

void help()
{
    cputs("Usage: yail [OPTIONS]\r\n");
          "  -h this message\r\n"
    #ifdef YAIL_BUILD_FILE_LOADER
          "  -l <filename> load image file\r\n"
    #endif
          "  -u <url> use this server address\r\n"
          "  -s <tokens> search terms\r\n";
}

void process_command_line(char* argv[])
{
    switch(argv[1][1])
    {
        case 'h':
            help();
            break;
        #ifdef YAIL_BUILD_FILE_LOADER
        case 'l':
            internal_to_atascii(argv[2], 40);
            load_image_file(argv[2]);
            break;
        #endif
        case 'u':
            strcpy(settings.url, argv[2]);
            break;
        case 's':
            stream_image(&argv[2]);
            break;
    }
}

//
int main(int argc, char* argv[])
{
    // Convert the version string to internal code format
    atascii_to_internal(version, 40);

    // Initialize the settings
    get_settings();

    //
    if(argc > 1)
    {
        process_command_line(argv);
        return 0;
    }
    else
    {
        // Clear the edit buffer so as not to confuse our console code.
        clrscr();

        // Initialize the frame buffer
        saveCurrentGraphicsState();
        setGraphicsMode(GRAPHICS_8);
        clearFrameBuffer();

        // Show console on startup
        show_console();
        start_console(0x00);

        while(!done)
        {
            if(kbhit())
            {
                char ch = cgetc();
                if(ch == CH_ESC)
                    break;
                if(ch == CH_ENTER)
                    ch = 0x00;
                
                show_console();
                start_console(ch);
            }
        }

        setGraphicsMode(GRAPHICS_0);
        restoreGraphicsState();
        clrscr();
    }

    return 0;
}
