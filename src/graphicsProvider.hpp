#ifndef __GRAPHICSPROVIDER_H
#define __GRAPHICSPROVIDER_H
#include <fxcg/display.h>
#include <fxcg/file.h>
#include <fxcg/keyboard.h>
#include <fxcg/system.h>
#include <fxcg/misc.h>
#include <fxcg/app.h>
#include <fxcg/serial.h>
#include <fxcg/rtc.h>
#include <fxcg/heap.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define plot(x,y,c) Bdisp_SetPoint_VRAM(x, y, c)
void drawRectangle(int x, int y, int width, int height, unsigned short color);
void drawLine(int x1, int y1, int x2, int y2, int color);
void drawFilledCircle(int centerx, int centery, int radius, color_t color);
void drawCircularCountdownIndicator(int centerx, int centery, int radius, color_t colorfg, color_t colorbg, int inner, int swap);
void replaceColorInArea(int x, int y, int width, int height, color_t color_old, color_t color_new);
void invertArea(short x, short y, short width, short height);
void darkenStatusbar();
void darkenFkeys(int numkeys);
void drawArrowDown(int bottomX, int bottomY, int color);
void drawFkeyPopup(int Fkey, const char* title);
void CopySpriteMasked(const unsigned short* data, int x, int y, int width, int height, unsigned short maskcolor);
void CopySpriteNbitMasked(const unsigned char* data, int x, int y, int width, int height, const color_t* palette, color_t maskColor, unsigned int bitwidth);
void drawSegvaultLogo(int x, int y);
int textColorToFullColor(int textcolor);
void progressMessage(const char* message, int cur, int total);
void closeProgressMessage();
void printCentered(char* text, int y, int FGC, int BGG);
void clearLine(int x, int y, color_t color=COLOR_WHITE);
void mPrintXY(int x, int y, const char*msg, int mode, int color);
void mPrintMini(int* x, int* y, const char* msg, int mode,
                unsigned int colorMask, int arg5, int arg6,
                int fg, int bg, int draw, int arg10);
// These variants deliberately bypass Utilities' message table.  They are for
// short ASCII metadata and numbers; passing a GB18030 translation to the
// mini-mini renderer is not reliable on all CG50 OS revisions.
void mPrintMiniRaw(int* x, int* y, const char* msg, int mode,
                   unsigned int colorMask, int arg5, int arg6,
                   int fg, int bg, int draw, int arg10);
void mPrintMiniMini(int* x, int* y, const char* msg, int mode,
                    int color, int draw);
// PrintMiniMini normally adds 24 pixels for the status area. Use this helper
// when x/y are already absolute LCD pixel coordinates.
void mPrintMiniMiniAt(int* x, int* y, const char* msg, int mode,
                      int color, int draw);
void mPrintMiniMiniRawAt(int* x, int* y, const char* msg, int mode,
                         int color, int draw);
// Render compact metadata without allowing a long field to cross the LCD
// edge. maxWidth is measured from the supplied x coordinate.
void mPrintMiniMiniClippedAt(int* x, int* y, const char* msg, int mode,
                             int color, int maxWidth, int draw);
void mPrintMiniMiniRawClippedAt(int* x, int* y, const char* msg, int mode,
                                int color, int maxWidth, int draw);
void multiPrintXY(int x, int y, const char* msg, int mode, int color);
void multiPrintMini(int x, int y, const char* msg, int fgcolor=COLOR_BLACK);
void mMsgBoxPush(int lines);
void mMsgBoxPop();
void popAllMsgBoxes();
int getMsgBoxCount();
void drawScreenTitle(const char* title, const char* subtitle = NULL);
void drawFkeyLabels(int f1=-1, int f2=-1, int f3=-1, int f4=-1, int f5=-1, int f6=-1);
int drawRGB24toRGB565(int r, int g, int b);

// needed for analog clock and some other... circular things:
double sine(double x);
//double cosine(double x);
#define cosine(x) sine((M_PI_2)+(x))
// for other graphics-related things:
int ipow(int base, int exp);

#endif
