/*============================================================================
 * imdebug_priv.h
 *   Internal header.  Defines common structs and values used by both
 *   imdebug.dll and imdbgdisplay.exe.
 *============================================================================
 * Copyright (c) 2002-2005 William V. Baxter III
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software. If you use
 *    this software in a product, an acknowledgment in the product
 *    documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *============================================================================
 */
#ifndef IMDEBUG_PRIV_H
#define IMDEBUG_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif 
#include <float.h>

#define IMDBG_MAX_CHANNELS 16

#define APPM_IMAGE_READY ((WPARAM)0x101)

#define IMDBG_UINT   1
#define IMDBG_INT    2
#define IMDBG_FLOAT  3

#define IMDBG_AUTOSCALE FLT_MIN

#define MAX_TITLE_LENGTH 80

#define IMDBG_PROTOCOL_VERSION 4

#define IMDBG_APP_MAJOR_VERSION 1
#define IMDBG_APP_MINOR_VERSION 1
#define IMDBG_APP_PATCH_LEVEL   2
#define IMDBG_APP_IS_BETA false
#define IMDBG_APP_VERSION_STRING "v1.12"
#define IMDBG_APP_DATE "December 2005"

#define IMDBG_DISPLAY_WINDOW_TITLE "Image Debugger Display"
#define IMDBG_DISPLAY_WINDOW_CLASS "ImDbgDisplay"
#define IMDBG_REG_KEY_VENDOR "Imdebug"
#define IMDBG_REG_KEY_APP "Imdebug"
#define IMDBG_MEMMAP_FILENAME "ImgDebugPipe"
#define IMDBG_SIGNAL_NAME     "ImgDebugImageReadyEvent"
#define IMDBG_SIGNAL_RECEIVED_NAME     "ImgDebugImageReceivedEvent"

enum { 
  IMDBG_FLAG_AUTOSCALE =0x1,
  IMDBG_FLAG_SCALE_SET =0x2,
  IMDBG_FLAG_BIAS_SET  =0x4
};

typedef struct
{
  // VERSION 2 MEMBERS:
  int VERSION; // set to IMDBG_PROTOCOL_VERSION
  int width;
  int height;
  int nchan; // number of channels
  int bpc[IMDBG_MAX_CHANNELS];   // bits for each channel
  int bitoffset[IMDBG_MAX_CHANNELS]; // bit offsets for each channel
  char type[IMDBG_MAX_CHANNELS]; // type for each channel
  char chanmap[4];   // where each output chan comes from (<0 for off)
  int colstride; // bits between start of each sample within a row
  int rowstride; // bits between start of each row
  int colgap; // extra bits to skip between samples in a row
  int rowgap; // extra bits to skip between rows
  float scale[4]; // scale factor to apply to channel values
  float bias[4];  // bias to apply to channel values

  // VERSION 3 ADDITIONS:
  char title[MAX_TITLE_LENGTH+1]; // label for title bar

  // VERSION 4 ADDITIONS
  unsigned int flags;
} ImProps;

// big enough for a 2048*2048 RGBA 32f image + header
#define IMDBG_MAX_DATA (16 * 2048 * 2048 + sizeof(ImProps))

#ifdef __cplusplus
} // end extern "C"
#endif 

#endif
