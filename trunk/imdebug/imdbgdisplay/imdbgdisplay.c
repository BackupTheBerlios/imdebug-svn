/*-----------------------------------------------------------
  imdbgdisplay.c -- displayer for debug images
 *-----------------------------------------------------------
  Copyright (c) 2002-2005 William V. Baxter III
 
  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any
  damages arising from the use of this software.
 
  Permission is granted to anyone to use this software for any
  purpose, including commercial applications, and to alter it and
  redistribute it freely, subject to the following restrictions:
 
  1. The origin of this software must not be misrepresented; you must
     not claim that you wrote the original software. If you use
     this software in a product, an acknowledgment in the product
     documentation would be appreciated but is not required.
 
  2. Altered source versions must be plainly marked as such, and must
     not be misrepresented as being the original software.
 
  3. This notice may not be removed or altered from any source distribution.
 *-----------------------------------------------------------
  Acknowledgements to Willem van Schaik who wrote the 
  "VisualPNG" program demo.  The image viewer code here was
  based initally off of his source code.

  For more info on VisualPNG see:
      http://www.schaik.com/png/visualpng.html

  Acknowledgements also to Jeffery Richter whose book,
  _Advanced_Windows_3rd_Ed, and accompanying source code, were
  critical in getting me up to speed on Win32 programming issues, like
  handling messages and memory-mapped file manipulation.
  *-----------------------------------------------------------*/

#include <windows.h>
#include <winuser.h>
#include <commctrl.h>
#include <math.h>
#include <float.h>
#include "CmnHdr.h"
#include "imdebug_priv.h"
#include "imdbgdisp_res.h"
#include "imdbgdispfile.h"
#include "blitfuncs.h"
#include "stdio.h"

#define NUM_SAVED_IMAGES_DEFAULT 10

#define ERROR_NO_IMAGE 0
#define ERROR_NULL_POINTER -1
#define ERROR_ZERO_PIXELS -2
#define ERROR_TOO_BIG -3
#define ERROR_ALLOC_FAIL -4

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM) ;
BOOL    CALLBACK AboutDlgProc (HWND, UINT, WPARAM, LPARAM) ;
BOOL    CALLBACK OptionsDlgProc (HWND, UINT, WPARAM, LPARAM) ;
BOOL CenterAbout (HWND hwndChild, HWND hwndParent);
BOOL isViewMenuChecked(HMENU hMenu, UINT uId);
BOOL isChannelMenuChecked(HMENU hMenu, UINT uId);

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#endif

enum {IMDM_TRANSLATE, IMDM_SCALE, IMDM_BIAS, IMDM_AUTOSCALE, IMDM_UNITYSCALE};
#define SCALE_BIAS_CONTROL_WIDTH 15
enum { BF_STRETCH=0x1, BF_SCALE=0x2, BF_BIAS=0x4 };
enum { BF_INT_OFFSET=0, BF_FLOAT_OFFSET = 5 };

PBLITFUNC g_blitFuncTable[BF_STRETCH|BF_SCALE|BF_BIAS][10];

static int g_historySize = NUM_SAVED_IMAGES_DEFAULT;
static BOOL g_matchScaleOnTagged = TRUE;

struct { UINT id; const char *key; } g_checkableMenuItems[] = {
  { IDM_VIEW_CHECK,   "check" },
  { IDM_VIEW_STRETCH, "stretch" },
  { IDM_VIEW_GRID,    "grid" },
  { IDM_VIEW_FLIP,    "flip" },
  { IDM_VIEW_SHARE,   "shareDisplay" },
  { IDM_VIEW_SB_CONTROLS,  "sbControls" },
  { 0, 0 }
};

typedef struct
{
  BOOL stretched;
  BOOL checked;
  BOOL flipped;
  BOOL calcZoom; // request to automatically scale/translate
  int  margin;
  int  extraBLmargin;  // extra margin for scrollbars when calcZoom on and
                       // scale&bias scrollbars are on
  int  grid;
  int  tx, ty; // xy translation
  float zoom;  // uniform stretch scaling
  float lastTypeMax;
  unsigned char enabledChannels;
}  DrawAttribs;

enum { 
  IMDBG_ENABLE_RED   = 0x1,
  IMDBG_ENABLE_GREEN = 0x2,
  IMDBG_ENABLE_BLUE  = 0x4,
  IMDBG_ENABLE_ALPHA = 0x8,
  IMDBG_ENABLE_RGB   = 0x7,
  IMDBG_ENABLE_RGBA  = 0xf
};

typedef struct
{
  int          ImgX, ImgY, WinW, WinH, PtrX, PtrY;
  ImProps     *pImprops;
  DrawAttribs *pDrawAttrs;
  BYTE        *pRawData;
} StatusMessageInfo;

static void initDrawAttribs(DrawAttribs *da)
{
  da->stretched = TRUE;
  da->checked = FALSE;
  da->flipped = FALSE;
  da->grid = 0;
  da->tx = da->ty = 0;
  da->zoom = 1.0;
  da->margin = 8;
  da->calcZoom = TRUE;
  da->extraBLmargin = 0;
  da->lastTypeMax = 1.0f;
  da->enabledChannels = IMDBG_ENABLE_RGBA;
}

static void getEnableAttrsFromMenuState(HMENU hMenu, DrawAttribs *da)
{
  if (isChannelMenuChecked(hMenu, IDM_CHAN_RED)) {
    da->enabledChannels = IMDBG_ENABLE_RED;
  }
  else if (isChannelMenuChecked(hMenu, IDM_CHAN_GREEN)) {
    da->enabledChannels = IMDBG_ENABLE_GREEN;
  }
  else if (isChannelMenuChecked(hMenu, IDM_CHAN_BLUE)) {
    da->enabledChannels = IMDBG_ENABLE_BLUE;
  }
  else if (isChannelMenuChecked(hMenu, IDM_CHAN_ALPHA)) {
    da->enabledChannels = IMDBG_ENABLE_ALPHA;
  }
  else if (isChannelMenuChecked(hMenu, IDM_CHAN_RGB)) {
    da->enabledChannels = IMDBG_ENABLE_RGB;
  }
  else {
    da->enabledChannels = IMDBG_ENABLE_RGBA;
  }
}

static void getDrawAttrsFromMenuState(HMENU hMenu, DrawAttribs *da)
{
  da->stretched = isViewMenuChecked(hMenu, IDM_VIEW_STRETCH);
  da->checked =   isViewMenuChecked(hMenu, IDM_VIEW_CHECK);
  da->flipped =   isViewMenuChecked(hMenu, IDM_VIEW_FLIP);
  da->grid =      isViewMenuChecked(hMenu, IDM_VIEW_GRID);
  da->extraBLmargin =
    isViewMenuChecked(hMenu, IDM_VIEW_SB_CONTROLS)?SCALE_BIAS_CONTROL_WIDTH:0;

  getEnableAttrsFromMenuState(hMenu, da);
}

void GetPaintableRect(RECT *rect, HWND mainwin, HWND statuswin)
{
  // Returns rect of client area minus status bar area
  // and minus scale and bias sliders

  RECT winRect, stRect;
  int cw = SCALE_BIAS_CONTROL_WIDTH;
  GetClientRect(mainwin, &winRect);
  GetWindowRect(statuswin, &stRect);
  if ( !isViewMenuChecked(GetMenu(mainwin), IDM_VIEW_SB_CONTROLS) ) {
    cw = 0;
  }
  SetRect(rect, winRect.left, winRect.top, 
          winRect.right - cw, winRect.bottom -cw - (stRect.bottom-stRect.top));
}

static void initBlitFuncs()
{
  // First get the complete list of available blitters
#define DECLARE_BLITTER(x,y) x, #y,
  struct { PBLITFUNC f; char *a; } blitFuncs[] = {
    #include "blitfuncdecl.h"
    0, 0
  };
#undef DECLARE_BLITTER

  // next sort them into table by category & arg size/type
  {
    int i=0;
    int flags;
    int index;
    while(blitFuncs[i].f != 0) {
      const char *a=blitFuncs[i].a;
      unsigned char c;
      flags = 0;
      index = 0;
      for (c=0; c<strlen(a); c++) {
        switch(a[c]) {
        case 'z': flags |= BF_STRETCH; break;
        case 's': flags |= BF_SCALE; break;
        case 'b': flags |= BF_BIAS; break;
        case '8': index += 0; break;
        case '6': index += 1; break;
        case '2': index += 2; break;
        case '4': index += 3; break;
        case 'n': index += 4; break;
        case 'i': index += BF_INT_OFFSET; break;
        case 'f': index += BF_FLOAT_OFFSET; break;
        default: 
          flags = -1; // this one doesn't go in the table
        }
      }
      if (flags>=0)
        g_blitFuncTable[flags][index] = blitFuncs[i].f;
      i++;
    }
  } 
}


static void loadRegistryPrefs(HWND mainWnd)
{
  HKEY myKey;
  LONG ret;
  RECT rect;
  DWORD historySize;
  DWORD size;
  DWORD type;
  HMENU hMenu = GetMenu(mainWnd);

  ret = RegOpenKeyEx(HKEY_CURRENT_USER,
                     TEXT("Software\\imdebug"),
                     0,
                     KEY_READ,
                     &myKey);
  if (ERROR_SUCCESS != ret) {
    return;
  }

  type = REG_BINARY;
  size = sizeof(RECT);
  ret = RegQueryValueEx( myKey, TEXT("rect"), NULL, &type,(BYTE*)&rect,&size);
  if (ret == ERROR_SUCCESS) {
    MoveWindow(mainWnd, rect.left, rect.top, 
               rect.right-rect.left, rect.bottom-rect.top, TRUE);
  }

  type = REG_DWORD;
  size = sizeof(DWORD);
  ret = RegQueryValueEx( myKey, TEXT("history"), NULL, 
                         &type,(BYTE*)&historySize, &size);
  if (ret == ERROR_SUCCESS) {
    g_historySize = historySize;
  }

  {
    BOOL flag;
    type = REG_BINARY;
    size = sizeof(BOOL);
    ret = RegQueryValueEx( myKey, TEXT("copyscale"), NULL,
                           &type, (BYTE*)&flag, &size);
    if (ret == ERROR_SUCCESS) {
      g_matchScaleOnTagged = flag;
    }
  }
 
  {
    int i=0;
    BOOL flag;
    size = sizeof(BOOL);
    while (g_checkableMenuItems[i].id) {
      ret = RegQueryValueEx( myKey, g_checkableMenuItems[i].key,
                             NULL, &type, (BYTE*)&flag, &size);
      if (ret == ERROR_SUCCESS) {
        CheckMenuItem (hMenu, g_checkableMenuItems[i].id, 
                       flag?MF_CHECKED:MF_UNCHECKED);
      }
      i++;
    }
  }
  RegCloseKey(myKey);

}

static void saveRegistryPrefs(HWND mainWnd)
{
  HKEY myKey;
  LONG ret;
  RECT rect;
  BOOL flag;
  HMENU hMenu = GetMenu(mainWnd);

  ret = RegOpenKeyEx(HKEY_CURRENT_USER,
                     TEXT("Software\\imdebug"),
                     0,
                     KEY_WRITE|KEY_READ,
                     &myKey);
  if (ERROR_SUCCESS != ret) {
    // try create
    ret = RegCreateKeyEx(
      HKEY_CURRENT_USER,
      TEXT("Software\\imdebug"),
      0, NULL,
      REG_OPTION_NON_VOLATILE,
      KEY_WRITE,
      NULL,
      &myKey,
      NULL
      );
    if (ret != ERROR_SUCCESS) 
      return;
  }
 
  GetWindowRect(mainWnd, &rect);
  ret = RegSetValueEx(myKey, TEXT("rect"),0, REG_BINARY, 
                      (BYTE*)&rect, sizeof(RECT) );

  ret = RegSetValueEx(myKey, TEXT("history"),0, REG_DWORD, 
                      (BYTE*)&g_historySize, sizeof(UINT) );

  ret = RegSetValueEx(myKey, TEXT("copyscale"),0, REG_BINARY, 
                      (BYTE*)&g_matchScaleOnTagged, sizeof(BOOL) );

  {
    int i=0;
    while (g_checkableMenuItems[i].id) {
      flag = isViewMenuChecked(hMenu, g_checkableMenuItems[i].id);
      ret = RegSetValueEx(myKey, g_checkableMenuItems[i].key,0, REG_BINARY,
                          (BYTE*)&flag, sizeof(BOOL) );
      i++;
    }
  }
  RegCloseKey(myKey);
}

static void mapWindowToImage(int xWin, int yWin, int hWin, 
                             int* xImg, int *yImg,
                             const DrawAttribs *pAttrs, 
                             const ImProps *pProps)
{
  if (pAttrs->flipped) {
    yWin = hWin - yWin;
  }
  *xImg = (int)floor((xWin - pAttrs->tx)/pAttrs->zoom);
  *yImg = (int)floor((yWin - pAttrs->ty)/pAttrs->zoom);
}

static void mapImageToWindow(int xImg, int yImg, int *xWin, int *yWin,
                             const DrawAttribs *pAttrs, const ImProps *pProps)
{
  *xWin = (int)floor((xImg*pAttrs->zoom) + pAttrs->tx);
  *yWin = (int)floor((yImg*pAttrs->zoom) + pAttrs->ty);
}


static void createFileMapping(HANDLE *pFileMap, LPVOID *pMapView)
{

  *pFileMap = CreateFileMapping((HANDLE) 0xFFFFFFFF, 
                                NULL, PAGE_READWRITE, 0, 
                                IMDBG_MAX_DATA,
                                __TEXT("ImgDebugPipe"));
  if (*pFileMap != NULL) {
    // File mapping created successfully (or already existed).
    if (*pMapView == NULL)
    {
      // Map a (read-only) view of the file into our address space.
      *pMapView = MapViewOfFile(*pFileMap,
                                FILE_MAP_READ
                                /*| FILE_MAP_WRITE*/, 0, 0, 0);
      
      if ((BYTE *) *pMapView == NULL) 
      {
        chMB(__TEXT("Can't map view of file."));
      }
    }
    
  } else {
    chMB(__TEXT("Can't create file mapping."));
  }
}

static void destroyFileMapping(HANDLE *pFileMap, LPVOID *pMapView)
{
  if (*pMapView) {  
    UnmapViewOfFile (*pMapView) ;  
    *pMapView = NULL;
  }
  if (*pFileMap) {
    CloseHandle(*pFileMap);
    *pFileMap = NULL;
  }
}


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PSTR szCmdLine, int iCmdShow)
{
  static char  szAppName[] = "ImDbgDisplay" ;
  HWND         hwnd ;
  MSG          msg ;
  WNDCLASSEX   wndclass ;
  HACCEL       hAccel;
  int ixBorders, iyBorders;

  wndclass.cbSize        = sizeof (wndclass) ;
  wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
  wndclass.lpfnWndProc   = WndProc ;
  wndclass.cbClsExtra    = 0 ;
  wndclass.cbWndExtra    = 0 ;
  wndclass.hInstance     = hInstance ;
  wndclass.hIcon         = LoadIcon (hInstance, szAppName ) ;
  wndclass.hIconSm       = ExtractIcon(hInstance, szAppName, 1) ;
  wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
  wndclass.hbrBackground = NULL;//(HBRUSH) GetStockObject (BLACK_BRUSH) ;
  wndclass.lpszMenuName  = szAppName;
  wndclass.lpszClassName = szAppName ;

  RegisterClassEx (&wndclass) ;

  // calculate size of window-borders
  ixBorders = 2 * (GetSystemMetrics (SM_CXBORDER) +
                   GetSystemMetrics (SM_CXDLGFRAME));
  iyBorders = 2 * (GetSystemMetrics (SM_CYBORDER) +
                   GetSystemMetrics (SM_CYDLGFRAME)) +
                   GetSystemMetrics (SM_CYCAPTION) +
                   GetSystemMetrics (SM_CYMENUSIZE) +
                   1; /* WvS: don't ask me why? */

  hwnd = CreateWindow (
    szAppName, IMDBG_DISPLAY_WINDOW_TITLE,
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT,
    512 + ixBorders, 512 + iyBorders,
    //CW_USEDEFAULT, CW_USEDEFAULT,
    NULL, NULL, hInstance, NULL) ;

  InitCommonControls();

  initBlitFuncs();

  ShowWindow (hwnd, iCmdShow) ;
  UpdateWindow (hwnd) ;

  hAccel = LoadAccelerators (hInstance, szAppName);

  while (GetMessage (&msg, NULL, 0, 0))
  {
    if (!TranslateAccelerator (hwnd, hAccel, &msg))
    {
      TranslateMessage (&msg) ;
      DispatchMessage (&msg) ;
    }
  }
  return msg.wParam ;
}


void DrawBitmap (HDC hdc, int xStart, int yStart, HBITMAP hBitmap)
{
  BITMAP bm ;
  HDC    hMemDC ;
  POINT  pt ;

  hMemDC = CreateCompatibleDC (hdc) ;
  SelectObject (hMemDC, hBitmap) ;
  GetObject (hBitmap, sizeof (BITMAP), (PSTR) &bm) ;
  pt.x = bm.bmWidth ;
  pt.y = bm.bmHeight ;

  BitBlt (hdc, xStart, yStart, pt.x, pt.y, hMemDC, 0, 0, SRCCOPY) ;

  DeleteDC (hMemDC) ;
}


static void setScaleAndBias(BlitStats *bs, float typeMin, float typeMax, 
                            float *scale, float *bias)
{
  // EVEN NEWER BEHAVIOR:
  // Compute global scale and bias.  Scaling per-channel was just weird.
  // If we always link all channels that's just as good an arbitrary choice
  // as always scaling independently.

  // NEW BEHAVIOR:
  // All scales and biases are computed independently per channel.
  // Otherwise we have to provide some way to specify independently
  // what channels are linked and that's a pain.  Besides, usually when you
  // need autoscale it's because you're looking at single-channel grid
  // values, and then it's not an issue.

  // OLD BEHAVIOR:
  // calculates a uniform scale across channels that gets all channels
  // to fit in the type-allowable range.

  int i;

  float vmin = typeMax;
  float vmax = typeMin;
  float fScale;
  for (i=0; i<4; i++) {
    if (scale[i] == IMDBG_AUTOSCALE) {
      if (vmin > bs->min[i]) vmin = bs->min[i];
      if (vmax < bs->max[i]) vmax = bs->max[i];

    }
  }
  
  if (fabs(vmin-vmax)<1e-7) {
    fScale = 1.0f;
  }
  else {
    fScale = (typeMax-typeMin)/(vmax-vmin);
  }
  for (i=0; i<4; i++) {
    if (scale[i] == IMDBG_AUTOSCALE) {
      scale[i] = (typeMax-typeMin)/(vmax-vmin);
      bias[i]  = (typeMin-vmin) * scale[i];
    }
  }

}          


BOOL FillBitmap (
  BYTE *pDiData, int WinW, int WinH,
  ImProps *pProps, BYTE *pbImage, DrawAttribs* pAttrs )
{
  const int cDIChannels = 3;
  int NewW, NewH;
  int cxImgPos, cyImgPos;

  int cxImgSize =    pProps->width;
  int cyImgSize =    pProps->height;
  int cImgChannels = pProps->nchan;

  BOOL uniformType = TRUE;
  BOOL uniformWidth = TRUE;
  BOOL byteAligned = TRUE;
  BOOL byteAlignedCol = TRUE;
  BOOL computeScaleAndBias = FALSE;

  char srcChanByteOffsets[4] = {-1,-1,-1,-1};
  char srcChanBitOffsets[4] = {-1,-1,-1,-1};
  int srcSampleByteWidth = 0;
  int srcSampleBitWidth  = 0;

  int MARGIN = pAttrs->margin;

  int blitFuncFlags = -1;
  int blitFuncIndex = -1;

  float typeMax = 0, typeMin = 0;

  PBLITFUNC pBlitFunc = 0;
  PBLITFUNC pBlitNoStretchFunc = 0;

  // channel offsets
/*
  int ro = pProps->chanmap[0];
  int go = pProps->chanmap[1];
  int bo = pProps->chanmap[2];
*/
  int ao, cOutChannels;
  char effectiveChanmap[4];
  memcpy(effectiveChanmap, pProps->chanmap, 4*sizeof(char));
  if (! (pAttrs->enabledChannels & IMDBG_ENABLE_RED))
    effectiveChanmap[0] = -1;
  if (! (pAttrs->enabledChannels & IMDBG_ENABLE_GREEN))
    effectiveChanmap[1] = -1;
  if (! (pAttrs->enabledChannels & IMDBG_ENABLE_BLUE))
    effectiveChanmap[2] = -1;
  if (! (pAttrs->enabledChannels & IMDBG_ENABLE_ALPHA))
    effectiveChanmap[3] = -1;

  // Make alpha grayscale if that's all there is
  if (pAttrs->enabledChannels == IMDBG_ENABLE_ALPHA) {
    effectiveChanmap[0] = effectiveChanmap[3];
    effectiveChanmap[1] = effectiveChanmap[3];
    effectiveChanmap[2] = effectiveChanmap[3];
    effectiveChanmap[3] = -1;
  }

  ao = effectiveChanmap[3];

  cOutChannels = (ao<0) ? 3 : 4;

  {
    // detect some special "easy" cases
    int i;
    int type = pProps->type[0];
    for (i=0; i<cImgChannels; i++)
    {
      if (pProps->bpc[i] & 0x7) byteAligned = FALSE;
      if (pProps->type[i] != type)
        uniformType = FALSE;
      if (pProps->bpc[i] != pProps->bpc[0]) {
        uniformWidth = FALSE;
      }
    }
  }

  // Compute byte channel offsets and widths
  {
    int j;
    for (j=0; j<4; j++) {
      if (effectiveChanmap[j] >= 0) {
        srcChanBitOffsets[j] = pProps->bitoffset[ effectiveChanmap[j] ];
        srcChanByteOffsets[j] = srcChanBitOffsets[j] >> 3;
      }
    }
    srcSampleBitWidth = pProps->colstride;
    srcSampleByteWidth = srcSampleBitWidth >> 3;
    if (srcSampleBitWidth&7) byteAlignedCol = FALSE;
  }

  if (uniformType && (byteAligned || byteAlignedCol)) {
    int i;
    typeMax = (float)(~((~0UL)<<pProps->bpc[0]));
    if (!uniformWidth) {
      blitFuncIndex = 4;
    }
    else {
      switch(pProps->bpc[0]) {
      case 8:   blitFuncIndex = 0;  break;
      case 16:  blitFuncIndex = 1;  break;
      case 32:  blitFuncIndex = 2;  break;
      case 64:  blitFuncIndex = 3;  break;
      default:  blitFuncIndex = 4;
      }
    }
    if (pProps->type[0] == IMDBG_FLOAT) {
      blitFuncIndex += BF_FLOAT_OFFSET;
      typeMax = 1.0;
    }
    blitFuncFlags = 0;
    if (pAttrs->stretched) {
      blitFuncFlags |= BF_STRETCH;
    }
    for(i=0;i<4;i++) {
      if (pProps->bias[i] != 0.0) {
        blitFuncFlags |= BF_BIAS;
      }
      if (pProps->scale[i] != 1.0) {
        blitFuncFlags |= BF_SCALE;
      }
      if (pProps->scale[i] == IMDBG_AUTOSCALE) {
        computeScaleAndBias = TRUE;
        blitFuncFlags |= BF_BIAS;
        blitFuncFlags |= BF_SCALE;
      }
    }
  }

  pAttrs->lastTypeMax = typeMax;
  //----------------------------------------
  // handle the image stretched

  if (!pAttrs->calcZoom) {
    // use given tx, ty and zoom
    cxImgPos = pAttrs->tx;
    cyImgPos = pAttrs->ty;
    NewW = (int)(pAttrs->zoom * cxImgSize);
    NewH = (int)(pAttrs->zoom * cyImgSize);
  }
  else if (pAttrs->stretched)
  {
    NewW = WinW - 2 * MARGIN - pAttrs->extraBLmargin;
    NewH = WinH - 2 * MARGIN - pAttrs->extraBLmargin;

    // stretch the image to it's window determined size

    if ((NewH * cxImgSize) > (cyImgSize * NewW))
    {
      // window taller than image -- fit to width
      NewH = NewW * cyImgSize / cxImgSize;
      cxImgPos = MARGIN;
      cyImgPos = (WinH - NewH) / 2;
    }
    else
    {
      // window wider than image -- fit to height
      NewW = NewH * cxImgSize / cyImgSize;
      cyImgPos = MARGIN;
      if (pAttrs->flipped) cyImgPos += pAttrs->extraBLmargin;
      cxImgPos = (WinW - NewW) / 2;
    }
    // stash scale and translate parameters
    if (pAttrs->calcZoom) {
      pAttrs->tx = cxImgPos;
      pAttrs->ty = cyImgPos;
      pAttrs->zoom = NewW / (float)cxImgSize;
    }
  }
  else /* !bStretched */
  {
    // calculate the central position

    cxImgPos = (WinW - cxImgSize) / 2;
    cyImgPos = (WinH - cyImgSize) / 2;

    // check for image larger than window and set clipping

    if (cxImgPos < MARGIN)
      cxImgPos = MARGIN;
    if (cyImgPos < MARGIN)
      cyImgPos = MARGIN;

    NewW = WinW-2*MARGIN;
    NewH = WinH-2*MARGIN;

    // stash calculated zoom and translate parameters
    if (pAttrs->calcZoom) {
      pAttrs->tx = cxImgPos;
      pAttrs->ty = cyImgPos;
      pAttrs->zoom = 1.0;
    }

  }
  pAttrs->calcZoom = FALSE;


  // Choose a blitting function
  if (uniformType && (byteAligned || byteAlignedCol)) {
    pBlitFunc = g_blitFuncTable[blitFuncFlags][blitFuncIndex];
    pBlitNoStretchFunc =
      g_blitFuncTable[blitFuncFlags&~(BF_STRETCH)][blitFuncIndex];
  }

  if (!pBlitFunc)
  {
    // try to find other backup blit functions here...
  }  

  // blit image to DIB
  if (pBlitFunc) {
    int i;
    BOOL restoreScaleAndBias = FALSE;
    float saveScale[4];
    float saveBias[4];

    if (computeScaleAndBias) 
    {
      // compute the scaling first with a trial blitting w/ stats
      BlitStats bs;
      initBlitStats(&bs);
      // blitfuncs don't write to dst when passed a BlitStats, 
      // so it's safe & convenient to pretend dest "buffer" is same
      // size as src.
      pBlitNoStretchFunc(
        0,  pProps->width, pProps->width,
        pProps->width, pProps->height, 0, 0, // dst rect
        pbImage, pProps->width, srcSampleByteWidth,
        pProps->width, pProps->height, 0, 0, // src rect
        srcChanByteOffsets,
        pProps,
        &bs
        );
      // Save scale and bias:

      /*
       (actually this is really counterproductive.
       once we compute a good scale & bias we should continue to use it.
       no reason to recompute every time, since the image data isn't 
       changing.)
      restoreScaleAndBias = TRUE;
      for (i=0; i<4; i++) {
        saveScale[i] = pProps->scale[i];
        saveBias[i]  = pProps->bias[i];
      }
      */
      setScaleAndBias(&bs, typeMin, typeMax, pProps->scale, pProps->bias);
    }

    pBlitFunc(
      pDiData,  WinW, WinH, NewW, NewH, cxImgPos, cyImgPos,
      pbImage,
      pProps->width,
      srcSampleByteWidth,
      pProps->width, pProps->height, 0, 0,
      srcChanByteOffsets,
      pProps,
      0
      );

    if (restoreScaleAndBias) {
      // this puts back the AUTOSCALE indicators the way they were
      for (i=0; i<4; i++) {
        pProps->scale[i] = saveScale[i];
        pProps->bias[i]  = saveBias[i] ;
      }
    }
  }

  return (pBlitFunc != 0);
}

BOOL ClearBitmap (BYTE *pDiData, int WinW, int WinH, BOOL bChecker)
{
  BYTE *dst;
  int x, y, col;

  // initialize the background with

  dst = pDiData;
  for (y = 0; y < WinH; y++)
  {
    col = 0;
    for (x = 0; x < WinW; x++)
    {
      int v = ( !bChecker || (((y>>4)&1)^((x>>4)&1)) ) ? 127 : 255;

      // fill with GRAY or WHITE/GRAY checker 
      *dst++ = v;
      *dst++ = v;
      *dst++ = v;
      col += 3;
    }
    // rows start on 4 byte boundaries
    while ((col % 4) != 0)
    {
      dst++;
      col++;
    }
  }

  return TRUE;
}

static void DrawGrid(BYTE *pDiData, int WinW, int WinH, 
                     ImProps* pProps, DrawAttribs*pAttrs)
{
  int i,j;
  int StartX,StartY,EndX,EndY;
  int CurX,CurY;
  BYTE* dst;
  WORD wDIRowBytes;
  mapImageToWindow(0,0,&StartX,&StartY,pAttrs, pProps);
  mapImageToWindow(pProps->width,pProps->height,&EndX,&EndY,pAttrs, pProps);
  if (StartX<0)     StartX=0;  if (StartX>=WinW) StartX=WinW-1;
  if (StartY<0)     StartY=0;  if (StartY>=WinH) StartY=WinH-1;
  if (EndX<0)       EndX=0;    if (EndX>=WinW) EndX=WinW-1;
  if (EndY<0)       EndY=0;    if (EndY>=WinH) EndY=WinH-1;

  wDIRowBytes = (WORD) ((3 * WinW + 3L) >> 2) << 2;

  // horizontal lines
  for (j=0; j<=pProps->height; j++) 
  {
    mapImageToWindow(0,j,&CurX,&CurY,pAttrs, pProps);
    if (CurY<0 || CurY >= WinH) continue;
    dst = pDiData + wDIRowBytes * CurY + StartX * 3;
    for(i=StartX; i<=EndX; i++)
    {
      // Black line
      *dst++ = 0;
      *dst++ = 0;
      *dst++ = 0;
    }
  }

  // vertical lines
  for (j=0; j<=pProps->width; j++)
  {
    mapImageToWindow(j,0,&CurX,&CurY,pAttrs, pProps);
    if (CurX<0 || CurX>=WinW) continue;
    dst = pDiData + wDIRowBytes * StartY + CurX * 3;
    for(i=StartY; i<=EndY; i++)
    {
      // Black line
      dst[2] = 0;
      dst[1] = 0;
      dst[0] = 0;
      dst+=wDIRowBytes;
    }
  }


}


BOOL CreateDIBImage (
  HWND hwnd, 
  BYTE **ppDib, BYTE **ppDiData, int WinW, int WinH,
  ImProps *pProps, BYTE *pbImage,
  DrawAttribs *pAttrs)
{
  BYTE                       *pDib = *ppDib;
  BYTE                       *pDiData = *ppDiData;
  BITMAPINFOHEADER           *pbmih;
  WORD                        wDIRowBytes;
  BOOL                        bStretched = pAttrs->stretched;

  int ImgW =         pProps->width;
  int ImgH =         pProps->height;
  int cImgChannels = pProps->nchan;

  // Allocate memory for the RGB Device Independant bitmap
  // (always 3 channels, always rgb format for display)

  wDIRowBytes = (WORD) ((3 * WinW + 3L) >> 2) << 2;

  if (pDib)
  {
    free (pDib);
    pDib = NULL;
  }

  if (!(pDib = (BYTE *) malloc (sizeof(BITMAPINFOHEADER) +
                                wDIRowBytes * WinH)))
  {
    MessageBox (hwnd, TEXT ("Error in allocating memory for image"),
                "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
    *ppDib = pDib = NULL;
    return FALSE;
  }
  *ppDib = pDib;
  memset (pDib, 0, sizeof(BITMAPINFOHEADER));

  // initialize the dib-structure

  pbmih = (BITMAPINFOHEADER *) pDib;
  pbmih->biSize = sizeof(BITMAPINFOHEADER);
  pbmih->biWidth = WinW;
  pbmih->biHeight =  -((long) WinH);
  pbmih->biPlanes = 1;
  pbmih->biBitCount = 24;
  pbmih->biCompression = BI_RGB;
  pbmih->biSizeImage = wDIRowBytes*WinH; // shouldn't be necessary
  pDiData = pDib + sizeof(BITMAPINFOHEADER);
  *ppDiData = pDiData;

  if (pAttrs->flipped) {
    pbmih->biHeight *= -1L;
  }

  // first fill bitmap with background pattern (either gray or checker board)

  ClearBitmap (pDiData, WinW, WinH, pAttrs->checked);

  // then overlay the user's image

  if (pbImage)
  {
    int ret;
    ret = FillBitmap (pDiData, WinW, WinH, pProps, pbImage, pAttrs);
    if (!ret) return ret; // image format not supported

    if (pAttrs->grid && pAttrs->zoom >= 4) {
      DrawGrid(pDiData, WinW, WinH, pProps, pAttrs);
    }
  }

  return TRUE;
}

BOOL CreateDIBSectionImage (
  HWND hwnd,
  BYTE **ppDib, BYTE **ppDiData,
  HBITMAP *hBitmap, HDC *dibHDC,
  int WinW, int WinH,
  ImProps *pProps, BYTE *pbImage,
  DrawAttribs *pAttrs)
{
  BITMAPINFOHEADER           *pbmih;
  WORD                        wDIRowBytes;
  BOOL                        bStretched = pAttrs->stretched;

  int ImgW =         pProps->width;
  int ImgH =         pProps->height;
  int cImgChannels = pProps->nchan;

  wDIRowBytes = (WORD) ((3 * WinW + 3L) >> 2) << 2;

  // Allocate header if not already allocated

  if (!*ppDib &&
      !(*ppDib = (BYTE *) malloc (sizeof(BITMAPINFOHEADER))) )
  {
    MessageBox (hwnd, TEXT ("Error in allocating memory for image"),
                "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
    *ppDib = NULL;
    return FALSE;
  }
  memset (*ppDib, 0, sizeof(BITMAPINFOHEADER));

  // Initialize the header structure

  pbmih = (BITMAPINFOHEADER *) *ppDib;
  pbmih->biSize = sizeof(BITMAPINFOHEADER);
  pbmih->biWidth = WinW;
  pbmih->biHeight =  -((long) WinH);
  pbmih->biPlanes = 1;
  pbmih->biBitCount = 24;
  pbmih->biCompression = BI_RGB;
  pbmih->biSizeImage = wDIRowBytes*WinH; // shouldn't be necessary

  if (pAttrs->flipped) {
    pbmih->biHeight *= -1L;
  }

  // Now create the DIBSection (allocates image bits based on header info)
  if (*dibHDC) DeleteDC(*dibHDC);
  if (*hBitmap) DeleteObject(*hBitmap); // frees image's memory bits too
  {
    *dibHDC = CreateCompatibleDC(GetWindowDC(hwnd));
    if (!*dibHDC) {
      MessageBox (hwnd, TEXT ("Error in allocating memory DC for image"),
                  "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
    }
    *hBitmap = CreateDIBSection(*dibHDC, (BITMAPINFO*)pbmih, 
                                DIB_RGB_COLORS, ppDiData,
                                NULL, 0L);
    if (!*hBitmap) {
      MessageBox (hwnd, TEXT ("Error in allocating DIBSection for image"),
                  "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
    }
    // select the bmp into the memory dc so we can do gdi drawing on it
    SelectObject(*dibHDC, *hBitmap);
  }

  // first fill bitmap with background pattern (either gray or checker board)

  GdiFlush();
  ClearBitmap (*ppDiData, WinW, WinH, pAttrs->checked);

  // then overlay the user's image

  if (pbImage)
  {
    int ret;
    ret = FillBitmap (*ppDiData, WinW, WinH, pProps, pbImage, pAttrs);
    if (!ret) return ret; // image format not supported

    if (pAttrs->grid && pAttrs->zoom >= 4) {
      DrawGrid(*ppDiData, WinW, WinH, pProps, pAttrs);
    }
  }
  GdiFlush();

  return TRUE;
}


static ImProps* mapProps(LPVOID mapview)
{
  return (ImProps*)mapview;
}
static BYTE* mapImage(LPVOID mapview)
{
  return ((BYTE*)mapview)+sizeof(ImProps);
}

static int computeRawDataSize(ImProps *pProp)
{
  int imsize;

/*
  int nbits = 0;
  int lastBpc = 8;
  int i;

  for (i=0;i<pProp->nchan; i++) {
    int b = pProp->bpc[i];
    if (b<0) 
      nbits += lastBpc;
    else {
      nbits += b;
      lastBpc = b;
    }
  }
  imsize = (int)ceil((nbits * pProp->width*pProp->height)/8.0f);
*/
  imsize = (int)ceil((pProp->rowstride * pProp->height)/8.0f);
  return imsize;
}

static void copyRawImageData(LPVOID mapview, ImProps *pProp, BYTE **rawData)
{
  int rawSize;

  memcpy(pProp, mapProps(mapview), sizeof(ImProps));

  rawSize = computeRawDataSize(pProp);

  // Free old data
  if (*rawData != NULL) {
    free (*rawData);
    *rawData = NULL;
  }

  // Alloc new data
  if (pProp->nchan < 0) { // special signal
    *rawData = 0;
    pProp->nchan = ERROR_NULL_POINTER;
  }
  else if (rawSize == 0) {
    *rawData = 0;
    pProp->nchan = ERROR_ZERO_PIXELS;
  }
  else if (rawSize+sizeof(ImProps) > IMDBG_MAX_DATA) {
    // too big to hanlde
    *rawData = 0;
    pProp->nchan = ERROR_TOO_BIG;
  }
  else {
    *rawData = (BYTE*)malloc(rawSize);
    if (!*rawData) {
      MessageBox (GetActiveWindow(), 
                  TEXT ("Error in allocating memory for image"),
                  "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
      pProp->nchan = ERROR_ALLOC_FAIL;
      return;
    }
    memcpy(*rawData, mapImage(mapview), rawSize);
  }
}



static BOOL convertToDeviceBitmap(
  HWND hwnd, ImProps *pProps, BYTE *rawImage, 
  BYTE **ppDib, BYTE **ppDiData, 
  int WinW, int WinH, DrawAttribs *pAttrs)
{
  return CreateDIBImage (hwnd, ppDib, ppDiData, WinW, WinH,
                         pProps, rawImage, pAttrs);
}


static BOOL refreshDeviceBitmapSection(
  HWND hwnd, ImProps *pProps, BYTE *rawImage, 
  BYTE **ppDib, BYTE **ppDiData, 
  HBITMAP *hBitmap, HDC *dibDC,
  int WinW, int WinH, DrawAttribs *pAttrs)
{
  if (WinW <= 0 || WinH <= 0) {
    return 1;
  }

  return CreateDIBSectionImage (hwnd, ppDib, ppDiData, 
                                hBitmap, dibDC,
                                WinW, WinH,
                                pProps, rawImage, pAttrs);
}

// Moves and updates statusbar; returns height of statusbar
static int moveStatusBar(HWND hwndStatusBar, int WinW, int WinH)
{
  RECT rWindow;
  int x, y, cx, cy;
  GetWindowRect(hwndStatusBar, &rWindow);
  cy = rWindow.bottom - rWindow.top;
  cx = WinH;
  x=0;
  y = WinH - cy;
  MoveWindow(hwndStatusBar, x, y, cx, cy, TRUE);

  // resize parts too
  {
    int WID[] = { /*WinW/5,*/ (int)((2.2f/3.0f)*WinW), -1 };
    SendMessage(hwndStatusBar, SB_SETPARTS, (WPARAM)2, (LPARAM)WID);
  }

  return cy;
}


static BOOL getValueOfTypeAndSize(BYTE *p, char t, int s, int off, 
                                  long* lv, double *dv)
{
  //if (s&7) return FALSE; // eek! non-integral byte width size!
  switch(t) {
  case IMDBG_UINT:
    switch(s) {
    case 8:   *lv = *((unsigned char*)p); return TRUE;
    case 16:  *lv = *((unsigned short*)p); return TRUE;
    case 32:  *lv = *((unsigned int*)p); return TRUE;
      // case 64:  *lv = *((unsigned long*)p); return TRUE;
    default: 
      *lv = bytePtr2Val_BitOffsets(p, off, s);
      return TRUE;
    }
    break;
  case IMDBG_INT:
    switch(s) {
    case 8:   *lv = *((char*)p); return TRUE;
    case 16:  *lv = *((short*)p); return TRUE;
    case 32:  *lv = *((int*)p); return TRUE;
    case 64:  *lv = *((long*)p); return TRUE;
    default: return FALSE;
    }
    break;
  case IMDBG_FLOAT:
    switch(s) {
    case 32:  *dv = *((float*)p); return TRUE;
    case 64:  *dv = *((double*)p); return TRUE;
    default: return FALSE;
    }
    break;
  }
  return FALSE;
}

static void setImageCountStatus(HWND hwndStatusBar, 
                                int cur, int first, int last,
                                int total, int saved, float zoom)
{
  // set statusbar message
  char msg[100];
  int disp = total + ((cur <= last) ? (cur-last) : (cur-last-saved) );
  sprintf(msg, "%d / %d  %1.1fx", disp, total, zoom);
  SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)1, (LPARAM)msg);
}


static BOOL getImageDataStatusInfo(
  HWND hwndStatusBar, DrawAttribs *pAttr, ImProps *pprops,
  BYTE *pRawData,
  int PtrX, int PtrY, int WinW, int WinH,
  BOOL isChangingScaleBias,
  StatusMessageInfo *retInfo
  )
{
  char msg[100] = "Ready";
  int xImg, yImg;

  if (isChangingScaleBias) {
    sprintf(msg, "*%g %+g", pprops->scale[0], pprops->bias[0]);
    SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)msg);
    return FALSE;
  }
  // determine if we're over the scrollbar
  {
    HWND hwnd = GetParent(hwndStatusBar);
    RECT r;
    int cw = SCALE_BIAS_CONTROL_WIDTH;
    POINT p = { PtrX, PtrY };
    GetPaintableRect(&r, hwnd, hwndStatusBar);
    if (!PtInRect(&r, p)) {
      RECT sbr;
      BOOL msgset = FALSE;
      if (SetRect(&sbr, 0 ,r.bottom, r.right-cw, r.bottom+cw) &&
          PtInRect(&sbr, p)) 
      {
        sprintf(msg, "Drag left and right to bias image values");
        msgset = TRUE;
      }
      else if (SetRect(&sbr, r.right, 0, r.right+cw, r.bottom) &&
               PtInRect(&sbr, p)) 
      {
        sprintf(msg, "Drag up and down to scale image values");
        msgset = TRUE;
      }
      else if (SetRect(&sbr, r.right-cw, r.bottom, r.right, r.bottom+cw) &&
               PtInRect(&sbr, p)) 
      {
        sprintf(msg, "Click for unity scale, zero bias on image values");
        msgset = TRUE;
      }
      else if (SetRect(&sbr, r.right, r.bottom, r.right+cw, r.bottom+cw) &&
               PtInRect(&sbr, p)) 
      {
        sprintf(msg, "Click to autoscale image values");
        msgset = TRUE;
      }
      if (msgset) {
        SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)msg);
        return FALSE;
      }
    }
  }

  mapWindowToImage(PtrX,PtrY,WinH,&xImg,&yImg,pAttr,pprops);
  if (PtrX > 0 && xImg>=0 && xImg < pprops->width &&
      PtrY > 0 && yImg>=0 && yImg < pprops->height && pRawData!=0) 
  {
    StatusMessageInfo *smsg = retInfo;
    smsg->ImgX = xImg;
    smsg->ImgY = yImg;
    smsg->WinW = WinW;
    smsg->WinH = WinH;
    smsg->PtrX = PtrX;
    smsg->PtrY = PtrY;
    smsg->pImprops = pprops;
    smsg->pDrawAttrs = pAttr;
    smsg->pRawData = pRawData;
    
    return TRUE;
  }        
  else {
    // nothing particular to show... so show image stats
    int l=0,i;
    int bpc=pprops->bpc[0];
    int type=pprops->type[0];
    
    l += sprintf(msg+l, "%dx%d, %d-channel, ", pprops->width, pprops->height, pprops->nchan);
    
    for (i=0;i<pprops->nchan; i++) {
      if (pprops->bpc[i] != bpc) bpc = -1;
      if (pprops->type[i] != type) type = -1;
    }
    if (bpc>=0) {
      l += sprintf(msg+l, "%d-bit ", bpc);
    }
    else {
      l += sprintf(msg+l, "(%d", pprops->bpc[0]);
      for (i=1;i<pprops->nchan; i++) {
        l += sprintf(msg+l, ",%d", pprops->bpc[i]);
      }
      l += sprintf(msg+l, ")-bit ", bpc);
    }
    if (type>=0) {
      switch (type) {
      case IMDBG_FLOAT:
        l += sprintf(msg+l, "float ", bpc);
        break;
      }
    } else {
      l += sprintf(msg+l, "mixed-type ", bpc);
    }
    l += sprintf(msg+l, "image");
    SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)msg);
    return FALSE;
  }
}
static void drawCustomStatusBarError(HWND hwnd, DRAWITEMSTRUCT *ds, int err)
{
  char msg[100] = "Unknown Error";
  int l;
  RECT rect = ds->rcItem;
  // either zero size or too big or failure to alloc
  if (err == ERROR_ZERO_PIXELS) {
    strcpy(msg, "Image has zero pixels");
  }
  else if (err == ERROR_TOO_BIG) {
    strcpy(msg, "Error: Image too big");
  }
  else if (err == ERROR_ALLOC_FAIL) {
    strcpy(msg, "Error: Out of memory");
  }
  else if (err == ERROR_NULL_POINTER) {
    strcpy(msg, "Error: Image is null pointer");
  }
  else if (err == ERROR_NO_IMAGE) {
    strcpy(msg, "No image");
  }
  l = strlen(msg);
  rect.left+=2;
  SetBkMode(ds->hDC, TRANSPARENT);
  DrawText(ds->hDC, msg, l, &rect , DT_SINGLELINE|DT_VCENTER);
}

static void drawCustomStatusBar(HWND hwnd, DRAWITEMSTRUCT *ds, 
                                StatusMessageInfo *pInfo )
{
  SIZE sz;
  ImProps *p  = pInfo->pImprops;
  DrawAttribs *attr = pInfo->pDrawAttrs;
  BYTE *pRawData = pInfo->pRawData;
  RECT rect = ds->rcItem;
  char msg[100];
  int l, c;
  BYTE *base;
  COLORREF colormap[] = {
    RGB(180,50,50),
    RGB(30,140,30),
    RGB(50,50,200),
    RGB(100,100,100),
    RGB(0,0,0)
  };
  char colorchar[] = "RGBA";

  SetBkMode(ds->hDC, TRANSPARENT);
  rect.left += 2;

  l = sprintf(msg, "(%d, %d)", pInfo->ImgX, pInfo->ImgY);

  if (!(p->rowstride&7)&&!(p->colstride&7) && p->nchan>0) 
  {
    int colors[4];
    int numcolors = 0;
    int lastnumcolors = 0;
    BOOL isGrayScale;
    char effectiveChanmap[4];
    memcpy(effectiveChanmap, p->chanmap, 4*sizeof(char));
    if (! (attr->enabledChannels & IMDBG_ENABLE_RED))
      effectiveChanmap[0] = -1;
    if (! (attr->enabledChannels & IMDBG_ENABLE_GREEN))
      effectiveChanmap[1] = -1;
    if (! (attr->enabledChannels & IMDBG_ENABLE_BLUE))
      effectiveChanmap[2] = -1;
    if (! (attr->enabledChannels & IMDBG_ENABLE_ALPHA))
      effectiveChanmap[3] = -1;
    // Make alpha grayscale if that's all there is
    if (attr->enabledChannels == IMDBG_ENABLE_ALPHA) {
      effectiveChanmap[0] = effectiveChanmap[3];
      effectiveChanmap[1] = effectiveChanmap[3];
      effectiveChanmap[2] = effectiveChanmap[3];
      effectiveChanmap[3] = -1;
    }


    isGrayScale = p->nchan == 1 && 
      effectiveChanmap[0] == effectiveChanmap[1] &&
      effectiveChanmap[1] == effectiveChanmap[2];

    // byte-aligned memory, we can handle it, so print values
    l += sprintf(msg+l, " = [");
    base = pRawData;
    base += (p->rowstride>>3)*pInfo->ImgY + (p->colstride>>3)*pInfo->ImgX;
    for (c = 0; c<p->nchan; c++)
    {
      long lv;
      double dv;
      if (getValueOfTypeAndSize( base+(p->bitoffset[c]>>3),
                                 p->type[c],p->bpc[c],
                                 p->bitoffset[c]&0x7, &lv, &dv )
          )
      {
        numcolors = 0;
        if (c == effectiveChanmap[0]) {
          colors[numcolors++] = 0; //red
        }
        if (c == effectiveChanmap[1]) {
          colors[numcolors++] = 1; //green
        }
        if (c == effectiveChanmap[2]) {
          colors[numcolors++] = 2; //blue
        }
        if (c == effectiveChanmap[3]) {
          colors[numcolors++] = 3; //alpha
        }
        if (numcolors==0) {
          numcolors=1;
          colors[0] = 4;  // black
        }
        {
          DrawText(ds->hDC, msg, l, &rect, DT_SINGLELINE|DT_VCENTER);
          GetTextExtentPoint32(ds->hDC, msg, l, &sz);
          rect.left += sz.cx;
          l=0;
          if (numcolors==1 && !isGrayScale) {
            SetTextColor(ds->hDC, colormap[colors[0]]);
          }
          else if (numcolors>1 && !isGrayScale)
          {
            int k;
            for (k=0; k<numcolors; k++) {
              //SetBkMode(ds->hDC, OPAQUE);
              //SetBkColor(ds->hDC, colormap[colors[k]]);
              SetTextColor(ds->hDC, colormap[colors[k]]);
              DrawText(ds->hDC, colorchar+colors[k], 1, 
                       &rect, DT_SINGLELINE|DT_VCENTER);
              GetTextExtentPoint32(ds->hDC, colorchar+colors[k], 1, &sz);
              rect.left += sz.cx;
            }
            rect.left += 2;
            // SetBkMode(ds->hDC, TRANSPARENT);
            SetTextColor(ds->hDC, RGB(0,0,0));
          }
          else {
            SetTextColor(ds->hDC, RGB(0,0,0));
          }
        }

        if (p->type[c]==IMDBG_UINT||p->type[c]==IMDBG_INT) {
          l += sprintf(msg+l, "%ld, ", lv);
        }
        else if (p->type[c]==IMDBG_FLOAT) {
          l += sprintf(msg+l, "%g, ", dv);
        }
        else {
          l += sprintf(msg+l, "?, ");
        }
      }
      else {
        l += sprintf(msg+l, "?, ");
      }
      lastnumcolors = numcolors;
    }
    l-=2;
    {
      DrawText(ds->hDC, msg, l, &rect, DT_SINGLELINE|DT_VCENTER);
      GetTextExtentPoint32(ds->hDC, msg, l, &sz);
      rect.left += sz.cx;
      l=0;
      SetTextColor(ds->hDC, RGB(0,0,0));
    }

    l += sprintf(msg+l,"]");

    // draw scale and bias too
    // FIXME IF PER-CHANNEL SCALE & BIAS IS IMPLEMENTED!!!
    if (p->scale[0] != 1.0) {
      l += sprintf(msg+l," *%g", p->scale[0]);
    }
    if (p->bias[0] != 0) {
      l += sprintf(msg+l," %+g", p->bias[0]);
    }
  }

  if (l>0) {
    DrawText(ds->hDC, msg, l, &rect, DT_SINGLELINE|DT_VCENTER);
  }
}

static void updateWindowTitle(HWND wnd, const char *imtitle)
{
  char base[] = IMDBG_DISPLAY_WINDOW_TITLE;
  if (imtitle && imtitle[0]) {
    char buf[25+MAX_TITLE_LENGTH];
    _snprintf(buf,25+MAX_TITLE_LENGTH, "%s - %s", imtitle, base);
    SetWindowText(wnd, buf);
  }
  else {
    SetWindowText(wnd, base);
  }
}

static BOOL isViewMenuChecked(HMENU hMenu, UINT uId)
{
  // wow I'm amazed how much work it is just to get the 
  // state of a menu item... and the hard coded 2 !! Yikes!
  HMENU subMenu;
  UINT uFlags;
  const UINT VIEW_MENU_ID = 2; /* MAGIC HARD CODED VALUE! */
  subMenu = GetSubMenu(hMenu, VIEW_MENU_ID);
  uFlags = GetMenuState(subMenu, uId, MF_BYCOMMAND);
  return (uFlags & MF_CHECKED)?TRUE:FALSE;
}
static BOOL isChannelMenuChecked(HMENU hMenu, UINT uId)
{
  // wow I'm amazed how much work it is just to get the 
  // state of a menu item... and the hard coded 2 !! Yikes!
  HMENU subMenu;
  UINT uFlags;
  const UINT CHAN_MENU_ID = 3; /* MAGIC HARD CODED VALUE! */
  subMenu = GetSubMenu(hMenu, CHAN_MENU_ID);
  uFlags = GetMenuState(subMenu, uId, MF_BYCOMMAND);
  return (uFlags & MF_CHECKED)?TRUE:FALSE;
}


static void doZoom(int dirn, int xWin, int yWin, DrawAttribs *pAttrs)
{
  float factor = 1.2f;
  float z = pAttrs->zoom;

  if (dirn > 0) {
    if (z > (1.f/(factor+0.01f)) && z < 1.0f)  {
      z = 1.0f/(float)pAttrs->zoom;
      pAttrs->stretched = FALSE;
    }
    else {
      z = factor;
      pAttrs->stretched = TRUE;
    }
  }
  else {
    if (z < (factor+0.01) && z > 1.0f) {
      z = 1.0f/(float)pAttrs->zoom;
      pAttrs->stretched = FALSE;
    }
    else {
      z = 1.0f/factor;
      pAttrs->stretched = TRUE;
    }
  }

  pAttrs->tx = (int)(xWin + (pAttrs->tx-xWin)*z);
  pAttrs->ty = (int)(yWin + (pAttrs->ty-yWin)*z);
  pAttrs->zoom *= z;
}


LRESULT CALLBACK WndProc (HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
  static HWND               hwndStatusBar = NULL;

  static HANDLE             pFileMap = NULL;
  static LPVOID             pMapView = NULL;

  static HINSTANCE          hInstance ;
  //static int _protect2[32];
  static BYTE              *pCurDib = NULL;
  static BYTE              *pCurDiData = NULL;
  static HDC               hCurDibDC = NULL;
  static HBITMAP           hCurDibBitmap = NULL;

  static BYTE              **pRawData = NULL;
  static ImProps            *improps = NULL;
  static int                WinW, WinH;
  static DrawAttribs        drawAttr;

  static int                refreshPending = 1;

  static int                imagesDisplayed = 0;
  static int                numImages = 0;
  static int                curImage = 0;
  static int                firstImage = 0;
  static int                lastImage = 0;

  static int                lastPtrWinX = 0;
  static int                lastPtrWinY = 0;

  static POINT ptBeg = {0,0};
                                  
  static int                dragMode = IMDM_TRANSLATE;

  switch (iMsg)
  {
  case WM_CREATE :
  { 
    hInstance = ((LPCREATESTRUCT) lParam)->hInstance ;

    createFileMapping(&pFileMap, &pMapView);

    // Read info from registry
    loadRegistryPrefs(hwnd);

    // Allocate & init history buffers
    pRawData = (BYTE**)  malloc(sizeof(BYTE*)  *g_historySize);
    improps  = (ImProps*)malloc(sizeof(ImProps)*g_historySize);
    ZeroMemory(pRawData, g_historySize*sizeof(BYTE*));
    ZeroMemory(improps,  g_historySize*sizeof(ImProps));

    initDrawAttribs(&drawAttr);
    FileInitialize(hwnd);
    {
      // STATUS BAR SETUP
      hwndStatusBar = CreateStatusWindow( 
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | CCS_BOTTOM |
        SBARS_SIZEGRIP,
        "Ready", hwnd, 288/*just an id*/);
    }
    // make sure all the draw attribs match menu states
    getDrawAttrsFromMenuState(GetMenu(hwnd), &drawAttr);
 /*   
    {
      TRACKMOUSEEVENT mev;
      mev.cbSize = sizeof(mev);
      mev.dwFlags = TME_LEAVE;
      mev.hwndTrack = hwnd;
      mev.dwHoverTime = HOVER_DEFAULT;
      _TrackMouseEvent(&mev);
    }
*/
  }
  return 0 ;

  case WM_SIZE:
    WinW = LOWORD (lParam);
    WinH = HIWORD (lParam);

    // move status bar (returns status height)
    WinH -= moveStatusBar(hwndStatusBar, WinW, WinH);

    // invalidate the client area for later update

    InvalidateRect (hwnd, NULL, TRUE);

    // draw the current image into the DIBitmap
    refreshPending++;
      
    return 0;

       
  case /*WM_MOUSEWHEEL*/0x020A:
  {
    int scroll = (short)HIWORD(wParam);
    POINT win;

    scroll /= /*WHEEL_DELTA*/120;
    win.x = LOWORD (lParam) ;
    win.y = HIWORD (lParam) ;

    if (wParam & MK_CONTROL) {

      if (scroll > 0) {
        if ( curImage!=firstImage ) --curImage;
        if (curImage<0) curImage = g_historySize-1;
      }
      else {
        // switch to next output image
        if (  curImage != lastImage )
          ++curImage; curImage%=g_historySize;
      }

      InvalidateRect(hwnd, NULL, TRUE);
      drawAttr.calcZoom=TRUE;
      drawAttr.stretched = isViewMenuChecked(GetMenu(hwnd), IDM_VIEW_STRETCH);
      refreshPending++;

      setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                          imagesDisplayed, g_historySize, drawAttr.zoom);
    }
    else {
      ScreenToClient(hwnd, &win);

      doZoom(scroll, win.x, win.y, &drawAttr);
    }

    InvalidateRect (hwnd, NULL, TRUE);

    setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                        imagesDisplayed, g_historySize, drawAttr.zoom);
    lastPtrWinX = win.x;
    lastPtrWinY = win.y;
    refreshPending++;
  }
  return 0 ;

  case WM_LBUTTONDOWN :
    {
      RECT userRect;
      int wR, wB;
      BOOL sbCtrls = FALSE;
      int cw = SCALE_BIAS_CONTROL_WIDTH;
      sbCtrls = isViewMenuChecked(GetMenu(hwnd), IDM_VIEW_SB_CONTROLS);
      GetPaintableRect(&userRect, hwnd, hwndStatusBar);
      wR = userRect.right;
      wB = userRect.bottom;
      ptBeg.x = LOWORD (lParam) ;
      ptBeg.y = HIWORD (lParam) ;
      if (sbCtrls && ptBeg.x > wR && ptBeg.y < wB)
          dragMode = IMDM_SCALE;
      else if (sbCtrls && ptBeg.x < wR-cw && ptBeg.y > wB ) 
          dragMode = IMDM_BIAS;
      else if (sbCtrls && ptBeg.x > wR-cw && ptBeg.x <= wR && ptBeg.y > wB)
          dragMode = IMDM_UNITYSCALE;
      else if (sbCtrls && ptBeg.x > wR && ptBeg.y > wB)
          dragMode = IMDM_AUTOSCALE;
      else
          dragMode = IMDM_TRANSLATE;
      SetCapture(hwnd);
    }
    return 0 ;

  case WM_LBUTTONUP :
    if (GetCapture()==hwnd && dragMode == IMDM_AUTOSCALE) {
      float *s = &improps[curImage].scale[0];
      s[0] = s[1] = s[2] = IMDBG_AUTOSCALE;
      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
    }
    else if (GetCapture()==hwnd && dragMode == IMDM_UNITYSCALE) {
      float *s = &improps[curImage].scale[0];
      float *b = &improps[curImage].bias[0];
      s[0] = s[1] = s[2] = 1.0f;
      b[0] = b[1] = b[2] = 0.0f;
      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
    }
    ReleaseCapture();
    return 0 ;

  case WM_MOUSEMOVE:
  {
    int xWin, yWin;
    
    xWin = GET_X_LPARAM(lParam); 
    yWin = GET_Y_LPARAM(lParam); 
    {
      static TRACKMOUSEEVENT mev;
      mev.cbSize = sizeof(mev);
      mev.dwFlags = TME_LEAVE;
      mev.hwndTrack = hwnd;
      mev.dwHoverTime = HOVER_DEFAULT;
      _TrackMouseEvent(&mev);
    }

    if (wParam & MK_LBUTTON && GetCapture()==hwnd )
    {
      if (dragMode == IMDM_TRANSLATE) {
        drawAttr.tx += xWin - ptBeg.x;
        drawAttr.ty += (yWin - ptBeg.y) * (drawAttr.flipped?-1:1);
      }
      else if (dragMode == IMDM_SCALE) {
        float s = improps[curImage].scale[0];
        float *srgba = &improps[curImage].scale[0];
        s *= (float)pow(1.007, ptBeg.y - yWin );
        srgba[0]=srgba[1]=srgba[2]=s;
      }
      else if (dragMode == IMDM_BIAS) {
        float b = improps[curImage].bias[0];
        float *brgba = &improps[curImage].bias[0];
        b += (float) 1.0/255.0f*drawAttr.lastTypeMax*improps[curImage].scale[0] * ( xWin - ptBeg.x );
        brgba[0]=brgba[1]=brgba[2]=b;
      }
      ptBeg.x = xWin;
      ptBeg.y = yWin;
      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
    }
    else if (wParam & MK_RBUTTON) {
      // leaving this available for future context menu
    }
    else {
      if (improps[curImage].nchan > 0) {
        SendMessage(hwndStatusBar, SB_SETTEXT, SBT_OWNERDRAW, (LPARAM)0);
      }
    }
    lastPtrWinX = xWin;
    lastPtrWinY = yWin;
  }
  return 0;

  case WM_MOUSELEAVE:
    if (improps[curImage].nchan > 0) {
      SendMessage(hwndStatusBar, SB_SETTEXT, SBT_OWNERDRAW, (LPARAM)0);
      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
    }
    lastPtrWinX = -1;
    lastPtrWinY = -1;
    return 0;

  case WM_CHAR :
    //InvalidateRect (hwnd, NULL, TRUE);
    return 0 ;

  case WM_PAINT :
  {
    BOOL ret = TRUE;
    int cw = isViewMenuChecked(GetMenu(hwnd), IDM_VIEW_SB_CONTROLS) ? SCALE_BIAS_CONTROL_WIDTH : 0;
    
    if (refreshPending) {

      // draw the current image into the DIBitmap
      ret = refreshDeviceBitmapSection(
        hwnd, &improps[curImage], pRawData[curImage],
        &pCurDib, &pCurDiData, 
        &hCurDibBitmap, &hCurDibDC,
        WinW, WinH, &drawAttr);
      if (!ret) {
        // we had trouble converting image
        SendMessage(hwndStatusBar, SB_SETTEXT, 
                    (WPARAM)0, (LPARAM)__TEXT("Unsupported image format"));
      } else {
        SendMessage(hwndStatusBar, SB_SETTEXT, SBT_OWNERDRAW, (LPARAM)0);
      }

      // Draw scale and bias controls if they're on
      if (ret && cw) 
      {
        HBITMAP imgL, imgR, imgU, imgD, imgA, img1, imgbg;
        RECT userRect;
        HDC srcDC;
        int wR, wB;
        GetPaintableRect(&userRect, hwnd, hwndStatusBar);
        wR = userRect.right;
        wB = userRect.bottom;

        imgL = LoadImage(hInstance, MAKEINTRESOURCE(IDB_LFARROW), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
        imgR = LoadImage(hInstance, MAKEINTRESOURCE(IDB_RTARROW), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
        imgU = LoadImage(hInstance, MAKEINTRESOURCE(IDB_UPARROW), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
        imgD = LoadImage(hInstance, MAKEINTRESOURCE(IDB_DNARROW), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
        imgA = LoadImage(hInstance, MAKEINTRESOURCE(IDB_AUTOSCALE), IMAGE_BITMAP, 0,0,LR_DEFAULTCOLOR);
        img1 = LoadImage(hInstance, MAKEINTRESOURCE(IDB_UNITYSCALE), IMAGE_BITMAP, 0,0,LR_DEFAULTCOLOR);
        imgbg = LoadImage(hInstance, MAKEINTRESOURCE(IDB_SCROLLBG), IMAGE_BITMAP, 0,0,LR_DEFAULTCOLOR);
        srcDC = CreateCompatibleDC(GetWindowDC(hwnd));

        SelectObject(srcDC, imgL);
        BitBlt(hCurDibDC, 0,wB,     WinW, WinH, srcDC, 0, 0, SRCCOPY);
        SelectObject(srcDC, imgR);
        BitBlt(hCurDibDC, wR-cw-cw,wB, WinW, WinH, srcDC, 0, 0, SRCCOPY);
        SelectObject(srcDC, imgU);
        BitBlt(hCurDibDC, wR,0,     WinW, WinH, srcDC, 0, 0, SRCCOPY);
        SelectObject(srcDC, imgD);
        BitBlt(hCurDibDC, wR,wB-cw, WinW, WinH, srcDC, 0, 0, SRCCOPY);
        SelectObject(srcDC, img1);
        BitBlt(hCurDibDC, wR-cw,wB, WinW, WinH, srcDC, 0, 0, SRCCOPY);
        SelectObject(srcDC, imgA);
        BitBlt(hCurDibDC, wR,wB,    WinW, WinH, srcDC, 0, 0, SRCCOPY);

        DeleteDC(srcDC);
        DeleteObject(imgL);
        DeleteObject(imgR);
        DeleteObject(imgU);
        DeleteObject(imgD);
        DeleteObject(imgA);
        DeleteObject(img1);

        {
          RECT rect;
          HBRUSH scrollTrackBGBrush = CreatePatternBrush(imgbg);
          SetRect(&rect, cw, wB, wR-cw-cw, wB+cw);
          FillRect(hCurDibDC, &rect, scrollTrackBGBrush);
          SetRect(&rect, wR, cw, wR+cw, wB-cw);
          FillRect(hCurDibDC, &rect, scrollTrackBGBrush);
        }
        DeleteObject(imgbg);

        GdiFlush();
      }

      updateWindowTitle(hwnd, improps[curImage].title);
      refreshPending = 0;
    }
    
    if (pCurDib) {
      PAINTSTRUCT ps ;
      HDC hdc = BeginPaint (hwnd, &ps) ;
      BitBlt(hdc, 0, 0, WinW, WinH, hCurDibDC, 0, 0, SRCCOPY);
      EndPaint (hwnd, &ps) ;
    }

  }
  return 0 ;


  case WM_APP:
    if (wParam == APPM_IMAGE_READY &&
        !isViewMenuChecked(GetMenu(hwnd), IDM_VIEW_IGNORE)) 
    {
      //BOOL ret;
      // WE GOT A NEW IMAGE DISPLAY REQUEST
      
      if (numImages != 0) {
        ++lastImage; lastImage %= g_historySize;
        if (firstImage==lastImage) {
          // circular buf is full we're throwing one away
          if (curImage == firstImage) {
            ++curImage; curImage %= g_historySize;
          }
          ++firstImage; firstImage %= g_historySize;
        }
        else {
          numImages++;
        }
      } else {
        lastImage = firstImage = 0;
        numImages++;
      }
      // do we want a mode where curImage doesn't automatically
      // jump to each new image?
      curImage = lastImage;
      
      copyRawImageData(pMapView, &improps[lastImage], &pRawData[lastImage]);
      if (improps[lastImage].VERSION != IMDBG_PROTOCOL_VERSION)
      {
        // THIS EXE DOESN'T MATCH THE DLL BEING USED!!! BAIL OUT!!!
        if ( improps[lastImage].VERSION > IMDBG_PROTOCOL_VERSION ) {
          MessageBox (hwnd, 
                      TEXT ("Error: Version mismatch.  "
                            "The DLL you are linking with is"
                            " more recent than this executable. "
                            "Terminating."),
                      "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
        } else {
          MessageBox (hwnd, 
                      TEXT ("Error: Version mismatch.  "
                            "This exe is more recent than"
                            " the DLL you are linking with. "
                            "Terminating."),
                      "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
        }
        destroyFileMapping(&pMapView, &pFileMap);
        saveRegistryPrefs(hwnd);

        exit(0);
      }

      // possibly copy scale from previous tagged image of same tag
      if (g_matchScaleOnTagged &&
          improps[lastImage].title[0] != 0 &&
          !(improps[lastImage].flags & (IMDBG_FLAG_AUTOSCALE|
                                        IMDBG_FLAG_SCALE_SET|
                                        IMDBG_FLAG_BIAS_SET)) )
      {
        // search for a matching tag
        int idx = lastImage-1;
        BOOL match = FALSE;
        if (idx < 0) idx = g_historySize-1;
        while( idx != lastImage) {
          if ( !strcmp(improps[idx].title, improps[lastImage].title) )
          {
            match = TRUE;
            break;
          }
          idx--;
          if (idx < 0) idx = g_historySize-1;
        }
        if (match) {
          // copy scale and bias settings of that image
          int ch;
          for (ch=0;ch<4;ch++) {
            improps[lastImage].scale[ch] = improps[idx].scale[ch];
            improps[lastImage].bias[ch]  = improps[idx].bias[ch];
          }
        }
      }

      drawAttr.calcZoom = TRUE;
      refreshPending++;

      imagesDisplayed++;

      setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                          imagesDisplayed, g_historySize, drawAttr.zoom);

      updateWindowTitle(hwnd, improps[lastImage].title);

      InvalidateRect (hwnd, NULL, TRUE);
    }
    return 0;

  case WM_COMMAND:
  {
    HMENU hMenu = GetMenu (hwnd);

    switch (LOWORD (wParam))
    {
    case IDM_EDIT_COPY:
    {
      // Prepare 1-1 copy of what's on the screen in a new DIB
      // NOTE: Adobe Photoshop didn't like the format of this clipboard
      //       and refused to import it.  But WORD had no problem, and
      //       even lowly MS Paint could recognize it.
      //       I now seem to have fixed the problem by padding the 
      //       clipboard buffer, and by not passing in negative height
      //       (which is supposed to mean "flip the image").  I dunno
      //       why that seems to fix it or if it even does fix it 
      //       generally, but it worked for me.
      BYTE *pDib = NULL;
      BYTE *pDiData = NULL;
      BOOL ret;
      DrawAttribs da;
      initDrawAttribs(&da);
      da.stretched = FALSE;
      da.margin = 0;
      da.flipped = drawAttr.flipped; // this is irrelavent...

      ret = convertToDeviceBitmap(
        hwnd, &improps[curImage], pRawData[curImage],&pDib, &pDiData,
        improps[curImage].width, improps[curImage].height, &da);
      if (ret) // it worked!
      {
        WORD rowBytes = (WORD) ((3 * improps[curImage].width + 3L) >> 2) << 2;
        int dataSize = rowBytes * improps[curImage].height;
        int dibSize = sizeof(BITMAPINFOHEADER) + dataSize;
        HANDLE clipHandle;

        HANDLE hnd;
        BYTE* dataCopy;

        if (OpenClipboard(hwnd)) {
          EmptyClipboard();
          // It seems to help to pad the data some, and to
          // not use the negative height value.  dunno why.
          // this clipboard stuff is baffling.
          hnd = GlobalAlloc(GHND,dibSize+128);
          ((BITMAPINFOHEADER*)pDib)->biHeight *= -1;
          dataCopy = (BYTE*)GlobalLock(hnd);
          memcpy(dataCopy, pDib, dibSize);
          //memcpy(dataCopy, &bmi, sizeof(BITMAPINFO));
          //memcpy(dataCopy+sizeof(BITMAPINFO), pDiData, dataSize);
          GlobalUnlock(hnd);
          
          clipHandle = SetClipboardData(CF_DIB, hnd);
          CloseClipboard();
        }
/*
        if (!clipHandle)
        {
          LPVOID lpMsgBuf;
          FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                         FORMAT_MESSAGE_FROM_SYSTEM,
                         NULL,    GetLastError(),
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                         // Default language
                         (LPTSTR) &lpMsgBuf,    0,    NULL );
          chMB((const char*)lpMsgBuf);
          LocalFree( lpMsgBuf );
        }
*/

      }
      if (pDib) free(pDib);
      
    }
    return 0;

    case IDM_FILE_SAVE:
      // show the File Save dialog box
    {
      static char      szFileName[_MAX_PATH] ;
      static char      szTitleName[_MAX_FNAME + _MAX_EXT] ;

      if (FileSaveDlg (hwnd, szFileName, szTitleName))
      {
        if (!FileWrite (hwnd, szFileName))
        {
          MessageBox (hwnd, TEXT ("Could not write file!"),
                      "imdbgdisplay", MB_ICONEXCLAMATION | MB_OK);
        }
      }
    }
    return 0;

    case IDM_VIEW_NEXT:
      // switch to next output image
      if (  curImage != lastImage )
        ++curImage; curImage%=g_historySize;
      // draw the current image into the DIBitmap

      InvalidateRect(hwnd, NULL, TRUE);
      drawAttr.calcZoom=TRUE;
      drawAttr.stretched = isViewMenuChecked(hMenu, IDM_VIEW_STRETCH);
      refreshPending++;

      setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                          imagesDisplayed, g_historySize, drawAttr.zoom);

      // clear the image data status msg
      //SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)"Ready");

      return 0;

    case IDM_VIEW_PREV:
      // switch to previously output image
      if ( curImage!=firstImage ) --curImage;
      if (curImage<0) curImage = g_historySize-1;

      // draw the current image into the DIBitmap
      InvalidateRect(hwnd, NULL, TRUE);
      drawAttr.calcZoom=TRUE;
      drawAttr.stretched = isViewMenuChecked(hMenu, IDM_VIEW_STRETCH);
      refreshPending++;

      setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                          imagesDisplayed, g_historySize, drawAttr.zoom);
      // clear the image data status msg
      // SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)"Ready");
      return 0;


    case IDM_FILE_EXIT:

      // more cleanup needed...

      // free image buffer

    {
      int i;
      if (pCurDib != NULL)
      {
        free (pCurDib);
        pCurDib = NULL;
      }
      for (i=0; i<g_historySize; i++) {
        if (pRawData[i] != 0) {
          free (pRawData[i]);
          pRawData[i] = NULL;
        }
      }

      destroyFileMapping(&pMapView, &pFileMap);

      saveRegistryPrefs(hwnd);
      // let's go bye-bye ...
      exit (0);
    }
    return 0;

    case IDM_VIEW_GRID:
      drawAttr.grid = !drawAttr.grid;
      if (drawAttr.grid)
        CheckMenuItem (hMenu, IDM_VIEW_GRID, MF_CHECKED);
      else
        CheckMenuItem (hMenu, IDM_VIEW_GRID, MF_UNCHECKED);

      // invalidate the client area for later update
      InvalidateRect (hwnd, NULL, TRUE);

      // display the PNG into the DIBitmap
      if (imagesDisplayed) {
        refreshPending++;
      }
      return 0;

    case IDM_VIEW_CHECK:
      drawAttr.checked = !drawAttr.checked;
      if (drawAttr.checked)
        CheckMenuItem (hMenu, IDM_VIEW_CHECK, MF_CHECKED);
      else
        CheckMenuItem (hMenu, IDM_VIEW_CHECK, MF_UNCHECKED);

      // invalidate the client area for later update
      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
      return 0;

    case IDM_VIEW_STRETCH:
      drawAttr.stretched = !drawAttr.stretched;
      if (drawAttr.stretched)
        CheckMenuItem (hMenu, IDM_VIEW_STRETCH, MF_CHECKED);
      else
        CheckMenuItem (hMenu, IDM_VIEW_STRETCH, MF_UNCHECKED);

      // invalidate the client area for later update
      InvalidateRect (hwnd, NULL, TRUE);

      // display the PNG into the DIBitmap
      if (imagesDisplayed) {
        drawAttr.calcZoom=TRUE;
        refreshPending++;
      }
      return 0;

    case IDM_VIEW_FLIP:
      drawAttr.flipped = !drawAttr.flipped;
      if (drawAttr.flipped)
        CheckMenuItem (hMenu, IDM_VIEW_FLIP, MF_CHECKED);
      else
        CheckMenuItem (hMenu, IDM_VIEW_FLIP, MF_UNCHECKED);

      // invalidate the client area for later update
      InvalidateRect (hwnd, NULL, TRUE);
      if (isViewMenuChecked(hMenu, IDM_VIEW_STRETCH)) {
        drawAttr.calcZoom = TRUE;
      }
      if (imagesDisplayed) {
        refreshPending++;
      }
      return 0;

    case IDM_VIEW_RESCALE:
      // display the PNG into the DIBitmap
    {
      BOOL stretchSave=drawAttr.stretched;
      drawAttr.stretched = TRUE;
      drawAttr.calcZoom = TRUE;
      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
      drawAttr.stretched = stretchSave;
    }
    return 0;

    case IDM_VIEW_ZOOMIN:
    case IDM_VIEW_ZOOMOUT:
    {
      if (!imagesDisplayed) return 0;
      doZoom((LOWORD(wParam))==IDM_VIEW_ZOOMIN?1:-1,
             WinW/2, WinH/2, &drawAttr);

      InvalidateRect (hwnd, NULL, TRUE);

      setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                          imagesDisplayed, g_historySize, drawAttr.zoom);
      refreshPending++;
    }
    return 0;

    case IDM_VIEW_IGNORE:
      if (isViewMenuChecked(hMenu, IDM_VIEW_IGNORE))
        CheckMenuItem (hMenu, IDM_VIEW_IGNORE, MF_UNCHECKED);
      else
        CheckMenuItem (hMenu, IDM_VIEW_IGNORE, MF_CHECKED);
      SendMessage(hwndStatusBar, SB_SETTEXT, SBT_OWNERDRAW, (LPARAM)0);
      return 0;

    case IDM_VIEW_SHARE:
    {
      if (isViewMenuChecked(hMenu, IDM_VIEW_SHARE))
        CheckMenuItem (hMenu, IDM_VIEW_SHARE, MF_UNCHECKED);
      else
        CheckMenuItem (hMenu, IDM_VIEW_SHARE, MF_CHECKED);
      // must update preferences too so 
      saveRegistryPrefs(hwnd);
    }
    return 0;

    case IDM_VIEW_SB_CONTROLS:
    {
      if (isViewMenuChecked(hMenu, IDM_VIEW_SB_CONTROLS)) {
        CheckMenuItem (hMenu, IDM_VIEW_SB_CONTROLS, MF_UNCHECKED);
        drawAttr.extraBLmargin = 0;
      }
      else {
        CheckMenuItem (hMenu, IDM_VIEW_SB_CONTROLS, MF_CHECKED);
        drawAttr.extraBLmargin = SCALE_BIAS_CONTROL_WIDTH;
      }

      InvalidateRect (hwnd, NULL, TRUE);
      refreshPending++;
    }
    return 0;

    case IDM_CHAN_RGBA:
    case IDM_CHAN_RGB:
    case IDM_CHAN_RED:
    case IDM_CHAN_GREEN:
    case IDM_CHAN_BLUE:
    case IDM_CHAN_ALPHA:
    {
      unsigned int newitem=LOWORD(wParam);
      unsigned int olditem;
      switch(drawAttr.enabledChannels) {
        case IMDBG_ENABLE_RGBA:  olditem = IDM_CHAN_RGBA ; break;
        case IMDBG_ENABLE_RGB:   olditem = IDM_CHAN_RGB  ; break;
        case IMDBG_ENABLE_RED:   olditem = IDM_CHAN_RED  ; break;
        case IMDBG_ENABLE_GREEN: olditem = IDM_CHAN_GREEN; break;
        case IMDBG_ENABLE_BLUE:  olditem = IDM_CHAN_BLUE ; break;
        case IMDBG_ENABLE_ALPHA: olditem = IDM_CHAN_ALPHA; break;
      }
      if (olditem==newitem) return 0;
      CheckMenuItem(hMenu, olditem, MF_UNCHECKED);
      CheckMenuItem(hMenu, newitem, MF_CHECKED);
      getEnableAttrsFromMenuState(hMenu, &drawAttr);
      InvalidateRect(hwnd, NULL, TRUE);
      refreshPending++;
    }
    return 0;

    case IDM_HELP_ABOUT:
    {
      INT_PTR ret;
      ret = DialogBox (hInstance, TEXT ("ABOUTBOX"), hwnd, AboutDlgProc) ;
      if (ret == -1) {
        LPVOID lpMsgBuf;
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                       FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL,    GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       // Default language
                       (LPTSTR) &lpMsgBuf,    0,    NULL );
        chMB((const char*)lpMsgBuf);
        LocalFree( lpMsgBuf );
      }
    }
    return 0;

    case IDM_EDIT_OPTIONS:
    {
      INT_PTR ret;
      ret = DialogBox (hInstance, TEXT ("OPTIONSBOX"), hwnd, OptionsDlgProc) ;
      if (ret == -1) {
        LPVOID lpMsgBuf;
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                       FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL,    GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       // Default language
                       (LPTSTR) &lpMsgBuf,    0,    NULL );
        chMB((const char*)lpMsgBuf);
        LocalFree( lpMsgBuf );
      }
      else if (ret>0 && ret!=g_historySize) {
        int nhist = ret;
        int i;
        // Just throw away all the data when they resize the buffer.
        // So much easier that way.
        for (i=0; i<g_historySize; i++) {
          if (pRawData[i]) free(pRawData[i]);
        }
        // resize history buffer
        pRawData = (BYTE**)  realloc(pRawData, sizeof(BYTE*) *nhist);
        improps  = (ImProps*)realloc(improps, sizeof(ImProps)*nhist);
        ZeroMemory(pRawData, nhist*sizeof(BYTE*));
        ZeroMemory(improps,  nhist*sizeof(ImProps));
        g_historySize = nhist;
        
        // reset all history parameters
        imagesDisplayed = 0;
        numImages = 0;
        curImage = 0;
        firstImage = 0;
        lastImage = 0;

        InvalidateRect (hwnd, NULL, TRUE);
        refreshPending++;
        setImageCountStatus(hwndStatusBar, curImage, firstImage, lastImage,
                            imagesDisplayed, g_historySize, drawAttr.zoom);
      }
    }
    return 0;

    } // end inner switch
  }
  break;

  case WM_DRAWITEM:
  {
    // draw the dang statusbar
    HWND statusHWND = (HWND) wParam;
    DRAWITEMSTRUCT *ds = (DRAWITEMSTRUCT*)lParam;
    BOOL isChangingScaleBias = 
      GetCapture()==hwnd && (dragMode==IMDM_BIAS||dragMode==IMDM_SCALE);
    StatusMessageInfo info;
    if (improps[curImage].nchan<=0) {
      int err = improps[curImage].nchan;
      drawCustomStatusBarError(hwnd, ds, err);
    }
    else if (getImageDataStatusInfo(hwndStatusBar,&drawAttr, 
                                    &improps[curImage],
                                    pRawData[curImage],
                                    lastPtrWinX, lastPtrWinY, 
                                    WinW, WinH, 
                                    isChangingScaleBias,
                                    &info ))
    {
      drawCustomStatusBar(hwndStatusBar, ds, &info);
    }
    if (isViewMenuChecked(GetMenu(hwnd), IDM_VIEW_IGNORE)) {
      RECT rect = ds->rcItem;
      SetBkMode(ds->hDC, OPAQUE);
      SetBkColor(ds->hDC, RGB(180,50,50));
      SetTextColor(ds->hDC, RGB(255,255,255));
      DrawText(ds->hDC, " BLOCK ", 7, &rect, 
               DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
      SetBkMode(ds->hDC, TRANSPARENT);
    }  
  }
  break;
  case WM_CLOSE:
    saveRegistryPrefs(hwnd);
    break;

  case WM_DESTROY :
    destroyFileMapping(&pFileMap, &pMapView);

    PostQuitMessage (0) ;
    return 0 ;
  }
  return DefWindowProc (hwnd, iMsg, wParam, lParam) ;
}

/*--------------------------------------------------------------------------*/
/* ABOUT DIALOG BOX --------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

BOOL CALLBACK AboutDlgProc (HWND hDlg, UINT message,
                            WPARAM wParam, LPARAM lParam)
{
     switch (message)
     {
     case WM_INITDIALOG :
          ShowWindow (hDlg, SW_HIDE);
          CenterAbout (hDlg, GetWindow (hDlg, GW_OWNER));
          ShowWindow (hDlg, SW_SHOW);
          return TRUE ;

     case WM_COMMAND :
          switch (LOWORD (wParam))
          {
          case IDOK :
          case IDCANCEL :
               EndDialog (hDlg, 0) ;
               return TRUE ;
          }
          break ;
     }
     return FALSE ;
}

//---------------
//  CenterAbout
//---------------

BOOL CenterAbout (HWND hwndChild, HWND hwndParent)
{
   RECT    rChild, rParent, rWorkArea;
   int     wChild, hChild, wParent, hParent;
   int     xNew, yNew;
   BOOL  bResult;

   // Get the Height and Width of the child window
   GetWindowRect (hwndChild, &rChild);
   wChild = rChild.right - rChild.left;
   hChild = rChild.bottom - rChild.top;

   // Get the Height and Width of the parent window
   GetWindowRect (hwndParent, &rParent);
   wParent = rParent.right - rParent.left;
   hParent = rParent.bottom - rParent.top;

   // Get the limits of the 'workarea'
   bResult = SystemParametersInfo(
      SPI_GETWORKAREA,  // system parameter to query or set
      sizeof(RECT),
      &rWorkArea,
      0);
   if (!bResult) {
      rWorkArea.left = rWorkArea.top = 0;
      rWorkArea.right = GetSystemMetrics(SM_CXSCREEN);
      rWorkArea.bottom = GetSystemMetrics(SM_CYSCREEN);
   }

   // Calculate new X position, then adjust for workarea
   xNew = rParent.left + ((wParent - wChild) /2);
   if (xNew < rWorkArea.left) {
      xNew = rWorkArea.left;
   } else if ((xNew+wChild) > rWorkArea.right) {
      xNew = rWorkArea.right - wChild;
   }

   // Calculate new Y position, then adjust for workarea
   yNew = rParent.top  + ((hParent - hChild) /2);
   if (yNew < rWorkArea.top) {
      yNew = rWorkArea.top;
   } else if ((yNew+hChild) > rWorkArea.bottom) {
      yNew = rWorkArea.bottom - hChild;
   }

   // Set it, and return
   return SetWindowPos (hwndChild, NULL, xNew, yNew, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

/*--------------------------------------------------------------------------*/
/* OPTIONS DIALOG BOX ------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
BOOL CALLBACK OptionsDlgProc (HWND hDlg, UINT message,
                              WPARAM wParam, LPARAM lParam)
{
  static HWND hEdit = 0;
  static HWND hSpin = 0;

  // contains IDC_NUMEDIT and IDC_NUMSPIN

  switch (message)
  {
  case WM_INITDIALOG :
    ShowWindow (hDlg, SW_HIDE);
    CenterAbout (hDlg, GetWindow (hDlg, GW_OWNER));
    ShowWindow (hDlg, SW_SHOW);
    hEdit = GetDlgItem(hDlg, IDC_NUMEDIT);
    hSpin = GetDlgItem(hDlg, IDC_NUMSPIN);
    SetDlgItemInt(hDlg, IDC_NUMEDIT, g_historySize, FALSE);
    CheckDlgButton(hDlg, IDC_COPYSCALE,
                   g_matchScaleOnTagged?BST_CHECKED:BST_UNCHECKED);

    SendMessage(hSpin, UDM_SETBUDDY, (WPARAM)hEdit, (LPARAM)0);
    SendMessage(hSpin, UDM_SETRANGE, (WPARAM)0,
                (LPARAM) MAKELONG((short)100,(short)1));

    return TRUE ;

  case WM_COMMAND :
  {
    BOOL ret;
    UINT val;
    INT_PTR dlgRet = 0;
    switch (LOWORD (wParam))
    {
    case IDOK :
      // read value 
      val = GetDlgItemInt(hDlg, IDC_NUMEDIT, &ret, FALSE);
      if (ret) {
        dlgRet = val;  // for now, till there's more data we need to return
      }
      g_matchScaleOnTagged = 
        (IsDlgButtonChecked(hDlg, IDC_COPYSCALE)==BST_CHECKED);
      // FALL THROUGH!!

    case IDCANCEL :
      EndDialog (hDlg, dlgRet) ;
      return TRUE ;
    }
    break ;
  }
  }
  return FALSE ;
}

