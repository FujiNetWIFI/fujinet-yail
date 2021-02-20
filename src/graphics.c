// Copyright (C) 2021 Brad Colbert

#include "graphics.h"
#include "console.h"
#include "files.h"
#include "consts.h"
#include "types.h"
#include "utility.h"

#include <conio.h>
#include <atari.h>
#include <peekpoke.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//
void printModeDefs(DLModeDef modeDefs[]);

//
extern char* console_buff;
extern byte console_lines;

// Globals
void* ORG_SDLIST = 0;
void* VDSLIST_STATE = 0;
byte ORG_GPRIOR = 0x0;
byte NMI_STATE = 0x0;
byte ORG_COLOR1, ORG_COLOR2;
GfxDef gfxState;

// DLI definitions
void disable_9_dli(void);  // prototype for below

// Enable Gfx 9
#pragma optimize(push, off)
void enable_9_dli(void) {
    __asm__("pha");
    __asm__("tya");
    __asm__("pha");
    __asm__("txa");
    __asm__("pha");
    __asm__("sta %w", WSYNC);
    POKE(PRIOR, ORG_GPRIOR | GFX_9);
    POKEW(VDSLST, (unsigned)disable_9_dli);
    __asm__("pla");
    __asm__("tax");
    __asm__("pla");
    __asm__("tay");
    __asm__("pla");
    __asm__("rti");
}

// Disable Gfx 9
void disable_9_dli(void) {
    __asm__("pha");
    __asm__("tya");
    __asm__("pha");
    __asm__("txa");
    __asm__("pha");
    __asm__("sta %w", WSYNC);
    POKE(PRIOR, ORG_GPRIOR);
    POKEW(VDSLST, (unsigned)enable_9_dli);
    __asm__("pla");
    __asm__("tax");
    __asm__("pla");
    __asm__("tay");
    __asm__("pla");
    __asm__("rti");
}
#pragma optimize(pop)

void saveCurrentGraphicsState(void)
{
    ORG_SDLIST = OS.sdlst; //PEEKW(SDLSTL);
    VDSLIST_STATE = OS.vdslst; //PEEKW(VDSLST);
    NMI_STATE = ANTIC.nmien;
    ORG_GPRIOR = OS.gprior;       // Save current priority states
    ORG_COLOR1 = OS.color1;
    ORG_COLOR2 = OS.color2;
    //CONSOLE_MEM = PEEKW(SAVMSC);
}

void restoreGraphicsState(void)
{
    ANTIC.nmien = NMI_STATE; //POKE(NMIEN, NMI_STATE);
    OS.vdslst = VDSLIST_STATE; //POKEW(VDSLST, VDSLIST_STATE);
    OS.sdlst = ORG_SDLIST; // POKEW(SDLSTL, ORG_SDLIST);
    OS.color1 = ORG_COLOR1; //POKE(COLOR1, ORG_COLOR1);
    OS.color2 = ORG_COLOR2; //POKE(COLOR2, ORG_COLOR2);
    OS.gprior = ORG_GPRIOR; //POKE(GPRIOR, ORG_GPRIOR);       // restore priority states
    POKE(0x2BF, 24);
}

#define IS_LMS(x) (x & 64)

void generateDisplayList(const MemSegs* buffInfo, DLDef* dlInfo)
{
    byte segCount = 0;
    MemSeg* segDef = NULL;
    byte modeCount = 0;
    DLModeDef* modeDef = &dlInfo->modes[0];
    byte* dlCmd = NULL;
    unsigned line_in_seg = 1;

    // Allocate 1K of memory.  Will shrink when done.
    if(!dlInfo->address)
        dlInfo->address = malloc_constrianed(1024, 1024);
    dlCmd = dlInfo->address;

    #ifdef DEBUG_GRAPHICS
    cprintf("dlInfo_ad%02X dlCmd%02X\n\r", dlInfo->address, dlCmd);
    cgetc();
    clrscr();
    #endif

    while(modeDef->blank_lines != 0xFF) // for all of the modelines
    {
        int i = 0;
        unsigned line;
        void* lms_addr = NULL;

        // Convert the modelines to DL instructions.

        // Blank spaces
        for(; i < modeDef->blank_lines/8; ++i)
            *(dlCmd++) = (byte)DL_BLK8;

        // Iterate through all of the lines
        for(line = 0; line < modeDef->lines; ++line)
        {
            byte b = (byte)modeDef->mode;
            size_t size_block = 0;
            
            segDef = &((MemSegs*)buffInfo)->segs[segCount];
            size_block = (segDef->size / segDef->block_size) * segDef->block_size;

            if(!line) // first line
            {
                if(modeDef->buffer) // references a buffer address so LMS
                {
                    b = DL_LMS(b);
                    lms_addr = modeDef->buffer;
                }
            }

            if((line_in_seg * GFX_8_MEM_LINE) > size_block)
            {
                #ifdef DEBUG_GRAPHICS
                cprintf("%d:%d %d:%d %d %d %p ", modeCount, line, segCount, line_in_seg, line_in_seg * GFX_8_MEM_LINE, size_block, segDef->addr);
                #endif

                // Crossing memory segments means that this is an LMS.
                b = DL_LMS(b);

                ++segCount;
                line_in_seg = 1;

                lms_addr = buffInfo->segs[segCount].addr;
                #ifdef DEBUG_GRAPHICS
                cprintf("%p   \n\r", lms_addr);
                #endif
            }
            else
            {
                #ifdef DEBUG_GRAPHICS
                cprintf("%d:%d %d:%d %d %d %p        \n\r", modeCount, line, segCount, line_in_seg, line_in_seg * GFX_8_MEM_LINE, size_block, segDef->addr);
                #endif
            }
            ++line_in_seg;

            #ifdef DEBUG_GRAPHICS
            if(!((line+1)%24))
            {
                cgetc();
                clrscr();
            }
            #endif

            if(modeDef->dli)
                b = DL_DLI(b);

            *(dlCmd++) = b;

            if(IS_LMS(b))
            {
                *((unsigned*)dlCmd) = (unsigned)lms_addr;  // Add the address
                dlCmd += 2;
            }
        }

        ++modeCount;
        modeDef = &dlInfo->modes[modeCount];
    }

    // Add the JVB
    *(dlCmd++) = DL_JVB;
    *((unsigned*)dlCmd) = (unsigned)dlInfo->address;
    dlCmd+=2;

    // Finally clean up and shrink the memory down to the size of the DL
    dlInfo->size = (size_t)(dlCmd - (byte*)dlInfo->address);

    #ifdef DEBUG_GRAPHICS
    {
        int i;
        for(i = 0; i < dlInfo->size; ++i)
        {
            if(i)
                cputs(",");
            cprintf("%02X", ((byte*)dlInfo->address)[i]);
        }
    }
    cgetc();
    #endif


    //dlInfo->address = realloc(dlInfo->address, dlInfo->size);
}

void makeDisplayList(byte mode, const MemSegs* buffInfo, DLDef* dlInfo)
{
    size_t memPerLine = 0;
    size_t linesPerSeg = 0;
    byte segCount = 0, modeCount = 0;
    byte* dlCmd = NULL;
    unsigned i = 0;

    // Clear the modes
    memset(&dlInfo->modes, 0, sizeof(DLModeDef[MAX_MODE_DEFS]));

    switch(mode)
    {
        case GRAPHICS_0: // {0, DL_CHR40x8x1, 1, 0, CONSOLE_MEM}
        {
            DLModeDef def = {16, DL_CHR40x8x1, GFX_0_LINES, 0, 0};
            def.buffer = buffInfo->segs[0].addr;
            memPerLine = GFX_0_MEM_LINE;
            dlInfo->modes[0] = def;
            dlInfo->modes[1].blank_lines = 0xFF;
        }
        break;
        case GRAPHICS_8: // {8, DL_MAP320x1x1, 211, 0, 0}
        case GRAPHICS_9:
        case GRAPHICS_10:
        case GRAPHICS_11:
        {
            DLModeDef def = {8, DL_MAP320x1x1, GFX_8_LINES, 0, 0};
            def.buffer = buffInfo->segs[0].addr;
            memPerLine = GFX_8_MEM_LINE;
            dlInfo->modes[0] = def;
            dlInfo->modes[1].blank_lines = 0xFF;
        }
        break;
        case GRAPHICS_8_CONSOLE: // {8, DL_MAP320x1x1, 211, 0, 0}
        {
            DLModeDef gfx = {8, DL_MAP320x1x1, 0, 0, 0x0};
            DLModeDef console = {0, DL_CHR40x8x1, 0, 0, 0x0};
            gfx.lines = GFX_8_LINES - (8 * console_lines);
            gfx.buffer = buffInfo->segs[0].addr;
            console.lines = console_lines;
            console.buffer = OS.savmsc; //console_buff;
            memPerLine = GFX_8_MEM_LINE;
            dlInfo->modes[0] = gfx;
            dlInfo->modes[1] = console;
            dlInfo->modes[2].blank_lines = 0xFF;
        }
        break;
        case GRAPHICS_9_CONSOLE:
        case GRAPHICS_10_CONSOLE:
        case GRAPHICS_11_CONSOLE:
        {
            DLModeDef gfx = {8, DL_MAP320x1x1, 0, 0, 0x0};
            DLModeDef gfx_dli = {0, DL_MAP320x1x1, 1, 1, 0x0};
            gfx.lines = (GFX_8_LINES - (8 * console_lines)) - 1;
            gfx.buffer = buffInfo->segs[0].addr;
            memPerLine = GFX_8_MEM_LINE;
            dlInfo->modes[0] = gfx;
            dlInfo->modes[1] = gfx_dli;
            if(console_lines > 1)
            {
                DLModeDef console = {0, DL_CHR40x8x1, 0, 0, 0x0};
                DLModeDef console_dli = {0, DL_CHR40x8x1, 1, 1, 0x0};
                console.lines = console_lines - 1;
                console.buffer = OS.savmsc; //console_buff;
                dlInfo->modes[2] = console;
                dlInfo->modes[3] = console_dli;
                dlInfo->modes[4].blank_lines = 0xFF;
            }
            else
            {
                DLModeDef console = {0, DL_CHR40x8x1, 1, 1, 0x0};
                console.buffer = OS.savmsc; //console_buff;
                dlInfo->modes[2] = console;
                dlInfo->modes[3].blank_lines = 0xFF;
            }
        }
        break;

    } // switch mode

    #ifdef DEBUG_GRAPHICS
    clrscr();
    printModeDefs(dlInfo->modes);
    cgetc();
    clrscr();
    #endif

    generateDisplayList(buffInfo, dlInfo);
}

void freeDisplayList(DLDef* dlInfo)
{
    free(dlInfo->address);
    memset(dlInfo, 0, sizeof(DLDef));
}

void makeGraphicsDef(byte mode, GfxDef* gfxInfo)
{
    switch(mode)
    {
        case GRAPHICS_0:
            allocSegmentedMemory(GFX_0_MEM_LINE, GFX_0_LINES, 4096, &gfxInfo->buffer);
            break;
        case GRAPHICS_8:
        case GRAPHICS_9:
        case GRAPHICS_10:
        case GRAPHICS_11:
        case GRAPHICS_8_CONSOLE:
        case GRAPHICS_9_CONSOLE:
            allocSegmentedMemory(GFX_8_MEM_LINE, GFX_8_LINES, 4096, &gfxInfo->buffer);
    }

    makeDisplayList(mode, &gfxInfo->buffer, &gfxInfo->dl);
}

void enableConsole()
{
    switch(gfxState.mode)
    {
        case GRAPHICS_0: // {0, DL_CHR40x8x1, 1, 0, CONSOLE_MEM}
            break;
        case GRAPHICS_8: // {8, DL_MAP320x1x1, 211, 0, 0}
        {
            gfxState.mode |= GRAPHICS_CONSOLE_EN;

            makeDisplayList(gfxState.mode, &gfxState.buffer, &gfxState.dl);

            OS.sdlst = gfxState.dl.address;
            ANTIC.nmien = 0x40;

            POKE(0x2BF, 5);
        }
        break;
        case GRAPHICS_9:
        case GRAPHICS_10:
        case GRAPHICS_11:
        {
            gfxState.mode |= GRAPHICS_CONSOLE_EN;

            makeDisplayList(gfxState.mode, &gfxState.buffer, &gfxState.dl);

            OS.sdlst = gfxState.dl.address;
            OS.vdslst = disable_9_dli;
            ANTIC.nmien = 0x80 | 0x40;

            POKE(0x2BF, 5);
        }

    }
}

void disableConsole()
{
    switch(gfxState.mode) // ^ GRAPHICS_CONSOLE_EN)
    {
        case GRAPHICS_0: // {0, DL_CHR40x8x1, 1, 0, CONSOLE_MEM}
            break;
        case GRAPHICS_8_CONSOLE: // {8, DL_MAP320x1x1, 211, 0, 0}
        case GRAPHICS_9_CONSOLE:
        case GRAPHICS_10_CONSOLE:
        case GRAPHICS_11_CONSOLE:
        {
            gfxState.mode &= (byte)~GRAPHICS_CONSOLE_EN;

            makeDisplayList(gfxState.mode, &gfxState.buffer, &gfxState.dl);
            POKEW(SDLSTL, gfxState.dl.address);
            //OS.sdlst = gfxState.dl.address;

            ANTIC.nmien = 0x40;
            OS.vdslst = VDSLIST_STATE;

            POKE(0x2BF, 0);
        }
    }
}

void setGraphicsMode(byte mode)
{
    if(mode == gfxState.mode)
        return;

    if(!gfxState.buffer.size)
    {
        // don't free the display list memory.  It will be persistent.

        freeSegmentedMemory(&gfxState.buffer);

        // 
        makeGraphicsDef(mode, &gfxState);
    }

    #ifdef DEBUG_GRAPHICS
    cprintf("%p %p\n\r", gfxState.dl.address, gfxState.buffer.start);
    cgetc();
    #endif

    OS.color1 = 14;         // Color maximum luminance
    OS.color2 = 0;          // Background black

    switch(mode)
    {
        case GRAPHICS_0:
            OS.sdlst = gfxState.dl.address; //ORG_SDLIST;
            ANTIC.nmien = NMI_STATE;
            OS.vdslst = VDSLIST_STATE;
            POKE(0x2BF, 26);
        break;
        default:
            OS.sdlst = gfxState.dl.address;
        break;
    }

    // Set graphics mode specifc things
    switch(mode & ~GRAPHICS_CONSOLE_EN)
    {
        case GRAPHICS_0:
        case GRAPHICS_8:
            OS.gprior = ORG_GPRIOR; // Turn off GTIA
        break;
        case GRAPHICS_9:
            OS.gprior = ORG_GPRIOR | GFX_9;   // Enable GTIA   
        break;
        case GRAPHICS_10:
            OS.gprior = ORG_GPRIOR | GFX_10;   // Enable GTIA   
        break;
        case GRAPHICS_11:
            OS.gprior = ORG_GPRIOR | GFX_11;   // Enable GTIA   
        break;
    }

    gfxState.mode = mode;
}

void clearFrameBuffer()
{
    memset(gfxState.buffer.start, 0, gfxState.buffer.size);
}


#ifdef DEBUG_GRAPHICS
void printModeDefs(DLModeDef modeDefs[])
{
    byte i = 0;
    for(; i < MAX_MODE_DEFS; ++i)
    {
        if(modeDefs[i].blank_lines == 0xFF)
            return;

        cprintf("%d, %d, %d, %d, %02X\n\r", modeDefs[i].blank_lines, modeDefs[i].mode, modeDefs[i].lines, modeDefs[i].dli, modeDefs[i].buffer);
    }
}

// Shows the contents of a display list.
// name - simply used in the output header so you can tell which DL is which on the console.
// mloc - the location of the DL in memory 
void printDList(const char* name, DLDef* dlInfo)
{
    byte b = 0x00, low, high;
    unsigned idx = 0;

    cprintf("Displaylist %s at %p (%d)\n\r", name, dlInfo->address, dlInfo->size);
    while(1)
    {
        if(idx)
            cprintf(", ");

        // Get the instruction
        b = ((byte*)dlInfo->address)[idx];
        if((b & 0x40) && (b & 0x0F)) // these have two address bytes following)
        {
            low = ((byte*)dlInfo->address)[++idx];
            high = ((byte*)dlInfo->address)[++idx];
            cprintf("%02X (%02x%02x)", b, low, high);
            cgetc();
            clrscr();
        }
        else
            cprintf("%02X", b);

        idx++;

        if(b == 0x41) // JVB so done... maybe  have to add code to look at the address
            break;
    }
}
#endif