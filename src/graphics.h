// Copyright (C) 2021 Brad Colbert

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "types.h"
#include "utility.h"

#include <stddef.h>

//
#define GRAPHICS_0 0x01
#define GRAPHICS_8 0x02
#define GRAPHICS_9 0x04
#define GRAPHICS_10 0x08
#define GRAPHICS_11 0x10
#define GRAPHICS_CONSOLE_EN 0x80
#define GRAPHICS_8_CONSOLE GRAPHICS_8 | GRAPHICS_CONSOLE_EN
#define GRAPHICS_9_CONSOLE GRAPHICS_9 | GRAPHICS_CONSOLE_EN
#define GRAPHICS_10_CONSOLE GRAPHICS_10 | GRAPHICS_CONSOLE_EN
#define GRAPHICS_11_CONSOLE GRAPHICS_11 | GRAPHICS_CONSOLE_EN

// Memory usage, per line
#define GFX_0_MEM_LINE 40
#define GFX_8_MEM_LINE 40
#define GFX_9_MEM_LINE 40
#define GFX_10_MEM_LINE 40
#define GFX_11_MEM_LINE 40

// Lines per mode
#define GFX_0_LINES 24
#define GFX_8_LINES 220
#define GFX_9_LINES 220
#define GFX_10_LINES 220
#define GFX_11_LINES 220

// Framebuffer memory size
#define FRAMEBUFFER_SIZE (GFX_9_LINES*GFX_9_MEM_LINE)

// Display list memory size
#define DISPLAYLIST_SIZE 256
#define DISPLAYLIST_BLOCK_SIZE 0x1000

//
//#define CONSOLE_MEM 0xBC40  // We should read this from the system before we switch

#ifndef USE_ORIGINAL
// A simple structure for defining a display list in a code compact way
typedef struct _DLModeDef
{
    byte blank_lines;
    byte mode;        // From the Antic modes
    byte lines;       // # of lines of the mode
    byte dli;         // Switch for DLI
    byte* buffer;     // Address of screen memory for mode, 0x0000 if use SAVMSC + offset
} DLModeDef;

typedef DLModeDef** DLModeDefParray;

#define MAX_MODE_DEFS 8
typedef struct _DLDef
{
    void* address;         // location of the DL
    size_t size;           // size in memory of the display list
    DLModeDef modes[MAX_MODE_DEFS]; // compact definition of the display list
} DLDef;

#if 0
typedef struct _GfxDef
{
    byte mode;
    DLDef dl; 
} GfxDef;
#endif

typedef struct image_header
{
    unsigned char v1;
    unsigned char v2;
    unsigned char v3;
    unsigned char gfx;
    unsigned char memtkn;
    short size;
} ImageHeader;

//
typedef struct image_data
{
    ImageHeader header;
    byte* data;
} ImageData;

void saveCurrentGraphicsState(void);
void restoreGraphicsState(void);

void makeDisplayList(byte mode);
void setGraphicsMode(const byte mode);
#ifdef DEBUG_GRAPHICS
void printDList(const char* name);
#endif

void clearFrameBuffer(void);

void show_console(void);
void hide_console(void);

#else

// A simple structure for defining a display list in a code compact way
typedef struct _DLModeDef
{
    byte blank_lines;
    byte mode;        // From the Antic modes
    byte lines;       // # of lines of the mode
    byte dli;         // Switch for DLI
    void* buffer;     // Address of screen memory for mode, 0x0000 if use SAVMSC + offset
} DLModeDef;

typedef DLModeDef** DLModeDefParray;

#define MAX_MODE_DEFS 8
typedef struct _DLDef
{
    void* address;         // location of the DL
    size_t size;           // size in memory of the display list
    DLModeDef modes[MAX_MODE_DEFS]; // compact definition of the display list
} DLDef;

typedef struct _GfxDef
{
    byte mode;
    MemSegs buffer;
    DLDef dl; 
} GfxDef;

void saveCurrentGraphicsState(void);
void restoreGraphicsState(void);

void makeDisplayList(byte mode, const MemSegs* buffInfo, DLDef* dlInfo);
void makeGraphicsDef(byte mode, GfxDef* gfxInfo);

void setGraphicsMode(const byte mode);

void clearFrameBuffer(void);

void show_console(void);
void hide_console(void);

void printDList(const char* name, DLDef* dlInfo);
#endif

#endif // GRAPHICS_H