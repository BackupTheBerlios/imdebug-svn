 /*-----------------------------------------------------------*
  displayFrame.cpp -- displayer for debug images
  *-----------------------------------------------------------*

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
 
 *-----------------------------------------------------------*

  Acknowledgements to Jeroen van der Zijp who wrote most of the FOX
  toolkit and the "Image display" program demo.  The image viewer code
  here was based initally off of his source code.

  For more info on Fox see:
      http://www.fox-toolkit.org

 *-----------------------------------------------------------*/

/***********************************************************************
* Based off of Fox Image Viewer Demo,                                  *
* Origial copyright message:                                           *
************************************************************************
* Copyright (C) 2000 by Jeroen van der Zijp.   All Rights Reserved.    *
************************************************************************
* $Id: displayFrame.cpp 1916 2005-10-22 02:13:59Z baxter $         *
***********************************************************************/
#ifdef _WIN32
#include <windows.h>
#include <assert.h>
#endif
#include <fx.h>
#ifdef HAVE_PNG_H
#include "FXPNGImage.h"
#endif
#ifdef HAVE_JPEG_H
#include "FXJPGImage.h"
#endif
#ifdef HAVE_TIFF_H
#include "FXTIFImage.h"
#endif
#include "FXICOImage.h"
#include "FXTGAImage.h"
#include "FXRGBImage.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <list>

#include "ImdebugImageView.h"
#include "CustomStatusBar.h"
#include "CustomToolTip.h"
#include "ToolBarButton.h"
#include "ToolBarToggleButton.h"
#include "memorymappedfile.h"
#ifdef _WIN32
#include "win32event.h"
#endif
#include "imdebug_priv.h"
#ifndef BYTE
#define BYTE FXuchar
#endif
#include "blitfuncs.h"
#include "imdbgicons.h"

#define ERROR_NO_IMAGE 0
#define ERROR_NULL_POINTER -1
#define ERROR_ZERO_PIXELS -2
#define ERROR_TOO_BIG -3
#define ERROR_ALLOC_FAIL -4
enum { BF_STRETCH=0x1, BF_SCALE=0x2, BF_BIAS=0x4 };
enum { BF_INT_OFFSET=0, BF_FLOAT_OFFSET = 5 };

PBLITFUNC g_blitFuncTable[(BF_STRETCH|BF_SCALE|BF_BIAS)+1][10];
/**********************************************************************/

inline ImProps* mapProps(void *mapview)
{
  return (ImProps*)mapview;
}
inline FXuchar* mapImage(void *mapview)
{
  return ((FXuchar*)mapview)+sizeof(ImProps);
}
/**********************************************************************/
// DataTarg is a simple template class that binds together in one entity
// an FXDataTarget object and it's associated variable.
template <class T>
class DataTarg
{
  public:
    DataTarg() : targ(val) {}
    DataTarg(FXObject *tgt,FXSelector sel=0)
      : targ(val,tgt,sel) {}
    T& operator=(const T& o) { return val = o; }
    FXDataTarget& operator()() { return targ; }
    bool operator!() { return !val; }
    operator T() { return val; }
    
    FXDataTarget targ;
    T val;
};
typedef DataTarg<FXuint> UIntTarget;
typedef DataTarg<FXint>  IntTarget;
typedef DataTarg<FXbool> BoolTarget;
/**********************************************************************/

struct DrawAttribs
{
  DrawAttribs() 
    :
  stretched       (true),
  flipped         (false),
  //tx              (0),
  //ty              (0),
  //zoom            (1.0),
  margin          (8),
  calcZoom        (true),
  extraBLmargin   (0),
  lastTypeMax     (1.0f),
  enabledChannels (ENABLE_RGBA)
  {}
  bool stretched;
  bool flipped;
  bool calcZoom; // request to automatically scale/translate
  int  margin;
  int  extraBLmargin;  // extra margin for scrollbars when calcZoom on and
                       // scale&bias scrollbars are on
  //int  tx, ty; // xy translation
  //float zoom;  // uniform stretch scaling
  float lastTypeMax;
  enum { 
    ENABLE_RED   = 0x1,
    ENABLE_GREEN = 0x2,
    ENABLE_BLUE  = 0x4,
    ENABLE_ALPHA = 0x8,
    ENABLE_RGB   = 0x7,
    ENABLE_RGBA  = 0xf
  };
  unsigned char enabledChannels;
};
/**********************************************************************/

struct ImPropsEx : public ImProps
{
  ImPropsEx() : zoom(0.0f), tx(0), ty(0), globalImgNum(-1),imgNum(0) {}
  float zoom;
  float tx, ty;
  size_t rawImageSize;
  int globalImgNum; // sequence number of image out of all images received
  int imgNum;   // sequence number of image of all images with same title
};


/**********************************************************************/
static void setScaleAndBias(BlitStats *bs, 
                            float typeMin, float typeMax, 
                            float *scale, float *bias)
{
  // Compute global scale and bias across channels. 
  // Scaling per-channel is wierd.

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
      scale[i] = fScale; // (typeMax-typeMin)/(vmax-vmin);
      bias[i]  = (typeMin-vmin) * scale[i];
    }
  }

} 
double getTypemax(ImProps *props)
{
  if (props->type[0] == IMDBG_FLOAT) return 1.0;
  double typeMax = (double)(~((~0UL)<<props->bpc[0]));
  return typeMax;

}
/**********************************************************************/
static void mapWindowToImage(int xWin, int yWin, int hWin, 
                             int* xImg, int *yImg,
                             const DrawAttribs *pAttrs, 
                             const ImPropsEx *pProps)
{
  if (pAttrs->flipped) {
    yWin = hWin - yWin;
  }
  *xImg = (int)floor((xWin - pProps->tx)/pProps->zoom);
  *yImg = (int)floor((yWin - pProps->ty)/pProps->zoom);
}
static void mapImageToWindow(int xImg, int yImg, int *xWin, int *yWin,
                             const DrawAttribs *pAttrs, const ImPropsEx *pProps)
{
  *xWin = (int)floor((xImg*pProps->zoom) + pProps->tx);
  *yWin = (int)floor((yImg*pProps->zoom) + pProps->ty);
}
/**********************************************************************/
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


/**********************************************************************/
static const char *g_toolbarButtonText[] =
{
"&Delete\tDelete Image\tDelete current image.",
"&First\tFirst image\tFirst image.",
"&Prev\tPrevious image\tPrevious image.",
"&Next\tNext image\tNext image.",
"&Last\tLast image\tLast image.",
"Tag Pre&v\tPrevious image of same tag\tPrevious image with the same tag as this one.",
"Tag Ne&xt\tNext image of same tag\tNext image with the same tag as this one.",
"Lin&k\tLink image properties\tLink scale, translation, etc of all images with the same title",
"&Block\tBlock updates\tBlock new images coming from clients",
"Diff Ba&se\tSet image diff base\tMake this image the reference for diff",
"D&iff\tEnable image diffs\tEnable diffs with reference image"
};


/**********************************************************************/


class ImageWindow : public FXMainWindow {
  FXDECLARE(ImageWindow)
protected:
  typedef FXuchar Byte;

  typedef ImdebugImageView ImageView;
  //typedef FXImageView ImageView;
  //typedef FXImageFrame ImageView;

  ImageView         *imageview;
  FXHorizontalFrame *biasframe;
  FXVerticalFrame   *scaleframe;
  FXRecentFiles      mrufiles;              // Recent files
  FXString           filename;              // File being viewed
  FXMenuBar         *menubar;               // Menu bar
  FXStatusBar       *statusbar;             // Status bar
//  FXHorizontalFrame *toolbarframe;
  FXToolBarShell    *toolbarframe;
  FXToolBar         *toolbar;               // toolbar
  CustomToolTip     *tooltip;               // Tool tip
  FXMenuPane        *filemenu;              // File menu
  FXMenuPane        *editmenu;              // Edit menu
  FXMenuPane        *imagemenu;             // Image menu
  FXMenuPane        *viewmenu;              // View menu
  FXMenuPane        *helpmenu;              // Help menu
  FXMenuPane        *chanmenu;              // Channels menu
  FXLabel           *statusNavLabel;        // label for navigation status
  FXLabel           *statusBlockLabel;      // label indicating blocking
  FXLabel           *statusDiffLabel;       // label indicating image diff
  FXDial            *biasdial;              // for setting value biasing
  FXDial            *finebiasdial;          // for setting value biasing
  FXDial            *scaledial;             // for setting value scaling

  //FXString           tooltipString;
  //FXString           statusHelpString;

  FXIcon            *appIcon;
  FXIcon            *appIconMini;

  typedef std::list<FXObject*> ObjectCleanup;
  ObjectCleanup garbageMan;

  FXInputHandle win32ImageReceivedSignal;
  MemoryMappedFile  *mappedFile;  // Comm channel with clients

  Byte    **pRawData;             // array: space for raw image data recv'd
  ImPropsEx *improps;               // array: properties of images
  int      historySize;           // max number of images buffer can hold
  int      numImages;             // num image currently held
  int      curImage;              // index of current image
  int      firstImage;            // start index (circular buffer)
  int      lastImage;             // end   index (circular buffer)
  int      imagesDisplayed;       // total number of images recieved so far

  int      diffImage;             // index of base image for image diffs
  Byte     *pDiffData;            // diff image for diff tooltips...
  bool     bDiffValid;

  DrawAttribs drawAttr;           // attributes for drawing all images

  FXuint     downButtons;       // buttons currently pressed
  FXint      lastHoverX;
  FXint      lastHoverY;

  /////////////// DATA TARGETS ///////////////
  UIntTarget itEnabledChannels;           // Channels set
  BoolTarget btBlockUpdates;
  BoolTarget btPropagateScaleOnTagged;  // propagate scale settings 
  BoolTarget btLinkImageProps;
  BoolTarget btCheckerBackground;
  BoolTarget btAutoStretch;
  BoolTarget btShowPixelGrid;
  BoolTarget btFlipImage;
  BoolTarget btShareOutputWindow;
  BoolTarget btOverwriteImages;
  BoolTarget btHoldOnImage;
  BoolTarget btShowManipCtrls;
  BoolTarget btShowToolbarText;
  BoolTarget btDiffImage;
  //BoolTarget btShowToolbar;
  //BoolTarget btShowStatusbar;


protected:
  ImageWindow(){}
  void drawPixelGridOnImage(FXImage *img); 
  bool regenerateDisplayImage();
  void regenerateDiffImage(Byte* &pImage);
  void clearImageToBackground(FXImage *img);
  void copyRawImageData(ImPropsEx *pProp, Byte **rawData);
  static int  computeRawDataSize(ImProps *pProp);
  void writeStatusMessage(
    char *statusmsg, char *tooltipmsg, int ImgX, int ImgY, 
    Byte*rawptr, ImPropsEx *p, DrawAttribs *pAttr);
  void setImageCountStatus();
  void setHoverImageInfoStatus(int x, int y);
  void setHoverImageInfoStatus();
  void setNormalImageInfoStatus();
  void setNormalDialDraggingStatus();
  void doZoom(int dirn, int xWin, int yWin);

  // these next three methods set props on curImage, or on all images matching
  // curImage's tag, depending on the setting of btLinkImageProps
  void setImageTranslationProp(float tx, float ty);
  void setImageZoomProp(float zoom);
  void setImageScaleBiasProp(float scale, float bias);

  void setScaleBiasHelpText();
  void clearScaleBiasHelpText();
  void syncScaleBiasDials(bool doScale=true,bool doBias=true);
  void addNewImageSlotToCircularBuffer();
  int findPreviousImageIndexByTag(const char *title, int startAt=-999);
  int findNextImageIndexByTag(const char *title, int startAt=-999);

public:
  long onCmdAbout      (FXObject*,FXSelector,void*);
  long onCmdOpen       (FXObject*,FXSelector,void*);
  long onCmdSaveAs     (FXObject*,FXSelector,void*);
  long onCmdRecentFile (FXObject*,FXSelector,void*);
  long onCmdQuit       (FXObject*,FXSelector,void*);
  long onCmdCopy       (FXObject*,FXSelector,void*);
  long onCmdPaste      (FXObject*,FXSelector,void*);
  long onCmdDelete     (FXObject*,FXSelector,void*);
  long onCmdOptions    (FXObject*,FXSelector,void*);
  long onCmdNavigate   (FXObject*,FXSelector,void*);
  long onCmdRescale    (FXObject*,FXSelector,void*);
  long onCmdZoom       (FXObject*,FXSelector,void*);
  long onCmdToggles    (FXObject*,FXSelector,void*);
  long onUpdateMenu    (FXObject*,FXSelector,void*);
  long onUpdateToggles (FXObject*,FXSelector,void*);
  long onCmdChannels   (FXObject*,FXSelector,void*);
  long onUpdTitle      (FXObject*,FXSelector,void*);
  //long onQueryTip      (FXObject*,FXSelector,void*);
  //long onQueryStatus   (FXObject*,FXSelector,void*);
  long onUpdImage      (FXObject*,FXSelector,void*);
  long onNewImage      (FXObject*,FXSelector,void*);
  //long onIdleTask      (FXObject*,FXSelector,void*);
  long onScaleBias     (FXObject*,FXSelector,void*);
  long onScaleBiasRelease(FXObject*,FXSelector,void*);
  long onCmdScaleBias  (FXObject*,FXSelector,void*); // set autoscale, unity scale
  long onCmdSetDiffBase (FXObject*,FXSelector,void*); // set the current image to the diff base

  long onMouseUp       (FXObject*,FXSelector,void*);
  long onMouseDown     (FXObject*,FXSelector,void*);
  long onMouseMove     (FXObject*,FXSelector,void*);
  long onMouseWheel    (FXObject*,FXSelector,void*);
  long onLeave         (FXObject*,FXSelector,void*);
  long onResize        (FXObject*,FXSelector,void*);

  long onExternalNotify(FXObject*,FXSelector,void*);
  
public:
  enum{
    ID_ABOUT=FXMainWindow::ID_LAST,
    ID_OPEN,
    ID_SAVE_AS,
    ID_TITLE,
    ID_QUIT,
    ID_COPY,
    ID_CUT,
    ID_PASTE,
    ID_DELETE,
    ID_OPTIONS,
    ID_NEXT_IMAGE,
    ID_PREV_IMAGE,
    ID_FIRST_IMAGE,
    ID_PREV_OF_TITLE,
    ID_NEXT_OF_TITLE,
    ID_LAST_IMAGE,
    ID_RESCALE,
    ID_ZOOM_IN,
    ID_ZOOM_OUT,
    ID_ZOOM_100,
    ID_ZOOM_200,
    ID_ZOOM_300,
    ID_ZOOM_400,
    ID_ZOOM_500,
    ID_ZOOM_600,
    ID_ZOOM_700,
    ID_ZOOM_800,
    ID_ZOOM_900,
    ID_ZOOM_1000,
    ID_IMAGE_SCALE,
    ID_IMAGE_BIAS,
    ID_IMAGE_BIAS_FINE,
    ID_AUTOSCALE,
    ID_UNITYSCALE,
    ID_RECENTFILE,
    ID_SET_DIFF_BASE,
    
    ID_SCALE_BIAS_DIAL,

    ID_FIRST_TOGGLE,
    ID_BLOCK_UPDATES=ID_FIRST_TOGGLE,
    ID_SHARE_WINDOW,
    ID_LINK_PROPS,
    ID_OVERWRITE_IMAGES,
    ID_HOLD_ON_IMAGE,
    ID_FLIP,
    ID_STRETCH_FIT,
    ID_SHOW_GRID,
    ID_SHOW_SCALE_BIAS,
    ID_SHOW_TOOLBAR,
    ID_SHOW_TOOLBAR_TEXT,
    ID_SHOW_STATUSBAR,
    ID_CHECK_BKGND,
    ID_DIFF_IMAGE,
    ID_LAST_TOGGLE = ID_DIFF_IMAGE,

    ID_RGBA,
    ID_RGB,
    ID_RED,
    ID_GREEN,
    ID_BLUE,
    ID_ALPHA,
    ID_IDLE_TASK,

    ID_DISPLAY_AREA,
    ID_LAST
    };
public:
  ImageWindow(FXApp* a);
  virtual void create();
  FXbool loadimage(const FXString& file);
  FXbool saveimage(const FXString& file);
  virtual ~ImageWindow();
};


/***************************************************************************/

// Patterns
const FXchar patterns[]=
  "All Files (*)"
  "\nGIF Image (*.gif)"
  "\nBMP Image (*.bmp)"
  "\nXPM Image (*.xpm)"
  "\nPCX Image (*.pcx)"
  "\nICO Image (*.ico)"
  "\nRGB Image  (*.rgb)"
  "\nTARGA Image  (*.tga)"
#ifdef HAVE_PNG_H
  "\nPNG Image  (*.png)"
#endif
#ifdef HAVE_JPEG_H
  "\nJPEG Image (*.jpg)"
#endif
#ifdef HAVE_TIFF_H
  "\nTIFF Image (*.tif)"
#endif
  ;

/***************************************************************************/

// Map
FXDEFMAP(ImageWindow) ImageWindowMap[]={
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_ABOUT,      ImageWindow::onCmdAbout),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_OPEN,       ImageWindow::onCmdOpen),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_SAVE_AS,    ImageWindow::onCmdSaveAs),
  FXMAPFUNC(SEL_UPDATE,  ImageWindow::ID_TITLE,      ImageWindow::onUpdTitle),
  //FXMAPFUNC(SEL_UPDATE,  ImageWindow::ID_QUERY_TIP,     ImageWindow::onQueryTip),
  //FXMAPFUNC(SEL_UPDATE,  ImageWindow::ID_QUERY_HELP,    ImageWindow::onQueryStatus),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_QUIT,       ImageWindow::onCmdQuit),
  FXMAPFUNC(SEL_SIGNAL,  ImageWindow::ID_QUIT,       ImageWindow::onCmdQuit),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_COPY,       ImageWindow::onCmdCopy),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_PASTE,      ImageWindow::onCmdPaste),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_DELETE,     ImageWindow::onCmdDelete),
  FXMAPFUNC(SEL_CLOSE,   ImageWindow::ID_TITLE,      ImageWindow::onCmdQuit),
  //FXMAPFUNC(SEL_CHORE,   ImageWindow::ID_IDLE_TASK,  ImageWindow::onIdleTask), 

  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_OPTIONS,    ImageWindow::onCmdOptions),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_NEXT_IMAGE, ImageWindow::onCmdNavigate),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_PREV_IMAGE, ImageWindow::onCmdNavigate),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_NEXT_OF_TITLE, ImageWindow::onCmdNavigate),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_PREV_OF_TITLE, ImageWindow::onCmdNavigate),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_FIRST_IMAGE, ImageWindow::onCmdNavigate),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_LAST_IMAGE, ImageWindow::onCmdNavigate),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_RESCALE,    ImageWindow::onCmdRescale),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_ZOOM_IN,    ImageWindow::onCmdZoom),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_ZOOM_OUT,   ImageWindow::onCmdZoom),
  FXMAPFUNCS(SEL_COMMAND,ImageWindow::ID_ZOOM_100,  ImageWindow::ID_ZOOM_1000, ImageWindow::onCmdZoom),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_RECENTFILE, ImageWindow::onCmdRecentFile),
  FXMAPFUNC(SEL_COMMAND, ImageWindow::ID_SET_DIFF_BASE, ImageWindow::onCmdSetDiffBase),

  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_SAVE_AS,    ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_COPY,       ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_DELETE,     ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_NEXT_IMAGE, ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_PREV_IMAGE, ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_NEXT_OF_TITLE, ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_PREV_OF_TITLE, ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_FIRST_IMAGE, ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_LAST_IMAGE, ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_RESCALE,    ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_ZOOM_IN,    ImageWindow::onUpdateMenu),
  FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_ZOOM_OUT,   ImageWindow::onUpdateMenu),
  FXMAPFUNCS(SEL_UPDATE,ImageWindow::ID_ZOOM_100,   ImageWindow::ID_ZOOM_1000, ImageWindow::onUpdateMenu),
  //FXMAPFUNC(SEL_UPDATE, ImageWindow::ID_RECENTFILE, ImageWindow::onUpdateMenu),


  FXMAPFUNCS(SEL_COMMAND, ImageWindow::ID_FIRST_TOGGLE,ImageWindow::ID_LAST_TOGGLE, ImageWindow::onCmdToggles),
  FXMAPFUNCS(SEL_COMMAND, ImageWindow::ID_FIRST_TOGGLE,ImageWindow::ID_LAST_TOGGLE, ImageWindow::onUpdateToggles),
  FXMAPFUNCS(SEL_COMMAND, ImageWindow::ID_RGBA,        ImageWindow::ID_ALPHA,       ImageWindow::onCmdChannels),

  FXMAPFUNC(SEL_LEFTBUTTONPRESS,   ImageWindow::ID_DISPLAY_AREA, ImageWindow::onMouseDown),
  FXMAPFUNC(SEL_LEFTBUTTONRELEASE, ImageWindow::ID_DISPLAY_AREA, ImageWindow::onMouseUp),
  FXMAPFUNC(SEL_MOUSEWHEEL,        ImageWindow::ID_DISPLAY_AREA, ImageWindow::onMouseWheel),
  FXMAPFUNC(SEL_MOTION,            ImageWindow::ID_DISPLAY_AREA, ImageWindow::onMouseMove),
  FXMAPFUNC(SEL_CONFIGURE,         ImageWindow::ID_DISPLAY_AREA, ImageWindow::onResize),
  FXMAPFUNC(SEL_LEAVE,             ImageWindow::ID_DISPLAY_AREA, ImageWindow::onLeave),

  FXMAPFUNC(SEL_CHANGED,            ImageWindow::ID_IMAGE_BIAS,     ImageWindow::onScaleBias),
  FXMAPFUNC(SEL_CHANGED,            ImageWindow::ID_IMAGE_BIAS_FINE,ImageWindow::onScaleBias),
  FXMAPFUNC(SEL_CHANGED,            ImageWindow::ID_IMAGE_SCALE,    ImageWindow::onScaleBias),
  FXMAPFUNC(SEL_COMMAND,            ImageWindow::ID_IMAGE_BIAS,     ImageWindow::onScaleBiasRelease),
  FXMAPFUNC(SEL_COMMAND,            ImageWindow::ID_IMAGE_BIAS_FINE,ImageWindow::onScaleBiasRelease),
  FXMAPFUNC(SEL_COMMAND,            ImageWindow::ID_IMAGE_SCALE,    ImageWindow::onScaleBiasRelease),
  
  FXMAPFUNC(SEL_COMMAND,           ImageWindow::ID_AUTOSCALE,    ImageWindow::onCmdScaleBias),
  FXMAPFUNC(SEL_COMMAND,           ImageWindow::ID_UNITYSCALE,   ImageWindow::onCmdScaleBias),

  FXMAPFUNC(SEL_IO_READ,  0/*ImageWindow::ID_IDLE_TASK*/,ImageWindow::onExternalNotify),

  /*
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_ROTATE_90,  ImageWindow::onUpdImage),
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_ROTATE_180, ImageWindow::onUpdImage),
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_ROTATE_270, ImageWindow::onUpdImage),
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_MIRROR_HOR, ImageWindow::onUpdImage),
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_MIRROR_VER, ImageWindow::onUpdImage),
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_SCALE,      ImageWindow::onUpdImage),
  FXMAPFUNC(SEL_UPDATE,        ImageWindow::ID_CROP,       ImageWindow::onUpdImage),
  */
};


// Object implementation
FXIMPLEMENT(ImageWindow,FXMainWindow,ImageWindowMap,ARRAYNUMBER(ImageWindowMap))


// Make some windows
ImageWindow::ImageWindow(FXApp* a)
  : FXMainWindow(a,IMDBG_DISPLAY_WINDOW_TITLE,NULL,NULL,DECOR_ALL,0,0,850,600,0,0),
  win32ImageReceivedSignal(0),
  historySize(0),
  numImages(0),
  curImage(0),
  firstImage(0),
  lastImage(0),
  diffImage(0),
  pDiffData(0),
  bDiffValid(false),
  imagesDisplayed(0),
  downButtons(0)
{
  setTarget(this);
  setSelector(ID_TITLE);
    
  {
    FXIcon *big = new FXBMPIcon(getApp(), icon64pal);
    FXIcon *tiny= new FXBMPIcon(getApp(), icon16pal);
    setIcon(big);
    setMiniIcon(tiny);
    garbageMan.push_back( big );
    garbageMan.push_back( tiny );
    appIcon = big;
    appIconMini = tiny;
  }
  // Add a few extra hotkeys
  {
    FXAccelTable *table;
    table=getAccelTable();
    if(table){
#ifdef _WIN32
      // Every good win32 app should respnd to Alt-F4
      table->addAccel(fxparseAccel("Alt+F4"),this,FXSEL(SEL_COMMAND,ImageWindow::ID_QUIT));
#endif
    }
  }

  
  // Make menu bar
  menubar=new FXMenuBar(this,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|FRAME_RAISED);

  // Tool bar
  toolbarframe=new FXToolBarShell(this,FRAME_RAISED);
  toolbar=new FXToolBar(this,toolbarframe,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|FRAME_RAISED);
  new FXToolBarGrip(toolbar,toolbar,FXToolBar::ID_TOOLBARGRIP,TOOLBARGRIP_DOUBLE);

  // Status bar
  // statusbar=new FXStatusBar(this,LAYOUT_SIDE_BOTTOM|LAYOUT_FILL_X|STATUSBAR_WITH_DRAGCORNER);
  statusbar = new CustomStatusBar(this,LAYOUT_SIDE_BOTTOM|LAYOUT_FILL_X|STATUSBAR_WITH_DRAGCORNER);
  // Add another status text to statusbar
  statusNavLabel = new FXLabel(statusbar,
    "0 / 0  1.0x\tShowing image 0 out of 0.\nCurrent zoom is %100",
    0,FRAME_SUNKEN|LAYOUT_RIGHT|LAYOUT_CENTER_Y|JUSTIFY_RIGHT);
  
  // Add another status text to statusbar
  statusBlockLabel = new FXLabel(statusbar,"BLOCK",0,FRAME_SUNKEN|LAYOUT_RIGHT|LAYOUT_CENTER_Y|JUSTIFY_RIGHT);
  statusBlockLabel->setBackColor(FXRGB(255,0,0));
  statusBlockLabel->setTextColor(FXRGB(255,255,255));
  // FXFont *f = statusBlockLabel->getFont();
  statusBlockLabel->hide();

  // Add another status text to statusbar
  statusDiffLabel = new FXLabel(statusbar,"DIFF(0)",0,FRAME_SUNKEN|LAYOUT_RIGHT|LAYOUT_CENTER_Y|JUSTIFY_RIGHT);
  statusDiffLabel->setBackColor(FXRGB(0,128,0));
  statusDiffLabel->setTextColor(FXRGB(255,255,255));
  statusDiffLabel->hide();

  // File menu
  filemenu=new FXMenuPane(this);
  new FXMenuTitle(menubar,"&File",NULL,filemenu);
  garbageMan.push_back( (filemenu) );

  // Edit Menu
  editmenu=new FXMenuPane(this);
  new FXMenuTitle(menubar,"&Edit",NULL,editmenu);
  garbageMan.push_back( (editmenu) );

  // Image menu
  imagemenu=new FXMenuPane(this);
  new FXMenuTitle(menubar,"&Image",NULL,imagemenu);
  garbageMan.push_back( (imagemenu) );

  // Channels Menu
  chanmenu=new FXMenuPane(this);
  new FXMenuTitle(menubar,"&Channels",NULL,chanmenu);
  garbageMan.push_back( (chanmenu) );

  // View menu (app? setup?)
  viewmenu=new FXMenuPane(this);
  new FXMenuTitle(menubar,"&View",NULL,viewmenu);
  garbageMan.push_back( (viewmenu) );

  // Help menu
  helpmenu=new FXMenuPane(this);
  new FXMenuTitle(menubar,"&Help",NULL,helpmenu,LAYOUT_RIGHT);
  garbageMan.push_back( (helpmenu) );

  // Splitter
  //splitter=new FXSplitter(this,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y|SPLITTER_TRACKING|SPLITTER_VERTICAL|SPLITTER_REVERSED);

  // Sunken border for image widget
  //FXHorizontalFrame *imagebox=new FXHorizontalFrame(this,FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,0,0,0);
  FXPacker *imagebox=new FXPacker(this,LAYOUT_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,2,0,0);


  //new FXDial(imagebox,&int_target,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|LAYOUT_FILL_ROW|LAYOUT_FIX_WIDTH|DIAL_HORIZONTAL|DIAL_HAS_NOTCH,0,0,100);

  FXIcon *autoscale_icon=new FXBMPIcon(getApp(),autoscale);
  FXIcon *unityscale_icon=new FXBMPIcon(getApp(),unityscale);
  garbageMan.push_back( autoscale_icon );
  garbageMan.push_back( unityscale_icon );
  biasframe = new FXHorizontalFrame(imagebox,LAYOUT_FILL_X|LAYOUT_SIDE_BOTTOM,0,0,0,0, 1,1,0,1);
  FXHorizontalFrame * biashframe = new FXHorizontalFrame(biasframe,LAYOUT_FILL_X|LAYOUT_CENTER_Y,0,0,0,0, 0,0,0,0);
  biasdial      = new FXDial(biashframe,this,ID_IMAGE_BIAS      ,LAYOUT_CENTER_Y|LAYOUT_FILL_X|LAYOUT_FIX_HEIGHT|DIAL_HORIZONTAL|DIAL_HAS_NOTCH,0,0,0,16,0,0,0,0);
  finebiasdial  = new FXDial(biashframe,this,ID_IMAGE_BIAS_FINE ,LAYOUT_CENTER_Y|LAYOUT_FIX_WIDTH|LAYOUT_FIX_HEIGHT|DIAL_HORIZONTAL|DIAL_HAS_NOTCH,0,0,100,20,0,0,0,0);
  new ToolBarToggleButton(
    biasframe,"\tEnable color auto-scale","\tDisable color auto-scale",autoscale_icon,autoscale_icon,this,
    ID_AUTOSCALE,TOGGLEBUTTON_KEEPSTATE|LAYOUT_SIDE_RIGHT|LAYOUT_SIDE_BOTTOM|FRAME_RAISED|FRAME_THICK|LAYOUT_RIGHT);
  new ToolBarButton(
    biasframe,"\tReset color scale to 1.0",unityscale_icon,this,
    ID_UNITYSCALE,LAYOUT_SIDE_RIGHT|LAYOUT_SIDE_BOTTOM|FRAME_RAISED|FRAME_THICK|LAYOUT_RIGHT);
  
  scaleframe = new FXVerticalFrame(imagebox,LAYOUT_FILL_Y|LAYOUT_SIDE_RIGHT,0,0,0,0, 0,0,0,0);
  scaledial = new FXDial(scaleframe,this,ID_IMAGE_SCALE,LAYOUT_FILL_Y|LAYOUT_CENTER_X|DIAL_VERTICAL|DIAL_HAS_NOTCH);
  biasdial->setRange(-99999, 99999);
  biasdial->setValue(0);
  biasdial->setRevolutionIncrement(1000);
  biasdial->setNotchOffset(1000);
  finebiasdial->setRange(-99999, 99999);
  finebiasdial->setValue(0);
  finebiasdial->setRevolutionIncrement(1000);
  finebiasdial->setNotchOffset(1000);
  scaledial->setRange(-99999, 99999);
  scaledial->setValue(0);
  scaledial->setRevolutionIncrement(1000);
  scaledial->setNotchOffset(1000);

  biasdial->setTipText("Change color bias");
  finebiasdial->setTipText("Fine adjustment of color bias");
  scaledial->setTipText("Change color scale");

  // Make image widget
  //imageview=new FXImageView(imagebox,NULL,this,ID_DISPLAY_AREA,LAYOUT_FILL_X|LAYOUT_FILL_Y);
  imageview=new ImageView(imagebox,NULL,FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_X|LAYOUT_FILL_Y);
  imageview->setTarget(this);
  imageview->setSelector(ID_DISPLAY_AREA);
  imageview->enable();
  // Sunken border for file list
  //filebox=new FXHorizontalFrame(splitter,LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,0,0,0);


  // Make file list
  //FXHorizontalFrame* fileframe=new FXHorizontalFrame(filebox,FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,0,0,0, 0,0);
  //filelist=new FXFileList(fileframe,this,ID_FILELIST,LAYOUT_FILL_X|LAYOUT_FILL_Y|ICONLIST_MINI_ICONS|ICONLIST_AUTOSIZE);
  //new FXButton(filebox,"\tUp one level\tGo up to higher directory.",uplevelicon,filelist,FXFileList::ID_DIRECTORY_UP,BUTTON_TOOLBAR|FRAME_RAISED|LAYOUT_FILL_Y);

  // Toobar buttons

  // These first few aren't so useful.
  //new FXButton(toolbar,"&Open\tOpen Image\tOpen an image file.",NULL/*fileopenicon*/,this,ID_OPEN,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  //new FXButton(toolbar,"&Save\tSave Image\tSave current image to file.",NULL/*filesaveicon*/,this,ID_SAVE_AS,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  //new FXButton(toolbar,"&Copy\tCopy Image\tCopy current image to clipboard.",NULL,this,ID_COPY,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  FXIcon *ico = new FXGIFIcon(getApp(), deleteicon);  garbageMan.push_back(ico);
  const char **txt = g_toolbarButtonText;
  new ToolBarButton(toolbar,*txt++,ico,this,ID_DELETE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  //////
  new FXToolBarGrip(toolbar,NULL,0,TOOLBARGRIP_SINGLE);
  ico = new FXGIFIcon(getApp(), arrowfirst);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_FIRST_IMAGE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  ico = new FXGIFIcon(getApp(), arrowprev);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_PREV_IMAGE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  ico = new FXGIFIcon(getApp(), arrownext);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_NEXT_IMAGE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  ico = new FXGIFIcon(getApp(), arrowlast);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_LAST_IMAGE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  ico = new FXGIFIcon(getApp(), arrowprevtag);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_PREV_OF_TITLE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  ico = new FXGIFIcon(getApp(), arrownexttag);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_NEXT_OF_TITLE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);

  new FXToolBarGrip(toolbar,NULL,0,TOOLBARGRIP_SINGLE);
  ico = new FXGIFIcon(getApp(), linkicon);  garbageMan.push_back(ico);
  new ToolBarToggleButton(
    toolbar,*txt,*txt,
    ico,ico,&btLinkImageProps(),FXDataTarget::ID_VALUE,TOGGLEBUTTON_KEEPSTATE|ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  txt++;
  ico = new FXGIFIcon(getApp(), blockicon);  garbageMan.push_back(ico);
  new ToolBarToggleButton(
    toolbar,*txt,*txt,
    ico,ico,&btBlockUpdates(),FXDataTarget::ID_VALUE,TOGGLEBUTTON_KEEPSTATE|ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  txt++;

  new FXToolBarGrip(toolbar,NULL,0,TOOLBARGRIP_SINGLE);
  ico = new FXGIFIcon(getApp(), setdiffbaseicon);  garbageMan.push_back(ico);
  new ToolBarButton(toolbar,*txt++,ico,this,ID_SET_DIFF_BASE,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  ico = new FXGIFIcon(getApp(), difficon);  garbageMan.push_back(ico);
  new ToolBarToggleButton(
    toolbar,*txt,*txt,
    ico,ico,&btDiffImage(),FXDataTarget::ID_VALUE,TOGGLEBUTTON_KEEPSTATE|ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
  txt++;
  
/*
  // Color
  new FXButton(toolbar,"&Colors\tColors\tDisplay color dialog.",paletteicon,colordlg,FXWindow::ID_SHOW,ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED|LAYOUT_RIGHT);
*/
  // File Menu entries
  new FXMenuCommand(filemenu,"&Open...\tCtrl-O\tOpen an image file.",/*fileopenicon*/0,this,ID_OPEN);
  new FXMenuCommand(filemenu,"&Save As...\tCtrl-S\tSave current image to file.",/*filesaveicon*/0,this,ID_SAVE_AS);
  //new FXMenuCommand(filemenu,"Dump",NULL,getApp(),FXApp::ID_DUMP);

  // Recent file menu; this automatically hides if there are no files
  FXMenuSeparator* sep1=new FXMenuSeparator(filemenu);
  sep1->setTarget(&mrufiles);
  sep1->setSelector(FXRecentFiles::ID_ANYFILES);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_1);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_2);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_3);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_4);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_5);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_6);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_7);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_8);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_9);
  new FXMenuCommand(filemenu,NULL,NULL,&mrufiles,FXRecentFiles::ID_FILE_10);
  new FXMenuCommand(filemenu,"&Clear Recent Files",NULL,&mrufiles,FXRecentFiles::ID_CLEAR);
  FXMenuSeparator* sep2=new FXMenuSeparator(filemenu);
  sep2->setTarget(&mrufiles);
  sep2->setSelector(FXRecentFiles::ID_ANYFILES);
  new FXMenuCommand(filemenu,"&Quit\tCtrl-Q",NULL,this,ID_QUIT);

  // Edit Menu entries
  // new FXMenuCommand(editmenu,"&Undo\tCtrl-Z\tUndo last change.");
  // new FXMenuCommand(editmenu,"&Redo\tCtrl-R\tRedo last undo.");
  new FXMenuCommand(editmenu,"&Copy\tCtrl-C\tCopy current image to clipboard.",/*copyicon*/0,this,ID_COPY);
  new FXMenuCommand(editmenu,"C&ut\tCtrl-X\tCut current image to clipboard.",NULL,NULL,0);
  new FXMenuCommand(editmenu,"&Paste\tCtrl-V\tPaste from clipboard.",/*pasteicon*/0,this,ID_PASTE);
  new FXMenuCommand(editmenu,"&Delete\tDel\tDelete current image.",0,this,ID_DELETE);
  new FXMenuSeparator(editmenu);
  new FXMenuCommand(editmenu,"&Options...\t\tChange imdebug options.",NULL,this,ID_OPTIONS,0);

  // Image Menu entries
  new FXMenuCommand(imagemenu,"&Next Image\tDown\tGo to next image",NULL,this,ID_NEXT_IMAGE);
  new FXMenuCommand(imagemenu,"&Previous Image\tUp\tGo to previous image",NULL,this,ID_PREV_IMAGE);
  new FXMenuCommand(imagemenu,"&Last Image\tEnd\tGo to last image",NULL,this,ID_LAST_IMAGE);
  new FXMenuCommand(imagemenu,"&First Image\tHome\tGo to first image",NULL,this,ID_FIRST_IMAGE);
  new FXMenuCommand(imagemenu,"&Next Image by Tag\tPgDn\tGo to the next image with the same tag.",NULL,this,ID_NEXT_OF_TITLE);
  new FXMenuCommand(imagemenu,"&Previous Image by Tag\tPgUp\tGo to the previous image with the same tag",NULL,this,ID_PREV_OF_TITLE);
  new FXMenuSeparator(imagemenu);
  new FXMenuCheck(imagemenu,"Checker &Background\tCtrl+B\tShow checkerboard background",&btCheckerBackground(),FXDataTarget::ID_VALUE,ID_CHECK_BKGND);
  new FXMenuCheck(imagemenu,"Pixel &Grid\tCtrl+G\tShow grid lines around pixels",&btShowPixelGrid(),FXDataTarget::ID_VALUE,ID_SHOW_GRID);
  new FXMenuSeparator(imagemenu);
  {
    FXMenuPane *zoommenu = new FXMenuPane(this);
    garbageMan.push_back( zoommenu );
    for (int id=ID_ZOOM_100; id<=ID_ZOOM_900; id++) {
      FXString msg;
      int num = id-ID_ZOOM_100+1;
      msg.format("Zoom &%d00%%\tShift+%d\tZoom image too %d00%%",num,num,num);
      new FXMenuCommand(zoommenu,msg,NULL,this,id);
    }
    new FXMenuCommand(zoommenu,"Zoom 1&000%\tShift+0\tZoom image to 1000%",NULL,this,ID_ZOOM_1000);
    new FXMenuCascade(imagemenu,"&Zoom",NULL,zoommenu);
  }
  new FXMenuCommand(imagemenu,"Zoom &In\tCtrl+=\tZoom in on image.",NULL,this,ID_ZOOM_IN);
  new FXMenuCommand(imagemenu,"Zoom &Out\tCtrl+-\tZoom out of image",NULL,this,ID_ZOOM_OUT);
  new FXMenuCommand(imagemenu,"Zoo&m to fit\t/\tRescale image to fit right now",NULL,this,ID_RESCALE);
  new FXMenuCheck(imagemenu,"&Auto-zoom\tCtrl+T\tAutomatically zoom images to fit window",&btAutoStretch(),FXDataTarget::ID_VALUE,ID_STRETCH_FIT);
  new FXMenuCheck(imagemenu,"&Flip\tF\tFlip image vertically",&btFlipImage(),FXDataTarget::ID_VALUE,ID_FLIP);

  // Channels Menu entries
  new FXMenuRadio(chanmenu,"RGBA\tCtrl+1\tShow image RGBA channels.", &itEnabledChannels.targ,FXDataTarget::ID_OPTION+ID_RGBA);
  new FXMenuRadio(chanmenu,"RGB\tCtrl+2\tShow image RGB channels.",   &itEnabledChannels.targ,FXDataTarget::ID_OPTION+ID_RGB);
  new FXMenuRadio(chanmenu,"Red\tCtrl+3\tShow image red channel.",    &itEnabledChannels.targ,FXDataTarget::ID_OPTION+ID_RED);
  new FXMenuRadio(chanmenu,"Green\tCtrl+4\tShow image green channel.",&itEnabledChannels.targ,FXDataTarget::ID_OPTION+ID_GREEN);
  new FXMenuRadio(chanmenu,"Blue\tCtrl+5\tShow image blue channel.",  &itEnabledChannels.targ,FXDataTarget::ID_OPTION+ID_BLUE);
  new FXMenuRadio(chanmenu,"Alpha\tCtrl+6\tShow image alpha channel.",&itEnabledChannels.targ,FXDataTarget::ID_OPTION+ID_ALPHA);
  itEnabledChannels().setTarget(this);
  itEnabledChannels().setSelector(ID_RGBA);
  
  // View Menu entries
  new FXMenuCheck(viewmenu,"Show Scale && Bias &Ctrls\t\tShow sliders for image scale and bias",&btShowManipCtrls(),FXDataTarget::ID_VALUE,ID_SHOW_SCALE_BIAS);
  new FXMenuCheck(viewmenu,"Show &Statusbar\t\tShow the statusbar",statusbar,FXWindow::ID_TOGGLESHOWN);
  new FXMenuCheck(viewmenu,"Show &Toolbar\t\tShow the toolbar",toolbar,FXWindow::ID_TOGGLESHOWN);
  new FXMenuCheck(viewmenu,"Show Te&xt on Toolbar\t\tShow text labels on the toolbar",&btShowToolbarText(),FXDataTarget::ID_VALUE,ID_SHOW_TOOLBAR_TEXT);
  new FXMenuSeparator(viewmenu);
  new FXMenuCheck(viewmenu,"Block &Updates\tCtrl+I\tIgnore image updates from client",&btBlockUpdates(),FXDataTarget::ID_VALUE);
  new FXMenuCheck(viewmenu,"Share Output &Window\t\tReuse this window for all clients",&btShareOutputWindow(),FXDataTarget::ID_VALUE,ID_SHARE_WINDOW);
  new FXMenuCheck(viewmenu,"&Link Images\t\tLink scale, translation, etc of all images with the same title",
    &btLinkImageProps(),FXDataTarget::ID_VALUE,ID_LINK_PROPS);
  new FXMenuCheck(viewmenu,"&Replace Images\t\tSave new images in same slot as previous image if titles match",&btOverwriteImages(),FXDataTarget::ID_VALUE,ID_OVERWRITE_IMAGES);
  new FXMenuCheck(viewmenu,"&Hold on Image\tCtrl+J\tDo not automatically jump to the newest images",&btHoldOnImage(),FXDataTarget::ID_VALUE,ID_HOLD_ON_IMAGE);

  // Help Menu entries
  new FXMenuCommand(helpmenu,"&About Imdebug...",NULL,this,ID_ABOUT,0);

  // Make a tool tip
  //new FXToolTip(getApp(),TOOLTIP_NORMAL);
  tooltip = new CustomToolTip(getApp(),TOOLTIP_NORMAL);

  // Recent files
  mrufiles.setTarget(this);
  mrufiles.setSelector(ID_RECENTFILE);

  // Initialize file name
  filename="untitled";
  itEnabledChannels = ID_RGBA;
  btBlockUpdates = 0;
  btDiffImage = 0;

  btBlockUpdates().setTarget(this);       btBlockUpdates().setSelector(ID_BLOCK_UPDATES);
  btCheckerBackground().setTarget(this);  btCheckerBackground().setSelector(ID_CHECK_BKGND);
  btAutoStretch().setTarget(this);        btAutoStretch().setSelector(ID_STRETCH_FIT);
  btShowPixelGrid().setTarget(this);      btShowPixelGrid().setSelector(ID_SHOW_GRID);
  btFlipImage().setTarget(this);          btFlipImage().setSelector(ID_FLIP);
  btShareOutputWindow().setTarget(this);  btShareOutputWindow().setSelector(ID_SHARE_WINDOW);
  btLinkImageProps().setTarget(this);     btLinkImageProps().setSelector(ID_LINK_PROPS);
  btOverwriteImages().setTarget(this);    btOverwriteImages().setSelector(ID_OVERWRITE_IMAGES);
  btShowManipCtrls().setTarget(this);     btShowManipCtrls().setSelector(ID_SHOW_SCALE_BIAS);
  //btShowToolbar().setTarget(this);        btShowToolbar().setSelector(ID_SHOW_TOOLBAR);
  btShowToolbarText().setTarget(this);    btShowToolbarText().setSelector(ID_SHOW_TOOLBAR_TEXT);
  //btShowStatusbar().setTarget(this);      btShowStatusbar().setSelector(ID_SHOW_STATUSBAR);
  btDiffImage().setTarget(this);          btDiffImage().setSelector(ID_DIFF_IMAGE);


  //btPropagateScaleOnTagged().setTarget(this);
  //btPropagateScaleOnTagged().setSelector(ID_);

  //!! Change to readonly when signalling from main app working!!!
  mappedFile = new MemoryMappedFile(
    IMDBG_MEMMAP_FILENAME,MemoryMappedFile::OPEN_READ
    );


  initBlitFuncs();

}


// Clean up
ImageWindow::~ImageWindow()
{
  ObjectCleanup::iterator i = garbageMan.begin();
  ObjectCleanup::iterator end = garbageMan.end();
  for (; i!=end; ++i) 
    delete *i;

  if (pDiffData)
      delete [] pDiffData;
}


// About box
long ImageWindow::onCmdAbout(FXObject*,FXSelector,void*){
  FXMessageBox about(
    this,"About the Image Debugger",
    "The Image Debugger " IMDBG_APP_VERSION_STRING " - "
    IMDBG_APP_DATE "\n\n"
    "Image Display Application.\n\n"
    "Using the FOX C++ GUI Library (http://www.fox-toolkit.org)\n\n"
    "Copyright (C) 2003-2005 Bill Baxter (baxter@cs.unc.edu)",
    appIcon,MBOX_OK|DECOR_TITLE|DECOR_BORDER);
  about.execute();
  return 1;
}

void imageToImprops(FXImage *img, ImProps *props, const FXString &name)
{
  props->VERSION = IMDBG_PROTOCOL_VERSION;
  props->width = img->getWidth();
  props->height= img->getHeight();
  FXuint opts = img->getOptions();
  props->nchan = 4;
  props->bpc[0] = 8;
  props->bpc[1] = 8;
  props->bpc[2] = 8;
  props->bpc[3] = 8;
  props->bitoffset[0]=0;
  props->bitoffset[1]=8;
  props->bitoffset[2]=16;
  props->bitoffset[3]=24;
  props->type[0] = IMDBG_UINT;
  props->type[1] = IMDBG_UINT;
  props->type[2] = IMDBG_UINT;
  props->type[3] = IMDBG_UINT;
  props->chanmap[0] = 0;
  props->chanmap[1] = 1;
  props->chanmap[2] = 2;
  props->chanmap[3] = 3;
  props->colstride = 32;
  props->rowstride = 32*props->width;
  props->colgap = 0;
  props->rowgap = 0;
  memset(props->bias, 0, sizeof(float)*4);
  float *s= props->scale;
  s[0]=s[1]=s[2]=s[3]=1.0;
  strncpy(props->title,name.text(),MAX_TITLE_LENGTH);
  props->title[MAX_TITLE_LENGTH]=0;
  props->flags = 0;
  
}
// Load file
FXbool ImageWindow::loadimage(const FXString& file){
  FXString ext=FXFile::extension(file);
  FXImage *img=NULL;
  FXImage *old;
  if(comparecase(ext,"gif")==0){
    img=new FXGIFImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
  else if(comparecase(ext,"bmp")==0){
    img=new FXBMPImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
  else if(comparecase(ext,"xpm")==0){
    img=new FXXPMImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
  else if(comparecase(ext,"pcx")==0){
    img=new FXPCXImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
  else if(comparecase(ext,"ico")==0){
    img=new FXICOImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
  else if(comparecase(ext,"tga")==0){
    img=new FXTGAImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
  else if(comparecase(ext,"rgb")==0){
    img=new FXRGBImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
#ifdef HAVE_PNG_H
  else if(comparecase(ext,"png")==0){
    img=new FXPNGImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
#endif
#ifdef HAVE_JPEG_H
  else if(comparecase(ext,"jpg")==0){
    img=new FXJPGImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
#endif
#ifdef HAVE_TIFF_H
  else if(comparecase(ext,"tif")==0 || comparecase(ext,"tiff")==0){
    img=new FXTIFImage(getApp(),NULL,IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP);
    }
#endif

  // Perhaps failed
  if(img==NULL){
    FXMessageBox::error(this,MBOX_OK,"Error Loading Image","Unsupported type: %s",ext.text());
    return false;
    }

  // Load it
  FXFileStream stream;
  if(stream.open(file,FXStreamLoad))
  {
    getApp()->beginWaitCursor();
    img->loadPixels(stream);
    stream.close();

    img->create();
    old=imageview->getImage();
    imageview->setImage(img);
    delete old;
    
    // now copy image into an imdebug buffer
    addNewImageSlotToCircularBuffer();
    ImPropsEx *pProps = &improps[curImage];
    Byte **rawData = &pRawData[curImage];
    imageToImprops(img, pProps,FXFile::name(file));
    int rawSize = computeRawDataSize(pProps);
    // Free old data
    if (*rawData != NULL) {
      free (*rawData);
      *rawData = NULL;
    }

    if (rawSize == 0) {
      *rawData = 0;
      pProps->nchan = ERROR_ZERO_PIXELS;
    }
    else {
      *rawData = new Byte[rawSize];
      if (!*rawData) {
        FXMessageBox::error(this,MBOX_OK,"Imdebug Error","Error in allocating memory for image\n");
        pProps->nchan = ERROR_ALLOC_FAIL;
      }
      else {
        memcpy(*rawData, img->getData(), rawSize);
      }
    }
    pProps->tx = (imageview->getWidth()-img->getWidth())>>1;
    pProps->ty = (imageview->getHeight()-img->getHeight())>>1;
    pProps->zoom = 1.0;
    pProps->globalImgNum = imagesDisplayed;
    drawAttr.calcZoom = btAutoStretch ? true : false;
    regenerateDisplayImage();
    syncScaleBiasDials();
    
    imagesDisplayed++;

    setImageCountStatus();
    setNormalImageInfoStatus();
    
    getApp()->endWaitCursor();
  }
  return true;
}


// Save file
FXbool ImageWindow::saveimage(const FXString& file){
  FXString ext=FXFile::extension(file);
  FXImage *img = 0;
  FXImage *old = imageview->getImage();
  FXuint opt = IMAGE_OWNED|IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP;
  FXint w = old->getWidth();
  FXint h = old->getHeight();
  FXColor *olddata = old->getData();
  if(comparecase(ext,"gif")==0){
    img=new FXGIFImage(getApp(),NULL,opt,w,h);
    }
  else if(comparecase(ext,"bmp")==0){
    img=new FXBMPImage(getApp(),NULL,opt,w,h);
    }
//  else if(comparecase(ext,"xpm")==0){
//    img=new FXXPMImage(getApp(),old->getData(),opt,w,h);
//    }
  else if(comparecase(ext,"pcx")==0){
    img=new FXPCXImage(getApp(),NULL,opt,w,h);
    }
  else if(comparecase(ext,"ico")==0){
    img=new FXICOImage(getApp(),NULL,opt,w,h);
    }
  else if(comparecase(ext,"tga")==0){
    img=new FXTGAImage(getApp(),NULL,opt,w,h);
    }
  else if(comparecase(ext,"rgb")==0){
    img=new FXRGBImage(getApp(),NULL,opt,w,h);
    }
#ifdef HAVE_PNG_H
  else if(comparecase(ext,"png")==0){
    img=new FXPNGImage(getApp(),NULL,opt,w,h);
    }
#endif
#ifdef HAVE_JPEG_H
  else if(comparecase(ext,"jpg")==0){
    img=new FXJPGImage(getApp(),NULL,opt,w,h);
    }
#endif
#ifdef HAVE_TIFF_H
  else if(comparecase(ext,"tif")==0 || comparecase(ext,"tiff")==0){
    img=new FXTIFImage(getApp(),NULL,opt,w,h);
    }
#endif

  // Perhaps failed
  if(img==NULL){
    FXMessageBox::error(this,MBOX_OK,"Error Loading Image","Unsupported type: %s",ext.text());
    return false;
  }
  img->create();
  // copy the data now.
  FXColor *dst = img->getData();
  FXColor *src = old->getData();
  int sz = w*h;
  for (int i=0;i<sz; ++i ) 
    *dst++ = *src++;
  img->render();

  FXFileStream stream;
  if(stream.open(file,FXStreamSave)){
    getApp()->beginWaitCursor();
    //img=imageview->getImage();///// We may want to save in another format...
    // crop it to just image part.
    int l,r,t,b;
    ImPropsEx *p = &improps[curImage];
    mapImageToWindow(0,0,&l,&t,&drawAttr,p);
    mapImageToWindow(p->width,p->height,&r,&b,&drawAttr,p);
    int nw = img->getWidth();
    int nh = img->getHeight();
    if (l<0) l=0;
    if (t<0) t=0;
    if (b>nh) b = nh;
    if (r>nw) r = nw;
    img->crop(l,t,r-l,b-t);
    img->savePixels(stream);
    stream.close();
    // regenerateDisplayImage();
    delete img;
    getApp()->endWaitCursor();
  }
  return true;
}

void ImageWindow::setImageCountStatus()
{
  // set right part of statusbar message with navigation info
  // Normally we just display a global image number for every image
  // But if we're doing image replacement then it's better to set it
  // to show which image this is out of the similar images shown
  char msg[200];
  if (numImages==0){
    sprintf(msg, "0 / %d      ", imagesDisplayed);
    statusNavLabel->setText(msg);
    sprintf(msg, "A total of %d images have been shown.", imagesDisplayed);
    statusNavLabel->setTipText(msg);
  }
  else if (!btOverwriteImages)
  {
    int cur = curImage;
    int last = lastImage;
    int first = firstImage;
    int total = imagesDisplayed;
    float zoom = improps[cur].zoom;
    int disp = improps[cur].globalImgNum+1;
    sprintf(msg, "%d / %d  %1.1fx", disp, total, zoom);
    statusNavLabel->setText(msg);
    sprintf(msg, 
      "Showing image %d out of %d.\n"
      "Current zoom is %1.1fx", disp, total, zoom);
    statusNavLabel->setTipText(msg);
  }
  else
  {
    int cur = curImage;
    int last = lastImage;
    int first = firstImage;
    int total = imagesDisplayed;
    float zoom = improps[cur].zoom;
    int disp = improps[cur].globalImgNum+1;
    sprintf(msg, "%d / %d (%d) %1.1fx", disp, total, improps[cur].imgNum+1, zoom);
    statusNavLabel->setText(msg);
    sprintf(msg, 
      "Showing image %d out of %d.\n"
      "(Image #%d with this title)\n"
      "Current zoom is %1.1fx", disp,total, improps[cur].imgNum+1, zoom);
    statusNavLabel->setTipText(msg);
  }
}


static bool getValueOfTypeAndSize(unsigned char *p, char t, int s, int off, 
                                  long* lv, double *dv)
{
  //if (s&7) return false; // eek! non-integral byte width size!
  switch(t) {
  case IMDBG_UINT:
    switch(s) {
    case 8:   *lv = *((unsigned char*)p); return true;
    case 16:  *lv = *((unsigned short*)p); return true;
    case 32:  *lv = *((unsigned int*)p); return true;
      // case 64:  *lv = *((unsigned long*)p); return true;
    default: 
      *lv = bytePtr2Val_BitOffsets(p, off, s);
      return true;
    }
    break;
  case IMDBG_INT:
    switch(s) {
    case 8:   *lv = *((char*)p); return true;
    case 16:  *lv = *((short*)p); return true;
    case 32:  *lv = *((int*)p); return true;
    case 64:  *lv = *((long*)p); return true;
    default: return false;
    }
    break;
  case IMDBG_FLOAT:
    switch(s) {
    case 32:  *dv = *((float*)p); return true;
    case 64:  *dv = *((double*)p); return true;
    default: return false;
    }
    break;
  }
  return false;
}


void ImageWindow::writeStatusMessage(
  char *msg, char *tmsg, int ImgX, int ImgY, 
  Byte*rawptr, ImPropsEx *p, DrawAttribs *pAttr)
{
  int l = sprintf(msg,"(%d,%d)", ImgX, ImgY);
  int tl = sprintf(tmsg,"<#XXXXXX>\t (%d,%d)=",ImgX,ImgY);

  // draw scale and bias for toolbar here (later for statusbar)
  // FIXME IF PER-CHANNEL SCALE & BIAS IS IMPLEMENTED!!!
  if (p->scale[0] != 1.0) {
    tl += sprintf(tmsg+tl," *%.2g", p->scale[0]);
  }
  if (p->bias[0] != 0) {
    tl += sprintf(tmsg+tl," %+.2g", p->bias[0]);
  }
  tl += sprintf(tmsg+tl,"\n");


  Byte *base = rawptr;
  base += (p->rowstride>>3)*ImgY + (p->colstride>>3)*ImgX;
  assert(base - rawptr < p->rawImageSize);

  if (!(p->rowstride&7)&&!(p->colstride&7) && p->nchan>0) 
  {
    const char *colormap[] = {
      "<R>","<G>","<B>","<A>","<0>",
    /*
      "(180,50,50)",
      "(30,140,30)",
      "(50,50,200)",
      "(100,100,100)",
      "(0,0,0)",
    */
      0
    };
    char colorchar[] = "RGBA";

    int colors[4];
    int numcolors = 0;
    bool isGrayScale;
    char effectiveChanmap[4];
    memcpy(effectiveChanmap, p->chanmap, 4*sizeof(char));
    if (! (pAttr->enabledChannels & pAttr->ENABLE_RED))
      effectiveChanmap[0] = -1;
    if (! (pAttr->enabledChannels & pAttr->ENABLE_GREEN))
      effectiveChanmap[1] = -1;
    if (! (pAttr->enabledChannels & pAttr->ENABLE_BLUE))
      effectiveChanmap[2] = -1;
    if (! (pAttr->enabledChannels & pAttr->ENABLE_ALPHA))
      effectiveChanmap[3] = -1;
    // Make alpha grayscale if that's all there is
    if (pAttr->enabledChannels == pAttr->ENABLE_ALPHA) {
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
    tl += sprintf(tmsg+tl, "  [");
    base = rawptr;
    base += (p->rowstride>>3)*ImgY + (p->colstride>>3)*ImgX;
    assert(base - rawptr < p->rawImageSize);
    double typemax = getTypemax(p);
    Byte swatchrgb[4]={0,0,0,0};
    int swatchval;
    for (int c = 0; c<p->nchan; c++)
    {
      long lv;
      double dv;
      double scale = p->scale[0];
      double bias  = p->bias[0];
      assert(base - rawptr + (p->bitoffset[c]>>3) < p->rawImageSize);
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
        if (c == effectiveChanmap[3] //|| 
            /*(pAttr->enabledChannels == pAttr->ENABLE_ALPHA 
              && c==p->chanmap[3] )*/ )
        {
          colors[numcolors++] = 3; //alpha
        }
        if (numcolors==0) {
          numcolors=1;
          colors[0] = 4;  // black
        }
        {
          if (numcolors==1 && !isGrayScale) {
            l += sprintf(msg+l, "%s", colormap[colors[0]]);
            tl += sprintf(tmsg+tl, "%s", colormap[colors[0]]);
          }
          else if (numcolors>1 && !isGrayScale)
          {
            int k;
            for (k=0; k<numcolors; k++) {
              l += sprintf(msg+l, "%s%c", colormap[colors[k]],*(colorchar+colors[k]));
              tl += sprintf(tmsg+tl, "%s%c", colormap[colors[k]],*(colorchar+colors[k]));
            }
            l += sprintf(msg+l, "%s ", colormap[4]);
            tl += sprintf(tmsg+tl, "%s ", colormap[4]);
          }
          else {
            l += sprintf(msg+l, "%s", colormap[4]);
            tl += sprintf(tmsg+tl, "%s", colormap[4]);
          }
        }
        if (p->type[c]==IMDBG_UINT||p->type[c]==IMDBG_INT) {
          l += sprintf(msg+l, "%ld, ", lv);
          tl += sprintf(tmsg+tl, "%ld, ", lv);
          swatchval = (lv*scale+bias)/( typemax/255.0f);
        }
        else if (p->type[c]==IMDBG_FLOAT) {
          l += sprintf(msg+l, "%g, ", dv);
          tl += sprintf(tmsg+tl, "%g, ", dv);
          swatchval = 255.0*(dv*scale+bias)/typemax;
        }
        else {
          l += sprintf(msg+l, "?, ");
          tl += sprintf(tmsg+tl, "?, ");
          swatchval = -1;
        }
      }
      else {
        l += sprintf(msg+l, "?, ");
        tl += sprintf(tmsg+tl, "?, ");
        swatchval = -1;
      }
      if (swatchval < 0) swatchval = 0;
      if (swatchval > 255) swatchval = 255;
      for (int i=0; i<numcolors; i++) {
        if (colors[i]<4)
          swatchrgb[colors[i]] = swatchval;
      }
    }
    l-=2;
    tl-=2;
    {
      l += sprintf(msg+l, "%s", colormap[4]);
      tl += sprintf(tmsg+tl, "%s", colormap[4]);
    }

    l += sprintf(msg+l,"]");
    tl += sprintf(tmsg+tl,"]");

    // draw scale and bias for statusbar msg at end
    // FIXME IF PER-CHANNEL SCALE & BIAS IS IMPLEMENTED!!!
    if (p->scale[0] != 1.0) {
      l += sprintf(msg+l," *%.4g", p->scale[0]);
    }
    if (p->bias[0] != 0) {
      l += sprintf(msg+l," %+.4g", p->bias[0]);
    }
    char numtmp[10];
    sprintf(numtmp,"%02x%02x%02x", swatchrgb[0],swatchrgb[1],swatchrgb[2]);
    strncpy(tmsg+2, numtmp,6);
  }
  
}

void ImageWindow::setHoverImageInfoStatus(int PtrX, int PtrY)
{
  if (numImages<=0) return;
  char msg[256] = "Ready";
  char tipmsg[256] = "Ready";
  int xImg, yImg;
  int WinW = imageview->getWidth();
  int WinH = imageview->getHeight();
  
  ImPropsEx *pprops = &improps[curImage];

  Byte *pImage = 0;
  
  if (btDiffImage && bDiffValid && (pDiffData != 0) && (improps != 0)) {
      pImage = pDiffData;
      pprops = &improps[diffImage];
  }
  else if (pRawData != 0 && (improps != 0)) {
      pImage = pRawData[curImage];
      pprops = &improps[curImage];
  }

  mapWindowToImage(PtrX,PtrY,WinH,&xImg,&yImg,&drawAttr,pprops);
  if (PtrX > 0 && xImg>=0 && xImg < pprops->width &&
      PtrY > 0 && yImg>=0 && yImg < pprops->height && pImage != 0)//*/pRawData!=0) 
  { 
    writeStatusMessage(
      msg,tipmsg,xImg,yImg,pImage/*pRawData[curImage]*/,pprops/*&improps[curImage]*/,&drawAttr      
      );
    imageview->setHelpText(msg);
    imageview->setTipText(tipmsg);
    tooltip->setStyle( TOOLTIP_PERMANENT );
  }        
  else {
    onLeave(this,0,0);
  }
  lastHoverX=PtrX;
  lastHoverY=PtrY;
}
void ImageWindow::setHoverImageInfoStatus()
{
  setHoverImageInfoStatus(lastHoverX,lastHoverY);
}

void ImageWindow::setNormalImageInfoStatus()
{
  char msg[100] = "Ready";
  // when there's nothing particular to show... show image stats
    
  int l=0,i;
  ImProps *pprops = &improps[curImage];
  int bpc=pprops->bpc[0];
  int type=pprops->type[0];
  if (pprops->nchan <= 0)
  {
    int err = pprops->nchan;
    // This is some sort of error condition
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
    else {
      strcpy(msg, "Unknown error");
    }
  }
  else
  {
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
  }
  statusbar->getStatusLine()->setNormalText( FXString(msg) );
}


// Open
long ImageWindow::onCmdOpen(FXObject*,FXSelector,void*){
  FXFileDialog open(this,"Open Image");
  open.setFilename(filename);
  open.setPatternList(patterns);
  open.setIcon(appIcon);
  if(open.execute()){
    filename=open.getFilename();
    mrufiles.appendFile(filename);
    loadimage(filename);
    }
  return 1;
}


// Save
long ImageWindow::onCmdSaveAs(FXObject*,FXSelector,void*){
  FXFileDialog savedialog(this,"Save Image");
  savedialog.setFilename(filename);
  savedialog.setIcon(appIcon);
  if(savedialog.execute()){
    if(FXFile::exists(savedialog.getFilename())){
      if(MBOX_CLICKED_NO==FXMessageBox::question(this,MBOX_YES_NO,"Overwrite Image","Overwrite existing image?")) return 1;
      }
    filename=savedialog.getFilename();
    mrufiles.appendFile(filename);
    saveimage(filename);
    }
  return 1;
}


// Quit
long ImageWindow::onCmdQuit(FXObject*,FXSelector,void*){
  FXTRACE((100,"Quit\n"));

  // Write new window size back to registry
  FXRegistry &reg = getApp()->reg();
  reg.writeIntEntry("SETTINGS","x",getX());
  reg.writeIntEntry("SETTINGS","y",getY());
  reg.writeIntEntry("SETTINGS","width",getWidth());
  reg.writeIntEntry("SETTINGS","height",getHeight());

  reg.writeIntEntry("SETTINGS","history",historySize);
  reg.writeIntEntry("SETTINGS","copyscale",btPropagateScaleOnTagged);
  reg.writeIntEntry("SETTINGS","linkprops",btLinkImageProps);
  
  reg.writeIntEntry("SETTINGS","check",       btCheckerBackground  );
  reg.writeIntEntry("SETTINGS","stretch",     btAutoStretch        );
  reg.writeIntEntry("SETTINGS","grid",        btShowPixelGrid      );
  reg.writeIntEntry("SETTINGS","flip",        btFlipImage          );
  reg.writeIntEntry("SETTINGS","shareDisplay",btShareOutputWindow  );
  reg.writeIntEntry("SETTINGS","linkprops",   btLinkImageProps     );
  reg.writeIntEntry("SETTINGS","overwrite",   btOverwriteImages    );
  reg.writeIntEntry("SETTINGS","holdonimage", btHoldOnImage        );
  reg.writeIntEntry("SETTINGS","sbControls",  btShowManipCtrls     );
  reg.writeIntEntry("SETTINGS","statusbar",   statusbar->shown()   );
  reg.writeIntEntry("SETTINGS","toolbar",     toolbar->shown()     );
  reg.writeIntEntry("SETTINGS","toolbartext", btShowToolbarText    );
  const char *dockSide = "top";
  switch (toolbar->getDockingSide()) {
  case LAYOUT_SIDE_BOTTOM: dockSide = "bottom";  break;
  case LAYOUT_SIDE_LEFT:   dockSide = "left"  ;  break;
  case LAYOUT_SIDE_RIGHT:  dockSide = "right" ;  break;
  }
  reg.writeStringEntry("SETTINGS","tbdockside",dockSide);

  // Width of tree
  //getApp()->reg().writeIntEntry("SETTINGS","fileheight",filebox->getHeight());

  // Was file box shown
  //getApp()->reg().writeIntEntry("SETTINGS","filesshown",filebox->shown());

  //FXString dir=filelist->getDirectory();
  //getApp()->reg().writeStringEntry("SETTINGS","directory",dir.text());

  // Quit
  getApp()->exit(0);
  return 1;
}

// Copy to clipboard
long ImageWindow::onCmdCopy(FXObject* sender,FXSelector,void*)
{
  if (numImages<=0) return 0;
#ifdef _WIN32
  //CopyImageToWin32Clipboard();
/*
  if (OpenClipboard( this->getWindow() ) {
    EmptyClipboard();
    // It seems to help to pad the data some, and to
    // not use the negative height value.  dunno why.
    // this clipboard stuff is baffling.
    
    clipHandle = SetClipboardData(CF_DIB, GetDC);
    CloseClipboard();
  }
*/  
#endif
  return 1;
}

// Paste image from clipboard
long ImageWindow::onCmdPaste(FXObject* sender,FXSelector,void*)
{

  return 1;
}

// Delete current image
long ImageWindow::onCmdDelete(FXObject* sender,FXSelector,void*)
{
  if (numImages<=0) return 0;
  // remove current image from list
  delete pRawData[curImage];
  int prev = -1;
  int end = (lastImage+1)%historySize;

  for (int i=curImage; i!=end; i=(i+1)%historySize)
  {
    if (prev!=-1) {
      pRawData[prev] = pRawData[i];
      improps[prev]  = improps[i];
    }
    prev = i;
  }
  numImages--;
  if (curImage!=lastImage) {
    lastImage--;
    if (lastImage<0) lastImage=historySize-1;
  }
  else if (numImages>0){
    curImage--;
    if (curImage<0) curImage=historySize-1;
    lastImage = curImage;
  }
  regenerateDisplayImage();
  syncScaleBiasDials();
  setImageCountStatus();
  setNormalImageInfoStatus();
  setHoverImageInfoStatus();
  return 1;
}

// Update title
long ImageWindow::onUpdTitle(FXObject* sender,FXSelector,void*){
  FXString title;
  if (numImages>0) {
    ImPropsEx *p = &improps[curImage];
    if (p->title[0] != 0) {
      title.format("%s (%d/%d) - ", p->title, p->globalImgNum+1, imagesDisplayed);
    }
    else {
      title.format("(%d/%d) - ", p->globalImgNum+1, imagesDisplayed);
    }
  }
  title += FXString(IMDBG_DISPLAY_WINDOW_TITLE);
//  FXImage* image=imageview->getImage();
//  if(image){ title+=" (" + FXStringVal(image->getWidth()) + " x " + FXStringVal(image->getHeight()) + ")"; }
  sender->handle(this,FXSEL(SEL_COMMAND,FXWindow::ID_SETSTRINGVALUE),(void*)&title);
  return 1;
}
/*
long ImageWindow::onQueryTip(FXObject*sender,FXSelector,void*)
{
  if(!tooltipString.empty()){
    sender->handle(this,FXSEL(SEL_COMMAND,ID_SETSTRINGVALUE),&tooltipString);
    return 1;
  }
  return 0;
}

long ImageWindow::onQueryStatus(FXObject*sender,FXSelector,void*)
{
  if(!statusHelpString.empty()){
    sender->handle(this,FXSEL(SEL_COMMAND,ID_SETSTRINGVALUE),&statusHelpString);
    return 1;
  }
  return 0;
}
*/

// Open recent file
long ImageWindow::onCmdRecentFile(FXObject*,FXSelector,void* ptr){
  filename=(FXchar*)ptr;
//  filelist->setCurrentFile(filename);
  loadimage(filename);
  return 1;
}

long ImageWindow::onCmdOptions(FXObject* sender,FXSelector sel,void* ptr)
{
  FXDialogBox dlg(this,"Imdebug Options", DECOR_TITLE|DECOR_BORDER|DECOR_CLOSE);
  dlg.setIcon(appIconMini);
  FXVerticalFrame*vframe=new FXVerticalFrame(&dlg,LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0,0,0,0,0);

  FXGroupBox *gbox=new FXGroupBox(vframe,"Options",FRAME_GROOVE|LAYOUT_FILL_X|LAYOUT_FILL_Y);

  FXVerticalFrame*frame1=new FXVerticalFrame(gbox,LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0,0,0,0,0);
 
  // HISTORY 
  FXHorizontalFrame*hframe=new FXHorizontalFrame(frame1,LAYOUT_FILL_X);
  FXString histTip( "Number of images kept in history buffer" );
  new FXLabel(hframe,FXString("Image &history:\t")+histTip,NULL,LAYOUT_SIDE_LEFT|LAYOUT_CENTER_Y);
  FXSpinner* hist=new FXSpinner(hframe,5,NULL,0,JUSTIFY_RIGHT|FRAME_SUNKEN|FRAME_THICK|LAYOUT_SIDE_LEFT|LAYOUT_CENTER_Y);
  hist->setTipText(histTip);
  hist->setValue(historySize);
  hist->setRange(1,10000);

  // PROPAGATE SCALE
  FXCheckButton* prop=new FXCheckButton(frame1,
    "Propagate scale && bias\tCopy scale and bias values\nto new images that have the same\ntitle as a previous image."
    );
  prop->setCheck( btPropagateScaleOnTagged );
  // YES NO BUTTONS
  FXHorizontalFrame *btnframe=new FXHorizontalFrame(vframe,LAYOUT_FILL_X);
  new FXButton(btnframe,"  &OK  ",NULL,&dlg,FXDialogBox::ID_ACCEPT,BUTTON_INITIAL|BUTTON_DEFAULT|FRAME_RAISED|FRAME_THICK|LAYOUT_SIDE_LEFT|LAYOUT_CENTER_Y|LAYOUT_FIX_WIDTH|LAYOUT_RIGHT,0,0,74);
  new FXButton(btnframe,"&Cancel",NULL,&dlg,FXDialogBox::ID_CANCEL,BUTTON_DEFAULT|FRAME_RAISED|FRAME_THICK|LAYOUT_SIDE_LEFT|LAYOUT_CENTER_Y|LAYOUT_FIX_WIDTH|LAYOUT_RIGHT,0,0,74);

  FXint oldnr,oldnc;
  oldnr=historySize;
  oldnc=btPropagateScaleOnTagged;
  if(dlg.execute()){
    int nhist = hist->getValue();
    if (historySize != nhist)
    {
      // Just throw away all the data when they resize the buffer.
      // So much easier that way.
      for (int i=0; i<historySize; i++) {
        if (pRawData[i]) delete[](pRawData[i]);
      }
      // resize history buffer
      delete [] pRawData;
      delete [] improps;
      pRawData = new Byte*[nhist];
      improps  = new ImPropsEx[nhist];
      memset(pRawData, 0, nhist*sizeof(BYTE*));
      memset(improps, 0, nhist*sizeof(ImProps));
      historySize = nhist;
      
      // reset all history parameters
      imagesDisplayed = 0;
      numImages = 0;
      curImage = 0;
      firstImage = 0;
      lastImage = 0;

      diffImage = 0;
      
      setImageCountStatus();
      regenerateDisplayImage();
    }
    if (btPropagateScaleOnTagged.val != prop->getCheck())
    {
      btPropagateScaleOnTagged = prop->getCheck();
    }
  }
  
  /*
  OptionsDialog::Options opt;
  opt.historySize = historySize;
  OptionsDialog *dlg = new OptionsDialog(this, "Imdebug Options", opt);
  FXuint ret = dlg->execute();
  if (ret == FXDialogBox::ID_ACCEPT) {
    historySize = opt.historySize;
  }
  delete dlg;
  */
  return 1;
}
long ImageWindow::onCmdNavigate(FXObject* sender,FXSelector sel,void* ptr)
{
  if (numImages<0) return 0;
  int idx;
  switch(FXSELID(sel)){
  case ID_NEXT_IMAGE: 
    // switch to next output image
    if ( curImage != lastImage )
      ++curImage; curImage%=historySize;
    break;
  case ID_PREV_IMAGE: 
      // switch to previously output image
      if ( curImage!=firstImage ) --curImage;
      if (curImage<0) curImage = historySize-1;
    break;
  case ID_NEXT_OF_TITLE:
    idx = findNextImageIndexByTag(improps[curImage].title, curImage);
    if (idx>=0) curImage = idx;
    break;
  case ID_PREV_OF_TITLE:
    idx = findPreviousImageIndexByTag(improps[curImage].title, curImage);
    if (idx>=0) curImage = idx;
    break;
  case ID_FIRST_IMAGE:
    curImage = firstImage;
    break;
  case ID_LAST_IMAGE:
    curImage = lastImage;
    break;
  }
  drawAttr.calcZoom = false;
  drawAttr.stretched = fabs (improps[curImage].zoom - 1.0f) > 1e-7;
  regenerateDisplayImage();
  syncScaleBiasDials();
  setImageCountStatus();
  setNormalImageInfoStatus();
  setHoverImageInfoStatus();
  return 1;
}

long ImageWindow::onCmdToggles(FXObject* sender,FXSelector sel,void* ptr)
{
  switch(FXSELID(sel)) {
  case ID_BLOCK_UPDATES:
    //blockUpdates ^= 1;
    if (btBlockUpdates.val) {
      statusBlockLabel->show();
    } else {
      statusBlockLabel->hide();
    }
    statusbar->recalc();
    break;
  case ID_SHARE_WINDOW:
    break;
  case ID_LINK_PROPS:
    break;
  case ID_FLIP:
    drawAttr.flipped = btFlipImage ? true : false;
    regenerateDisplayImage();
    break;
  case ID_STRETCH_FIT:
    break;
  case ID_SHOW_GRID:
    regenerateDisplayImage();
    break;
  case ID_SHOW_SCALE_BIAS:
    if (btShowManipCtrls) {
      scaleframe->show();
      biasframe->show();
    }
    else {
      scaleframe->hide();
      biasframe->hide();
    }
    scaleframe->recalc();
    break;
  case ID_SHOW_TOOLBAR:
    if (toolbar->shown()) {
      toolbar->hide();
    }
    else {
      toolbar->show();
    }
    recalc();
    break;
  case ID_SHOW_TOOLBAR_TEXT:
    {
      FXWindow *win = toolbar->getFirst();
      const char **txt = g_toolbarButtonText;
      FXButton *b;
      FXToggleButton *tb;
      FXString label;
      FXString fullstr;
      while(win) {
        b = dynamic_cast<FXButton*>(win);
        tb = dynamic_cast<FXToggleButton*>(win);
        if (b) {
          if (btShowToolbarText) {
            fullstr = *txt++;
            label = fullstr.section('\t',0);
            b->setText(label);
            //b->setButtonStyle(ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
          } else {
            b->setText(0);
            //b->setButtonStyle(TEXT_OVER_ICON|BUTTON_TOOLBAR|FRAME_RAISED);
          }
        }
        else if (tb) {
          if (btShowToolbarText) {
            fullstr = *txt++;
            label = fullstr.section('\t',0);
            tb->setText( label );
            //label = fullstr.section('\t',1);
            tb->setAltText( label );
            //tb->setToggleStyle(TOGGLEBUTTON_KEEPSTATE|ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
          } else {
            tb->setText( FXString() );
            tb->setAltText( FXString() );
            //tb->setToggleStyle(TOGGLEBUTTON_KEEPSTATE|ICON_ABOVE_TEXT|BUTTON_TOOLBAR|FRAME_RAISED);
          }
        }
        win = win->getNext();
      }
      toolbarframe->recalc();
    }
    break;
  case ID_SHOW_STATUSBAR:
    if (statusbar->shown()) {
      statusbar->hide();
    }
    else {
      statusbar->show();
    }
    recalc();
    break;
  case ID_CHECK_BKGND:
    regenerateDisplayImage();
    break;
  case ID_DIFF_IMAGE:
    if (btDiffImage.val) {
      FXString text;
      text.format("DIFF (%d)", diffImage+1);
      statusDiffLabel->setText(text);
      statusDiffLabel->show();
    } else {
      statusDiffLabel->hide();
    }
    statusbar->recalc();
    regenerateDisplayImage();
    setNormalImageInfoStatus();
    break;
  }
  return 1;
}

long ImageWindow::onUpdateToggles(FXObject* sender,FXSelector sel,void* ptr)
{
  return 1;
  FXSelector msg=FXSEL(SEL_COMMAND,FXWindow::ID_UNCHECK);
  switch(FXSELID(sel)) {
  case ID_BLOCK_UPDATES:
    break;
  case ID_SHARE_WINDOW:
    break;
  case ID_LINK_PROPS:
    break;
  case ID_FLIP:
    break;
  case ID_STRETCH_FIT:
    break;
  case ID_SHOW_GRID:
    break;
  case ID_SHOW_SCALE_BIAS:
    break;
  case ID_CHECK_BKGND:
    break;
  }
  sender->handle(this,msg,NULL);
  sender->handle(this,FXSEL(SEL_COMMAND,FXWindow::ID_ENABLE),NULL);
  regenerateDisplayImage();
  return 1;
}

// Enable disable menu items
long ImageWindow::onUpdateMenu(FXObject*sender,FXSelector sel,void*)
{
  int enable = -1;
  switch(FXSELID(sel))
  {
    case ID_SAVE_AS:
    case ID_RESCALE:
    case ID_ZOOM_IN:
    case ID_ZOOM_OUT:
    case ID_ZOOM_100: case ID_ZOOM_200: case ID_ZOOM_300: case ID_ZOOM_400: case ID_ZOOM_500:
    case ID_ZOOM_600: case ID_ZOOM_700: case ID_ZOOM_800: case ID_ZOOM_900: case ID_ZOOM_1000:
    case ID_DELETE:
      enable = numImages>0;
      break;
    case ID_NEXT_IMAGE:
    case ID_LAST_IMAGE:
    case ID_NEXT_OF_TITLE:
      enable = (numImages>0 && curImage!=lastImage);
      break;
    case ID_PREV_IMAGE:
    case ID_FIRST_IMAGE:
    case ID_PREV_OF_TITLE:
      enable = (numImages>0 && curImage!=firstImage);
      break;
    case ID_COPY:
    case ID_CUT:
    case ID_PASTE:
      enable = 0;
      break;
  }
  if (enable==0) {
    sender->handle(this,FXSEL(SEL_COMMAND,FXWindow::ID_DISABLE),NULL);
  }
  else if (enable==1) {
    sender->handle(this,FXSEL(SEL_COMMAND,FXWindow::ID_ENABLE),NULL);
  }
  return 1;
}
long ImageWindow::onCmdRescale(FXObject* sender,FXSelector sel,void* ptr)
{
  drawAttr.calcZoom = true;
  regenerateDisplayImage();
  return 1;
}

void ImageWindow::setImageTranslationProp(float tx, float ty)
{
  if (!numImages) return;
  if (btLinkImageProps) {
    const char *tag = improps[curImage].title;
    int idx = findNextImageIndexByTag(tag,firstImage-1);
    while (idx>=0) {
      improps[idx].tx = tx;
      improps[idx].ty = ty;
      idx = findNextImageIndexByTag(tag,idx);
    }
  }
  else {
    improps[curImage].tx = tx;
    improps[curImage].ty = ty;
  }
}
void ImageWindow::setImageZoomProp(float zoom)
{
  if (!numImages) return;
  if (btLinkImageProps) {
    const char *tag = improps[curImage].title;
    int idx = findNextImageIndexByTag(tag,firstImage-1);
    while (idx>=0) {
      improps[idx].zoom = zoom;
      idx = findNextImageIndexByTag(tag,idx);
    }
  }
  else {
    improps[curImage].zoom = zoom;
  }
}
void ImageWindow::setImageScaleBiasProp(float scale, float bias)
{
  if (!numImages) return;
  if (btLinkImageProps) {
    const char *tag = improps[curImage].title;
    int idx = findNextImageIndexByTag(tag,firstImage-1);
    while (idx>=0) {
      for (int i=0;i<3;i++) {
        improps[idx].scale[i] = scale;
        improps[idx].bias[i]  = bias;
      }
      idx = findNextImageIndexByTag(tag,idx);
    }
  }
  else {
    for (int i=0;i<3;i++) {
      improps[curImage].scale[i] = scale;
      improps[curImage].bias[i] = bias;
    }
  }
}

void ImageWindow::doZoom(int dirn, int xWin, int yWin)
{
  DrawAttribs *pAttrs = &drawAttr;
  ImPropsEx   *pProps = &improps[curImage];
  float factor = 1.2f;
  float z = pProps->zoom;

  if (dirn == ID_ZOOM_100)
  {
    float zz = 1.0f;
    z = zz/z;
    pAttrs->stretched = FALSE;
  }
  else if (dirn>ID_ZOOM_100 && dirn<=ID_ZOOM_1000)
  {
    float zz = 1.0f + float(dirn-ID_ZOOM_100);
    z = zz/z;
    pAttrs->stretched = TRUE;
  }
  else if (dirn > 0) {
    if (z > (1.f/(factor+0.01f)) && z < 1.0f)  {
      z = 1.0f/(float)pProps->zoom;
      pAttrs->stretched = FALSE;
    }
    else {
      z = factor;
      pAttrs->stretched = TRUE;
    }
  }
  else {
    if (z < (factor+0.01) && z > 1.0f) {
      z = 1.0f/(float)pProps->zoom;
      pAttrs->stretched = FALSE;
    }
    else {
      z = 1.0f/factor;
      pAttrs->stretched = TRUE;
    }
  }
  setImageTranslationProp((int)(xWin + (pProps->tx-xWin)*z),
                          (int)(yWin + (pProps->ty-yWin)*z));
  setImageZoomProp( pProps->zoom * z );
}

long ImageWindow::onCmdZoom(FXObject* sender,FXSelector sel,void* ptr)
{
  switch(FXSELID(sel)){
  case ID_ZOOM_IN: 
    doZoom(1,imageview->getWidth()>>1,imageview->getHeight()>>1);
    break;
  case ID_ZOOM_OUT: 
    doZoom(-1,imageview->getWidth()>>1,imageview->getHeight()>>1);
    break;
  case ID_ZOOM_100: case ID_ZOOM_200: case ID_ZOOM_300: case ID_ZOOM_400: case ID_ZOOM_500:
  case ID_ZOOM_600: case ID_ZOOM_700: case ID_ZOOM_800: case ID_ZOOM_900: case ID_ZOOM_1000:
    doZoom(FXSELID(sel), imageview->getWidth()>>1,imageview->getHeight()>>1);
    break;
  }
  regenerateDisplayImage();
  setImageCountStatus();

  return 1;
}

void ImageWindow::clearScaleBiasHelpText()
{
  scaledial->setHelpText( "" );
  biasdial->setHelpText( "" );
  finebiasdial->setHelpText( "" );
  setNormalImageInfoStatus();
}
void ImageWindow::setScaleBiasHelpText()
{
  FXString dialHelp;
  dialHelp.format("Color scale: %f, bias %f", 
    improps[curImage].scale[0],
    improps[curImage].bias[0]
    );
  scaledial->setHelpText(dialHelp);
  biasdial->setHelpText(dialHelp);
  finebiasdial->setHelpText(dialHelp);
  statusbar->repaint();
  setNormalDialDraggingStatus();
}
void ImageWindow::setNormalDialDraggingStatus()
{
  FXString dialHelp;
  dialHelp.format("Color scale: %f, bias %f", 
    improps[curImage].scale[0],
    improps[curImage].bias[0]
    );
  statusbar->getStatusLine()->setNormalText( dialHelp );
  statusbar->repaint();
}

void ImageWindow::syncScaleBiasDials( bool doScale, bool doBias )
{
  if (!numImages) return;
  // turn the curImage scale & bias into slider scale & bias
  if (doBias) finebiasdial->setValue(0);
  float s = improps[curImage].scale[0];
  float b = improps[curImage].bias[0];
  int scaleinc = scaledial->getRevolutionIncrement()>>2;
  int biasinc = biasdial->getRevolutionIncrement()>>2;
  if (s==IMDBG_AUTOSCALE) {
    if (doScale) scaledial->setValue( 0 );
    if (doBias)  biasdial->setValue( 0 );
  } else {
    if (doScale)
      scaledial->setValue( (log10f(s)/log10f(2.0f)) * scaleinc );
    if (doBias) {
      double tmax = getTypemax(&improps[curImage]);
      int v = b*float(biasinc)*2.0f/(s*tmax);
      biasdial->setValue( v );
    }
  }
  clearScaleBiasHelpText();
}
long ImageWindow::onScaleBias(FXObject*sender,FXSelector sel,void*)
{
  if (!numImages) return 0;
  int v;
  FXString vs;
  switch (FXSELID(sel)) {
  case ID_IMAGE_SCALE:
    {
      float *srgba = &improps[curImage].scale[0];
      v = scaledial->getValue();
      // one revolution should double/half the value
      float s = powf(2.0f, v/float(scaledial->getRevolutionIncrement()>>2));
      setImageScaleBiasProp(s,improps[curImage].bias[0]);
      syncScaleBiasDials( false ); // just sync bias dials
    }
    break;
  case ID_IMAGE_BIAS:
  case ID_IMAGE_BIAS_FINE:
    {
      float *b = &improps[curImage].bias[0];
      float  s = improps[curImage].scale[0];
      double tmax = getTypemax(&improps[curImage]);
      v = biasdial->getValue();
      int vf = finebiasdial->getValue();
      float scaleinc = float(scaledial->getRevolutionIncrement()>>2);
      float biasinc  = float(biasdial->getRevolutionIncrement()>>2);
      float biasincf = float(finebiasdial->getRevolutionIncrement()>>2);
      // one-half revolution should go 1/2 the current mag scale 
      // so you're never more than two spins from zero
      // one-half rev of fine dial goes 1/200 the current mag scale.
      b[0] = ((0.5f/biasinc)*v + (0.01f/biasincf)*vf ) * s * tmax;
      setImageScaleBiasProp(s,b[0]);
    }
    break;
  }
  regenerateDisplayImage();
  setScaleBiasHelpText();
  ((FXWindow*)sender)->repaint();
  imageview->repaint();
  return 1;
}
long ImageWindow::onScaleBiasRelease(FXObject*sender,FXSelector sel,void*)
{
  clearScaleBiasHelpText();
  return 1;
}
long ImageWindow::onCmdScaleBias(FXObject*sender,FXSelector sel,void*)
{
  if (!numImages) return 0;
  switch (FXSELID(sel)) {
  case ID_AUTOSCALE:
    {
      FXint val;
      sender->handle(this,FXSEL(SEL_COMMAND,ID_GETINTVALUE),&val);
      if (val) {
        setImageScaleBiasProp(IMDBG_AUTOSCALE, improps[curImage].bias[0]);
      }
      else {
        //float *s = &improps[curImage].scale[0];
        //s[0] = s[1] = s[2] = 1.0f;
      }
    }
    break;
  case ID_UNITYSCALE:
    {
      setImageScaleBiasProp(1.0f, 0.0f);
    }
    break;
  }
  regenerateDisplayImage();
  syncScaleBiasDials();
  return 1;
}

// set the current image to the diff base
long ImageWindow::onCmdSetDiffBase (FXObject*sender,FXSelector sel,void*)
{
    diffImage = curImage;
    FXString text;
    text.format("DIFF (%d)", diffImage+1);
    statusDiffLabel->setText(text);
    regenerateDisplayImage();
    setNormalImageInfoStatus();
    return 1;
}

long ImageWindow::onCmdChannels(FXObject* sender,FXSelector sel,void* ptr)
{
  //switch(FXSELID(sel)){
  switch(itEnabledChannels){
  case ID_RGBA:
    drawAttr.enabledChannels = DrawAttribs::ENABLE_RGBA;
    statusbar->getStatusLine()->setText("Showing RGBA channels.");
    break;
  case ID_RGB:
    drawAttr.enabledChannels = DrawAttribs::ENABLE_RGB;
    statusbar->getStatusLine()->setText("Showing RGB channels.");
    break;
  case ID_RED:
    drawAttr.enabledChannels = DrawAttribs::ENABLE_RED;
    statusbar->getStatusLine()->setText("Showing Red Channel.");
    break;
  case ID_GREEN:
    drawAttr.enabledChannels = DrawAttribs::ENABLE_GREEN;
    statusbar->getStatusLine()->setText("Showing Green channel.");
    break;
  case ID_BLUE:
    drawAttr.enabledChannels = DrawAttribs::ENABLE_BLUE;
    statusbar->getStatusLine()->setText("Showing Blue channel.");
    break;
  case ID_ALPHA:
    drawAttr.enabledChannels = DrawAttribs::ENABLE_ALPHA;
    statusbar->getStatusLine()->setText("Showing Alpha channel.");
    break;
  }
  regenerateDisplayImage();
    statusbar->getStatusLine()->setText("Showing Alpha channel.");
  return 1;
}

void ImageWindow::clearImageToBackground(FXImage *img)
{
  int w = img->getWidth();
  int h = img->getHeight();
  FXColor *dst = img->getData();
  for (int y = 0; y < h; y++)
  {
    for (int x = 0; x < w; x++)
    {
      int v = ( !btCheckerBackground || (((y>>4)&1)^((x>>4)&1)) ) ? 127 : 255;

      // fill with GRAY or WHITE/GRAY checker 
      FXColor color = FXRGBA(v,v,v,255);
      *dst++ = color;
      //img->setPixel(x,y,color);
    }
  }

}
void ImageWindow::drawPixelGridOnImage(FXImage *img)
{
  FXColor *pData = img->getData();
  int WinW = img->getWidth();
  int WinH = img->getHeight(); 
  ImPropsEx* pProps = &improps[curImage];
  DrawAttribs*pAttrs = &drawAttr;
  int i,j;
  int StartX,StartY,EndX,EndY;
  int CurX,CurY;
  FXColor* dst;
  mapImageToWindow(0,0,&StartX,&StartY,pAttrs, pProps);
  mapImageToWindow(pProps->width,pProps->height,&EndX,&EndY,pAttrs, pProps);
  if (StartX<0)     StartX=0;  if (StartX>=WinW) StartX=WinW-1;
  if (StartY<0)     StartY=0;  if (StartY>=WinH) StartY=WinH-1;
  if (EndX<0)       EndX=0;    if (EndX>=WinW) EndX=WinW-1;
  if (EndY<0)       EndY=0;    if (EndY>=WinH) EndY=WinH-1;

  // horizontal lines
  const FXColor black = FXRGB(0,0,0);
  for (j=0; j<=pProps->height; j++) 
  {
    mapImageToWindow(0,j,&CurX,&CurY,pAttrs, pProps);
    if (CurY<0 || CurY >= WinH) continue;
    dst = pData + WinW * CurY + StartX;
    for(i=StartX; i<=EndX; i++)
    {
      // Black line
      *dst++ = black;
    }
  }

  // vertical lines
  for (j=0; j<=pProps->width; j++)
  {
    mapImageToWindow(j,0,&CurX,&CurY,pAttrs, pProps);
    if (CurX<0 || CurX>=WinW) continue;
    dst = pData + WinW * StartY + CurX;
    for(i=StartY; i<=EndY; i++)
    {
      // Black line
      *dst = black;
      dst+=WinW;
    }
  }


}

void ImageWindow::regenerateDiffImage(Byte* &pImage)
{
  ImPropsEx *pProps = &improps[curImage];
  ImPropsEx *pPropsBase = &improps[diffImage];

  bDiffValid =
    ((pPropsBase->width == pProps->width) &&
     (pPropsBase->height == pProps->height) &&
     (pPropsBase->rowstride == pProps->rowstride) &&
     (pPropsBase->colstride == pProps->colstride) &&
     (pPropsBase->colgap == pProps->colgap) &&
     (pPropsBase->rowgap == pProps->rowgap) &&
     (pPropsBase->nchan == pProps->nchan) );

  bDiffValid = bDiffValid && (pProps->nchan > 0);

  for (int c = 0; c < pPropsBase->nchan; ++c) {
    // channel bpc must match, and all channels must be 
    // 8, 16, 32, or 64 bits (i.e. native C types)
    bDiffValid = bDiffValid && 
      (pPropsBase->bpc[c] == pProps->bpc[c]) &&
      (pPropsBase->bpc[c] > 4 && pPropsBase->bpc[c] <= 64 &&
      ((pPropsBase->bpc[c] & (pPropsBase->bpc[c]-1))==0) );

    bDiffValid = bDiffValid && (pPropsBase->bitoffset[c] == pProps->bitoffset[c]);
    bDiffValid = bDiffValid && (pPropsBase->type[c] == pProps->type[c]);
    bDiffValid = bDiffValid && (pPropsBase->chanmap[c] == pProps->chanmap[c]);
  }

  if (bDiffValid) {
    // If all of the above match, then we can do a valid diff, 
    // so we need to create the diff image
    if (pDiffData)
      delete [] pDiffData;
    /*Byte **/pDiffData = new Byte[pProps->rawImageSize];

    Byte  *pbRawImage = pRawData[curImage];

    Byte *pbD = pDiffData;
    Byte *pbB = pRawData[diffImage];
    Byte *pbS = pbRawImage;

    int *Bpc = new int[pPropsBase->nchan];
    for (int c = 0; c < pPropsBase->nchan; ++c)
      Bpc[c] = pPropsBase->bpc[c] / 8;

    // do the diff
    for (int iPixel = 0; iPixel < pProps->width * pProps->height; ++iPixel) {
      for (int c = 0; c < pProps->nchan; ++c) {

        Bpc[c] = pPropsBase->bpc[c] / 8;

        // do the diff
        switch (pProps->type[c]) {
        case IMDBG_FLOAT:
          switch (pPropsBase->bpc[c]) {
            case 32: *((float*)pbD) = *((float*)pbS) - *((float*)pbB); break;
            case 64: *((double*)pbD) = *((double*)pbS) - *((double*)pbB); break;
            default: break;
          }
          break;
        case IMDBG_INT:
          switch (pPropsBase->bpc[c]) {
            case 8:  *((char*)pbD) = *((char*)pbS) - *((char*)pbB); break;
            case 16: *((short*)pbD) = *((short*)pbS) - *((short*)pbB); break;
            case 32: *((int*)pbD) = *((int*)pbS) - *((int*)pbB); break;
            case 64: *((long*)pbD) = *((long*)pbS) - *((long*)pbB); break;
            default: break;
          }
          break;
        case IMDBG_UINT:
          switch (pPropsBase->bpc[c]) {
            case 8:  *((unsigned char*)pbD) = abs(*((char*)pbS) - *((char*)pbB)); break;
            case 16: *((unsigned short*)pbD) = abs(*((short*)pbS) - *((short*)pbB)); break;
            case 32: *((unsigned int*)pbD) = abs(*((int*)pbS) - *((int*)pbB)); break;
            case 64: *((unsigned long*)pbD) = abs(*((long*)pbS) - *((long*)pbB)); break;
            default: break;
          }
        }
        pbD += Bpc[c];
        pbB += Bpc[c];
        pbS += Bpc[c];
      }
    }

    statusDiffLabel->setBackColor(FXRGB(0, 128, 0));
    pImage = pDiffData;
  }
  else
    statusDiffLabel->setBackColor(FXRGB(255, 0, 0));
}

bool ImageWindow::regenerateDisplayImage()
{
  // here's where we turn the current raw imdebug data into a displayable
  // image based on the current selected channel mappings, etc
  FXImage *img = imageview->getImage();
  if (!img) { 
    img = new FXImage(getApp(),NULL,IMAGE_OWNED|IMAGE_KEEP|IMAGE_SHMI|IMAGE_SHMP,
                      imageview->getWidth(),imageview->getHeight());
    img->create();
    imageview->setImage(img); 
  }
  int WinW = imageview->getWidth();
  int WinH = imageview->getHeight();
  if (WinW!=img->getWidth() || WinH != img->getHeight())
     img->resize(WinW, WinH);

  clearImageToBackground(img);
  
  Byte *pbDisplayImage = (Byte*)img->getData(); 
  Byte  *pbRawImage = pRawData[curImage];
  ImPropsEx *pProps = &improps[curImage];
  DrawAttribs *pAttrs = &drawAttr;

  bDiffValid = false;

  // Determine if we can diff, and if so, create diff image.
  if (btDiffImage) {
    regenerateDiffImage(pbRawImage);
  }

  if (numImages>0 && pProps->zoom < 1e-7) {
    if (btAutoStretch) pAttrs->calcZoom = true;
    else {
      pProps->zoom=1.0f;
      pProps->tx = (WinW-pProps->width)>>1;
      pProps->ty = (WinH-pProps->height)>>1;
    }
  }
  
  int NewW, NewH;
  int cxImgPos, cyImgPos;

  int cxImgSize =    pProps->width;
  int cyImgSize =    pProps->height;
  int cImgChannels = pProps->nchan;

  if (cImgChannels<=0 || numImages<=0) {
    img->render();
    imageview->update();
    return true;
  }

  bool uniformType = true;
  bool uniformWidth = true;
  bool byteAligned = true;
  bool byteAlignedCol = true;
  bool computeScaleAndBias = false;

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
  if (! (pAttrs->enabledChannels & pAttrs->ENABLE_RED))
    effectiveChanmap[0] = -1;
  if (! (pAttrs->enabledChannels & pAttrs->ENABLE_GREEN))
    effectiveChanmap[1] = -1;
  if (! (pAttrs->enabledChannels & pAttrs->ENABLE_BLUE))
    effectiveChanmap[2] = -1;
  if (! (pAttrs->enabledChannels & pAttrs->ENABLE_ALPHA))
    effectiveChanmap[3] = -1;
  // Make alpha grayscale if that's all there is
  if (pAttrs->enabledChannels == pAttrs->ENABLE_ALPHA) {
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
      if (pProps->bpc[i] & 0x7) byteAligned = false;
      if (pProps->type[i] != type)
        uniformType = false;
      if (pProps->bpc[i] != pProps->bpc[0]) {
        uniformWidth = false;
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
    if (srcSampleBitWidth&7) byteAlignedCol = false;
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
        computeScaleAndBias = true;
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
    cxImgPos = pProps->tx;
    cyImgPos = pProps->ty;
    NewW = (int)(pProps->zoom * cxImgSize);
    NewH = (int)(pProps->zoom * cyImgSize);
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
      pProps->tx = cxImgPos;
      pProps->ty = cyImgPos;
      pProps->zoom = NewW / (float)cxImgSize;
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
      pProps->tx = cxImgPos;
      pProps->ty = cyImgPos;
      pProps->zoom = 1.0;
    }

  }
  pAttrs->calcZoom = false;


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
    bool restoreScaleAndBias = false;
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
        pbRawImage, pProps->width, srcSampleByteWidth,
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
      restoreScaleAndBias = true;
      for (i=0; i<4; i++) {
        saveScale[i] = pProps->scale[i];
        saveBias[i]  = pProps->bias[i];
      }
      */
      setScaleAndBias(&bs, typeMin, typeMax, pProps->scale, pProps->bias);
    }

    pBlitFunc(
      pbDisplayImage,  WinW, WinH, NewW, NewH, cxImgPos, cyImgPos,
      pbRawImage,
      pProps->width,
      srcSampleByteWidth,
      pProps->width, pProps->height, 0, 0,
      srcChanByteOffsets,
      pProps,
      0
      );

    /*if (btDiffImage && bDiffValid)
    {
        delete [] pbDif;
    }*/

    if (restoreScaleAndBias) {
      // this puts back the AUTOSCALE indicators the way they were
      for (i=0; i<4; i++) {
        pProps->scale[i] = saveScale[i];
        pProps->bias[i]  = saveBias[i];
      }
    }
  }

  // draw grid if needed
  if (pBlitFunc && btShowPixelGrid && pProps->zoom >= 4) {
    drawPixelGridOnImage(img);
  }

  // flip if needed
  if (pBlitFunc && btFlipImage) {
    img->mirror(false,true);
  }

  img->render();
  imageview->setImage(img);
  imageview->update();
  return (pBlitFunc != 0);
}

// Update image
long ImageWindow::onUpdImage(FXObject* sender,FXSelector,void*){
  if(imageview->getImage())
    sender->handle(this,FXSEL(SEL_COMMAND,FXWindow::ID_ENABLE),NULL);
  else
    sender->handle(this,FXSEL(SEL_COMMAND,FXWindow::ID_DISABLE),NULL);
  return 1;
}

int ImageWindow::computeRawDataSize(ImProps *pProp)
{
  int imsize;
  imsize = (int)ceil((pProp->rowstride * pProp->height)/8.0f);
  return imsize;
}

void ImageWindow::copyRawImageData(ImPropsEx *pProp, Byte **rawData)
{
  void *mapview = mappedFile->data();

  size_t oldRawSize = pProp->rawImageSize;

  memcpy(pProp, mapProps(mapview), sizeof(ImProps));

  size_t rawSize = computeRawDataSize(pProp);
  pProp->rawImageSize = rawSize;

  // Free old data
  if (*rawData != NULL && oldRawSize != rawSize) {
    delete [] *rawData;
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
  else if (*rawData==NULL) {
    *rawData = new Byte[rawSize];
    if (!*rawData) {
      FXMessageBox::error(this,MBOX_OK,"Imdebug Error","Error in allocating memory for image\n");
      pProp->nchan = ERROR_ALLOC_FAIL;
      return;
    }
  }
  if (*rawData!=NULL)
    memcpy(*rawData, mapImage(mapview), rawSize);
}
void ImageWindow::addNewImageSlotToCircularBuffer()
{
  int originalLastImage = lastImage;
  if (numImages != 0) {
    ++lastImage; lastImage %= historySize;
    if (firstImage==lastImage) {
      // circular buf is full we're throwing one away
      if (curImage == firstImage) {
        ++curImage; curImage %= historySize;
      }
      ++firstImage; firstImage %= historySize;
    }
    else {
      numImages++;
    }
  } else {
    lastImage = firstImage = 0;
    numImages++;
  }

  if (!btHoldOnImage || numImages==1 /*|| curImage==originalLastImage*/)
    curImage = lastImage;
}

int ImageWindow::findPreviousImageIndexByTag(const char *title, int start)
{
  // search for a matching tag  starting from "start"
  bool match = false;
  int idx = start;
  if (idx<-100) {
    idx = lastImage;
  }
  idx--; // we don't check start, but the one after it.
  if (idx < 0) idx = historySize-1;

  int firstMinusOne = firstImage-1;
  if (firstMinusOne<0) firstMinusOne = historySize-1;

  while( idx != firstMinusOne ) {
    if ( !strcmp(improps[idx].title, title) )
    {
      match = true;
      break;
    }
    idx--;
    if (idx < 0) idx = historySize-1;
  }
  if (match) return idx;
  else return -1;
}
int ImageWindow::findNextImageIndexByTag(const char *title, int start)
{
  // search for a matching tag  starting from "start"
  bool match = false;
  int idx = start;
  if (idx<-100) {
    idx = firstImage;
  }
  idx++; // we don't check start, but the one after it.
  idx %= historySize;
  int lastPlusOne = (lastImage+1)%historySize;
  
  while( idx != lastPlusOne) {
    if ( !strcmp(improps[idx].title, title) )
    {
      match = true;
      break;
    }
    idx++;
    idx %= historySize;
  }
  if (match) return idx;
  else return -1;
}

long ImageWindow::onNewImage(FXObject*,FXSelector , void* )
{
  if (btBlockUpdates) return 0;

  int newImage = -1;
  bool overwritten =false;
  if (btOverwriteImages) {
    int idx = findPreviousImageIndexByTag(  mapProps(mappedFile->data())->title, lastImage );
    if (idx>=0) {
      overwritten = true;
      newImage = idx;
      if (!btHoldOnImage) curImage = idx;
    }
  }
  if (!overwritten) {
    addNewImageSlotToCircularBuffer();
    newImage = lastImage;
  }
  
  copyRawImageData(&improps[newImage], &pRawData[newImage]);
  if (improps[newImage].VERSION != IMDBG_PROTOCOL_VERSION)
  {
    // THIS EXE DOESN'T MATCH THE DLL BEING USED!!! BAIL OUT!!!
    if ( improps[newImage].VERSION > IMDBG_PROTOCOL_VERSION ) {
      FXMessageBox::error(this, MBOX_OK, "Imdebug Error",
        "Error: Version mismatch.  "
        "The DLL you are linking with is"
        " more recent than this executable. "
        "Terminating."
        );
    } else {
      FXMessageBox::error(this, MBOX_OK, "Imdebug Error",
        "Error: Version mismatch.  "
        "This exe is more recent than"
        " the DLL you are linking with. "
        "Terminating."
        );
    }
    delete mappedFile;
    mappedFile = 0;
    getApp()->exit(0);
  }
  
  improps[newImage].globalImgNum = imagesDisplayed;
  if (overwritten) {
    improps[newImage].imgNum++;
  }
  else 
    improps[newImage].imgNum = 0;

  // possibly copy scale bias and zooms
  // from previous tagged image of same tag
  bool match = false;
  if (!overwritten && 
      (btPropagateScaleOnTagged || btLinkImageProps) &&
      improps[newImage].title[0] != 0 &&
      !(improps[newImage].flags & (IMDBG_FLAG_AUTOSCALE|IMDBG_FLAG_SCALE_SET|IMDBG_FLAG_BIAS_SET)) )
  {
    int idx = findPreviousImageIndexByTag(improps[lastImage].title);
    
    if (idx>=0) {
      match = true;
      // copy scale and bias settings of that image
      int ch;
      for (ch=0;ch<4;ch++) {
        improps[lastImage].scale[ch] = improps[idx].scale[ch];
        improps[lastImage].bias[ch]  = improps[idx].bias[ch];
      }
      improps[lastImage].zoom = improps[idx].zoom;
      improps[lastImage].tx   = improps[idx].tx;
      improps[lastImage].ty   = improps[idx].ty;
    }
  }
  
  drawAttr.calcZoom = (!match&&btAutoStretch) ? true : false;
  regenerateDisplayImage();
  syncScaleBiasDials();

  imagesDisplayed++;
  
  setImageCountStatus();
  setNormalImageInfoStatus();
  
#ifdef _WIN32
  if (win32ImageReceivedSignal)
    SetEvent(win32ImageReceivedSignal);
#endif
  return 1;
}
/*
long ImageWindow::onIdleTask(FXObject*o,FXSelector s,void* p)
{
  // check for new image in the mmap file if we are listening for updates
  if (!btBlockUpdates && mappedFile && mappedFile->data()) 
  {
    ImProps props;
    memcpy(&props, mapProps(mappedFile->data()), sizeof(ImProps));
    if (props.VERSION==IMDBG_PROTOCOL_VERSION) {
      onNewImage(o,s,p);
      //mapProps(mappedFile->data())->VERSION = 0;
    }
  }
  // Re-register the chore
  getApp()->addChore(this, ID_IDLE_TASK);
  return 1;
}*/

long ImageWindow::onExternalNotify(FXObject*o,FXSelector s,void*p)
{
  if (!btBlockUpdates && mappedFile && mappedFile->data())
  {
      onNewImage(o,s,p);
  }
  else {
#ifdef _WIN32
  if (win32ImageReceivedSignal)
    SetEvent(win32ImageReceivedSignal);
#endif
  }
  return 1;
}
/*
// Mirror
long ImageWindow::onCmdMirror(FXObject*,FXSelector sel,void*){
  FXImage* image=imageview->getImage();
  FXASSERT(image);
  switch(FXSELID(sel)){
    case ID_MIRROR_HOR: image->mirror(true,false); break;
    case ID_MIRROR_VER: image->mirror(false,true); break;
    }
  imageview->setImage(image);
  return 1;
}


// Scale
long ImageWindow::onCmdScale(FXObject*,FXSelector,void*){
  FXImage* image=imageview->getImage();
  FXASSERT(image);
  FXint sx=image->getWidth();
  FXint sy=image->getHeight();
  FXDataTarget sxtarget(sx);
  FXDataTarget sytarget(sy);
  FXDialogBox  scalepanel(this,"Scale Image To Size");
  FXHorizontalFrame* frame=new FXHorizontalFrame(&scalepanel,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y);
  new FXLabel(frame,"W:",NULL,LAYOUT_CENTER_Y);
  new FXTextField(frame,5,&sxtarget,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|FRAME_SUNKEN|FRAME_THICK|JUSTIFY_RIGHT);
  new FXLabel(frame,"H:",NULL,LAYOUT_CENTER_Y);
  new FXTextField(frame,5,&sytarget,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|FRAME_SUNKEN|FRAME_THICK|JUSTIFY_RIGHT);
  new FXButton(frame,"Cancel",NULL,&scalepanel,FXDialogBox::ID_CANCEL,LAYOUT_CENTER_Y|FRAME_RAISED|FRAME_THICK,0,0,0,0, 20,20,4,4);
  new FXButton(frame,"OK",NULL,&scalepanel,FXDialogBox::ID_ACCEPT,LAYOUT_CENTER_Y|FRAME_RAISED|FRAME_THICK,0,0,0,0, 30,30,4,4);
  if(!scalepanel.execute()) return 1;
  if(sx<1 || sy<1) return 1;
  image->scale(sx,sy);
  imageview->setImage(image);
  return 1;
}


// Crop
long ImageWindow::onCmdCrop(FXObject*,FXSelector,void*){
  FXImage* image=imageview->getImage();
  FXASSERT(image);
  FXint cx=0;
  FXint cy=0;
  FXint cw=image->getWidth();
  FXint ch=image->getHeight();
  FXDataTarget cxtarget(cx);
  FXDataTarget cytarget(cy);
  FXDataTarget cwtarget(cw);
  FXDataTarget chtarget(ch);
  FXDialogBox  croppanel(this,"Crop image");
  FXHorizontalFrame* frame=new FXHorizontalFrame(&croppanel,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y);
  new FXLabel(frame,"X:",NULL,LAYOUT_CENTER_Y);
  new FXTextField(frame,5,&cxtarget,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|FRAME_SUNKEN|FRAME_THICK|JUSTIFY_RIGHT);
  new FXLabel(frame,"Y:",NULL,LAYOUT_CENTER_Y);
  new FXTextField(frame,5,&cytarget,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|FRAME_SUNKEN|FRAME_THICK|JUSTIFY_RIGHT);
  new FXLabel(frame,"W:",NULL,LAYOUT_CENTER_Y);
  new FXTextField(frame,5,&cwtarget,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|FRAME_SUNKEN|FRAME_THICK|JUSTIFY_RIGHT);
  new FXLabel(frame,"H:",NULL,LAYOUT_CENTER_Y);
  new FXTextField(frame,5,&chtarget,FXDataTarget::ID_VALUE,LAYOUT_CENTER_Y|FRAME_SUNKEN|FRAME_THICK|JUSTIFY_RIGHT);
  new FXButton(frame,"Cancel",NULL,&croppanel,FXDialogBox::ID_CANCEL,LAYOUT_CENTER_Y|FRAME_RAISED|FRAME_THICK,0,0,0,0, 20,20,4,4);
  new FXButton(frame,"OK",NULL,&croppanel,FXDialogBox::ID_ACCEPT,LAYOUT_CENTER_Y|FRAME_RAISED|FRAME_THICK,0,0,0,0, 30,30,4,4);
  if(!croppanel.execute()) return 1;
  if(cx<0 || cy<0 || cx+cw>image->getWidth() || cy+ch>image->getHeight()) return 1;
  image->crop(cx,cy,cw,ch);
  imageview->setImage(image);
  return 1;
}
*/

long ImageWindow::onMouseDown(FXObject*,FXSelector,void*)
{
  imageview->grab();
  downButtons |= 1;
  return 1;
}

long ImageWindow::onMouseUp(FXObject*,FXSelector,void*)
{
  imageview->ungrab();
  downButtons &= ~1;
  return 1;
}

long ImageWindow::onMouseMove(FXObject*,FXSelector,void*ptr)
{
  if (numImages<=0) return 0;
  FXEvent *ev=(FXEvent*)ptr;

  if ( downButtons & 1) {
    // Update ruler arrow locations
    int dx = ev->win_x-ev->last_x;
    int dy = ev->win_y-ev->last_y;
    if (btFlipImage) dy *= -1;
    setImageTranslationProp(improps[curImage].tx+dx, improps[curImage].ty+dy);
    regenerateDisplayImage();
    imageview->repaint();
  }
  else {
    setHoverImageInfoStatus(ev->win_x,ev->win_y);
  }
  return 1;
}

long ImageWindow::onLeave(FXObject*,FXSelector,void*)
{
  imageview->setHelpText( FXString() );
  tooltip->setStyle( TOOLTIP_NORMAL );
  tooltip->setText( FXString() );
  tooltip->hide();
  return 1;
}

long ImageWindow::onMouseWheel(FXObject*,FXSelector,void*ptr)
{
  FXEvent* ev=(FXEvent*)ptr;
  FXint scroll = ev->code; //is amount rolled the wheel ... = numclicks * 120
  if (ev->state & CONTROLMASK) {
    // nav image
    if (scroll > 0) {
      onCmdNavigate(this,FXSEL(SEL_COMMAND,ID_NEXT_IMAGE),0);
    }
    else {
      onCmdNavigate(this,FXSEL(SEL_COMMAND,ID_PREV_IMAGE),0);
    }
    return 1;
  }
  else {
    // zoom image
    doZoom(scroll, ev->win_x, ev->win_y);
  }
  regenerateDisplayImage();
  imageview->repaint();
  setImageCountStatus();
  setHoverImageInfoStatus(ev->win_x,ev->win_y);
  setNormalImageInfoStatus();
  return 1;
}

long ImageWindow::onResize(FXObject*,FXSelector,void*)
{
  regenerateDisplayImage();
  return 1;
}


// Create and show window
void ImageWindow::create(){
  FXint ww,hh,xx,yy;
  FXString dir;

  // Get size
  FXRegistry &reg = getApp()->reg();
  xx=reg.readIntEntry("SETTINGS","x",600);
  yy=reg.readIntEntry("SETTINGS","y",400);
  ww=reg.readIntEntry("SETTINGS","width",600);
  hh=reg.readIntEntry("SETTINGS","height",400);

  historySize = reg.readIntEntry("SETTINGS","history",40);
  btPropagateScaleOnTagged = reg.readIntEntry("SETTINGS","copyscale",1);
  
  btCheckerBackground = reg.readIntEntry("SETTINGS","check",       1);
  btAutoStretch       = reg.readIntEntry("SETTINGS","stretch",     1);
  btShowPixelGrid     = reg.readIntEntry("SETTINGS","grid",        1);
  btFlipImage         = reg.readIntEntry("SETTINGS","flip",        1);
  btShareOutputWindow = reg.readIntEntry("SETTINGS","shareDisplay",1);
  btLinkImageProps    = reg.readIntEntry("SETTINGS","linkprops",   1);
  btOverwriteImages   = reg.readIntEntry("SETTINGS","overwrite",   0);
  btHoldOnImage       = reg.readIntEntry("SETTINGS","holdonimage", 0);
  btShowManipCtrls    = reg.readIntEntry("SETTINGS","sbControls",  1);
  FXbool bShowToolbar  = reg.readIntEntry("SETTINGS","toolbar",     1);
  btShowToolbarText    = reg.readIntEntry("SETTINGS","toolbartext", 1);
  FXbool bShowStatusbar= reg.readIntEntry("SETTINGS","statusbar",   1);
  FXString dockingSide = reg.readStringEntry("SETTINGS","tbdockside","top");
  //dir=getApp()->reg().readStringEntry("SETTINGS","directory","~");
  //filelist->setDirectory(dir);

  //filebox->setHeight(fh);

  // Hide tree if asked for
  //if(!fs) filebox->hide();

  // Allocate and clear out the image history buffers here
  pRawData = new Byte*[historySize*sizeof(Byte*)];
  improps  = new ImPropsEx[historySize*sizeof(ImProps)];
  memset(pRawData,0, historySize*sizeof(Byte*));
  memset(improps, 0, historySize*sizeof(ImProps));

  // Reposition window
  position(xx,yy,ww,hh);

  FXMainWindow::create();


  // Settle for polling for now in lieu of getting direct "new image" 
  // signalling events from the clients.
  //getApp()->addChore(this, ID_IDLE_TASK);
#ifdef _WIN32
  FXInputHandle fd = CreateImageReadySignalEvent(IMDBG_SIGNAL_NAME);
  if (fd) {
    getApp()->addInput( fd, INPUT_READ, this, 0/*ID_IDLE_TASK*/ );
  }
  win32ImageReceivedSignal = CreateImageReadySignalEvent(IMDBG_SIGNAL_RECEIVED_NAME);
#endif
  // see if there's an image there already
  if (mapProps(mappedFile->data())->VERSION==IMDBG_PROTOCOL_VERSION)
    onExternalNotify(this,ID_DISPLAY_AREA,NULL);

  drawAttr.flipped = btFlipImage ? true : false;

  if (!btShowManipCtrls) {
    scaleframe->hide();
    biasframe->hide();
  }
  if (!bShowToolbar) {
    toolbar->hide();
  }
  if      (dockingSide == "left") { toolbar->setDockingSide(LAYOUT_SIDE_LEFT); }
  else if (dockingSide == "right") { toolbar->setDockingSide(LAYOUT_SIDE_RIGHT); }
  else if (dockingSide == "bottom") { toolbar->setDockingSide(LAYOUT_SIDE_BOTTOM); }
  if (!btShowToolbarText) {
    this->handle(this,FXSEL(SEL_COMMAND,ID_SHOW_TOOLBAR_TEXT),0);
  }
  if (!bShowStatusbar) {
    statusbar->hide();
  }
  show();
}


/***************************************************************************/
/*
class CustomFXApp : public FXApp
{
public:
  CustomFXApp(const FXString& name=IMDBG_REG_KEY_APP,
              const FXString& vendor=IMDBG_REG_KEY_VENDOR)
              : FXApp(name,vendor)
  {}
#ifdef _WIN32
  // For win32, watch for the WM_APP message coming to tell us that a 
  // new image is ready.  Unfortunately this doesn't seem to work unless
  // the app sends the WM_APP message with PostMessage instead of SendMessage.
  // But we need the synchronous behavior of SendMessage for it to work
  // right.
  FXbool getNextEvent(FXRawEvent& msg,FXbool blocking){
    FXbool ret = FXApp::getNextEvent(msg,blocking);
    fxmessage("msg: 0x%x\t0x%x\t0x%x\n",msg.message, msg.lParam, msg.wParam);
    if (imageReadyTarget && msg.message >= WM_APP && msg.message <= 0xBFFF) {
       imageReadyTarget->handle(0,FXSEL(SEL_IO_READ,0),0);
    }
    return ret;
  }
#endif
  void setImageReadyTarget(FXObject *obj) {
    imageReadyTarget = obj;
  }
private:
  FXObject *imageReadyTarget;

};
void MakeConsole() {
  AllocConsole( );  // Create Console Window
  freopen("CONIN$","rb",stdin);   // reopen stdin handle as console window input
  freopen("CONOUT$","wb",stdout);  // reopen stout handle as console window output
  freopen("CONOUT$","wb",stderr); // reopen stderr handle as console window output
}
*/


/***************************************************************************/
// Start the whole thing
int main(int argc,char *argv[]){

  // Make application
  FXApp application(IMDBG_REG_KEY_APP, IMDBG_REG_KEY_VENDOR);

  // Open display
  application.init(argc,argv);

  // Make window
  ImageWindow* window=new ImageWindow(&application);
  
  // Handle interrupt to save stuff nicely
  application.addSignal(SIGINT,window,ImageWindow::ID_QUIT);

  // Create it
  application.create();

  // Run It!
  return application.run();
}


