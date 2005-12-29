/*
 *============================================================================
 * blitfuncs.h
 *   This file provides the prototypes for all the blitter funcs, and a few
 *   other blitting related structures and functions.
 *============================================================================
 *
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
 *==========================================================================*/

#ifndef BLIT_FUNCS_H
#define BLIT_FUNCS_H
#include "imdebug_priv.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
// THIS IS THE TYPE SIGNATURE OF ALL THE BLITTER FUNCS
typedef void (*PBLITFUNC)(
  BYTE* pDestinationPixels, 
  int dstDataW, // W in pixels of bitmap we're copying into
  int dstW, int dstH, int dstX, int dstY, // rect of region to copy to
  BYTE* pSrc,
  int srcDataW, // W in pixels of src data we're copying from
  int srcDataColBytes, // W in bytes of single pixel of data we're copying from
  int srcW, int srcH, int srcX, int srcY,  // subrect of region to copy from
  char *srcChanOffsets,  // precomputed byte offsets for each channel
  ImProps *pProps, // contains all raw data about image
  BlitStats *bs // NULL or a pointer to a BlitStats structure to fill in.
                // when bs!=NULL, pDiData will not be modified.
  );
*/

#define BLITTER_DEST_CHANNELS 4
// define BLITTER_DEST_BGR to use BGR ordering on dest rather than RGB
// #define BLITTER_DEST_BGR


typedef struct {
  float min[4];//min of r,g,b,a
  float max[4];//max of r,g,b,a
} BlitStats;

void initBlitStats(BlitStats *bs);

// Utility routines (used by imdbgdisplay also)
unsigned int bytePtr2Val_BitOffsets(BYTE*x, int offset, int width);

#define DECLARE_BLITTER(func,attribs) \
void func (BYTE*,int,int,int,int,int,int,BYTE*,int,int,int,int,int,int,char*,ImProps*,BlitStats*);

typedef DECLARE_BLITTER((*PBLITFUNC),_dummy)

// HERE'S WHERE THE MEAT IS
#include "blitfuncdecl.h"

#undef DECLARE_BLITTER

#ifdef __cplusplus
} // end extern "C"
#endif

#endif
