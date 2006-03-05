/*
 *============================================================================
 * blitfuncs.c
 *   This file implements all the blitter functions by including
 *   the blit function template in blittemplate.h many times with slighly
 *   different setup before each inclusion.
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
typedef unsigned char BYTE;
typedef unsigned int WORD;

#include "blitfuncs.h"
#include "imdebug_priv.h"
#include <float.h>

void initBlitStats(BlitStats *bs)
{
  int i;
  for (i=0;i<4;i++) {
    bs->min[i] = FLT_MAX;
    bs->max[i] =  -FLT_MAX;
  }
}

static float Clamp01f(float v)
{
  if (v<0.f) return 0.f;
  if (v>1.f) return 1.f;
  return v;
}

static BYTE cvtFloat2Byte(float v)
{
  int iv;
  if (v<0.f) v=0.f;
  if (v>1.f) v=1.f;
  iv = (int)(v * 255.0f + 0.5f);
  return (BYTE)iv;
}

static BYTE cvtDouble2Byte(double v)
{
  int iv;
  if (v<0.0) v=0.0;
  if (v>1.0) v=1.0;
  iv = (int)(v * 255.0 + 0.5);
  return (BYTE)iv;
}

static BYTE cvtByteEx2Byte(int v)
{
  if (v>255) v=255;
  if (v<0) v=0;
  return (BYTE)v;
}

static BYTE cvtUShort2Byte(unsigned short v)
{
  // basically pretend this is 8+8 fixed point
  // so add .5 and truncate to 8 bits
  // -- actually adding very close to the top of the range
  //    causes overflow .. not good
  return (BYTE)((v/*+128*/)>>8);
}
#define cvtUShort2ByteM(v) ((BYTE)((v/*+128*/)>>8))

static BYTE cvtUInt2Byte(unsigned int v)
{
  // basically pretend this is 8+24 fixed point
  // so add .5 and truncate to 8 bits
  // -- actually adding very close to the top of the range
  //    causes overflow .. not good
  return (BYTE)((v/*+(1<<23)*/)>>24);
}
#define cvtUInt2ByteM(v) ((BYTE)((v/*+(1<<23)*/)>>24))

static BYTE cvtNbits2Byte(unsigned int v, int N)
{
  if (N > 8) {
    // pretend this is 8+(N-8) fixed point
    // so add .5 and truncate to 8 bits
    N-=8;
    return (BYTE)((v/*+(1<<(N-1))*/)>>N);
  }
  else if (N < 8) {
    // pad out to 8 bits
    N = 8-N;
    return (BYTE)(v<<N);
  }

  return (BYTE)v;
}

static BYTE cvtNLongbits2Byte(unsigned long v, int N)
{
  if (N > 8) {
    // pretend this is 8+(N-8) fixed point
    // so add .5 and truncate to 8 bits
    N-=8;
    return (BYTE)((v/*+(1<<(N-1))*/)>>N);
  }
  else if (N < 8) {
    // pad out to 8 bits
    N = 8-N;
    return (BYTE)(v<<N);
  }
  return (BYTE)v;
}


unsigned int bytePtr2Val_BitOffsets(BYTE*x, int offset, int width)
{
  // example: packed 5,6,5 format color.
  // 3rd component is offset 10, width 5.
  // 0       8       16      24
  // |   .   |   .   |   .   |   .
  // ||||||||||||||||||||||||||||||||
  // rrrrrggggggbbbbb
  // ^       ^
  // x      x+1
  //
  // hmmm... something like this anyway:
  int bytes=offset>>3;
  int bits=offset&0x7;
  unsigned int ret;
  x+=bytes;
  ret = ((*((unsigned int*)x))>>bits) & ~((~0)<<width);
  return ret;
  // anyway, this extracts values created like this:
  //                 val =  (b<<11)  |  (g<<5)  |  (r)
  //    with      offset =    11           5        0
  //               width =     5           6        5
  // and that's the same thing you get with this bit field struct:
  // struct {
  //     unsigned r:5;
  //     unsigned g:5;
  //     unsigned b:5;
  // };
  // ugh, little endian always makes my head hurt.
}


#if 0
/*
void stretchBlitCoreRGBA32f(
  int NewW, int NewH, int cOutChannels, BYTE *pImg,
  int cxImgSize, int cyImgSize, int cImgChannels, BYTE *pbImage,
  char *channelOffsets
  )
{
  // this has endian issues... there's an assumption of native
  // endian formatting.  Whether or not this is so should really
  // be specifiable from the calling interface.

  int yNew, xNew, yOld, xOld;
  BYTE r, g, b, a;
  float *pfImage = (float*)pbImage;

  // channel offsets
  int ro = channelOffsets[0];
  int go = channelOffsets[1];
  int bo = channelOffsets[2];
  int ao = channelOffsets[3];

  for (yNew = 0; yNew < NewH; yNew++)
  {
    yOld = yNew * cyImgSize / NewH;
    for (xNew = 0; xNew < NewW; xNew++)
    {
      float *base;
      xOld = xNew * cxImgSize / NewW;
      base = pfImage + cImgChannels * ((yOld*cxImgSize) + xOld);

      r = (ro>=0) ? cvtFloat2Byte(*(base + ro)) : 0;
      g = (go>=0) ? cvtFloat2Byte(*(base + go)) : 0;
      b = (bo>=0) ? cvtFloat2Byte(*(base + bo)) : 0;
      *pImg++ = r;
      *pImg++ = g;
      *pImg++ = b;
      if (cOutChannels == 4) {
        a = cvtFloat2Byte(*(base + ao));
        *pImg++ = a;
      }
    }
  }
}

void stretchBlitCoreIrregulari(
  int NewW, int NewH, int cOutChannels, BYTE *pImg,
  int cxImgSize, int cyImgSize, int cImgChannels, BYTE *pbImage,
  char *channelOffsets, int *bitsPerChannel
  )
{
  // this version is assumes byte-aligned integer type channels,
  // but not necessarily equally sized channels.
  // It's got endian issues, and it implicity
  // restricts channels to sizeof(unsigend int) too!
  int i;
  int pixWidth = 0;

  int yNew, xNew, yOld, xOld;
  BYTE r, g, b, a;

  int ro = (channelOffsets[0]<0) ? -1 : 0;
  int go = (channelOffsets[1]<0) ? -1 : 0;
  int bo = (channelOffsets[2]<0) ? -1 : 0;
  int ao = (channelOffsets[3]<0) ? -1 : 0;

  // construct masks
  unsigned int rm = ~((~0)<<bitsPerChannel[0]);
  unsigned int gm = ~((~0)<<bitsPerChannel[1]);
  unsigned int bm = ~((~0)<<bitsPerChannel[2]);
  unsigned int am = ~((~0)<<bitsPerChannel[3]);

  int rb = bitsPerChannel[0];
  int gb = bitsPerChannel[1];
  int bb = bitsPerChannel[2];
  int ab = bitsPerChannel[3];

  // compute byte offsets to each channel
  for (i=0; i<channelOffsets[0]; i++) {
    ro += bitsPerChannel[i] >> 3;
  }
  for (i=0; i<channelOffsets[1]; i++) {
    go += bitsPerChannel[i] >> 3;
  }
  for (i=0; i<channelOffsets[2]; i++) {
    bo += bitsPerChannel[i] >> 3;
  }
  for (i=0; i<channelOffsets[3]; i++) {
    ao += bitsPerChannel[i] >> 3;
  }

  for (i=0; i<cImgChannels; i++) {
    pixWidth += bitsPerChannel[i];
  }
  pixWidth >>= 3;

  for (yNew = 0; yNew < NewH; yNew++)
  {
    yOld = yNew * cyImgSize / NewH;
    for (xNew = 0; xNew < NewW; xNew++)
    {
      unsigned int *pv;
      BYTE *base;
      xOld = xNew * cxImgSize / NewW;
      base = pbImage + pixWidth * ((yOld*cxImgSize) + xOld);

      if (ro>=0) {
        pv = (unsigned int*)(base+ro);
        r = cvtNbits2Byte(*pv & rm, rb);
      }
      if (go>=0) {
        pv = (unsigned int*)(base+go);
        g = cvtNbits2Byte(*pv & gm, gb);
      }
      if (bo>=0) {
        pv = (unsigned int*)(base+bo);
        b = cvtNbits2Byte(*pv & bm, bb);
      }
      *pImg++ = r;
      *pImg++ = g;
      *pImg++ = b;
      if (cOutChannels == 4) {
        pv = (unsigned int*)(base+ao);
        a =  cvtNbits2Byte(*pv & am, ab);
        *pImg++ = a;
      }
    }
  }
}
*/
#endif

#define extractChannel(_t) \
do{ \
  if (_t##o>=0) { \
    if (_t##t==IMDBG_FLOAT) { \
      if (_t##b == 32)     _t = cvtFloat2Byte( *((float*)(base+_t##o)) ); \
      else if (_t##b==64)  _t = cvtDouble2Byte( *((double*)(base+_t##o)) ); \
      else _t## = 0; /* Error! we don't support N-bit floats! */  \
    } \
    else if (_t##t==IMDBG_UINT || _t##t==IMDBG_INT) { \
      pv = (unsigned long*)(base+_t##o); \
      cvtNLongbits2Byte(*pv & _t##m, _t##b); \
    } \
  } } while(0)

#if 0
/*
void stretchBlitCoreIrregularm(
  int NewW, int NewH, int cOutChannels, BYTE *pImg,
  int cxImgSize, int cyImgSize, int cImgChannels, BYTE *pbImage,
  char *channelOffsets, int *bitsPerChannel, char *channelTypes
  )
{
  // this version is assumes byte-aligned channels,
  // but different sizes and float and integer can be
  // intermixed.
  // It's got endian issues, and it implicity
  // restricts channels to sizeof(unsigend int) too!

  int i;
  int pixWidth = 0;

  int yNew, xNew, yOld, xOld;
  BYTE r=0, g=0, b=0, a=0;

  // init offsets (computed later)
  int ro = (channelOffsets[0]<0) ? -1 : 0;
  int go = (channelOffsets[1]<0) ? -1 : 0;
  int bo = (channelOffsets[2]<0) ? -1 : 0;
  int ao = (channelOffsets[3]<0) ? -1 : 0;

  // construct masks
  unsigned long rm = ~((~0L)<<bitsPerChannel[0]);
  unsigned long gm = ~((~0L)<<bitsPerChannel[1]);
  unsigned long bm = ~((~0L)<<bitsPerChannel[2]);
  unsigned long am = ~((~0L)<<bitsPerChannel[3]);

  // construct masks
  char rt = channelTypes[0];
  char gt = channelTypes[1];
  char bt = channelTypes[2];
  char at = channelTypes[3];

  int rb = bitsPerChannel[0];
  int gb = bitsPerChannel[1];
  int bb = bitsPerChannel[2];
  int ab = bitsPerChannel[3];

  // compute byte offsets to each channel
  for (i=0; i<channelOffsets[0]; i++) {
    ro += bitsPerChannel[i] >> 3;
  }
  for (i=0; i<channelOffsets[1]; i++) {
    go += bitsPerChannel[i] >> 3;
  }
  for (i=0; i<channelOffsets[2]; i++) {
    bo += bitsPerChannel[i] >> 3;
  }
  for (i=0; i<channelOffsets[3]; i++) {
    ao += bitsPerChannel[i] >> 3;
  }

  for (i=0; i<cImgChannels; i++) {
    pixWidth += bitsPerChannel[i];
  }
  pixWidth >>= 3;

  for (yNew = 0; yNew < NewH; yNew++)
  {
    yOld = yNew * cyImgSize / NewH;
    for (xNew = 0; xNew < NewW; xNew++)
    {
      unsigned long *pv;
      BYTE *base;
      xOld = xNew * cxImgSize / NewW;
      base = pbImage + pixWidth * ((yOld*cxImgSize) + xOld);

      extractChannel(r);
      extractChannel(g);
      extractChannel(b);

      *pImg++ = r;
      *pImg++ = g;
      *pImg++ = b;
      if (cOutChannels == 4) {
        extractChannel(a);
        *pImg++ = a;

      }
    }
  }
}
*/
#endif

//----------------------------------------------------------------------------
#define bbMIN(a,b) (((a)<(b))?(a):(b))


//----------------------------------------------------------------------------
// STRETCH BLITTERS
//----------------------------------------------------------------------------
#define STRETCH 1
#define BLITNAME stretchBlitCoreRGBA8i
#include "blittemplate.h"

#define BLITNAME stretchBlitCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

// SCALING STRETCH BLITTERS
#define BLITNAME stretchBlitSCoreRGBA8i
#define SCALE
#define SRCCVTTYPE int
#define VAL2BYTE(x) cvtByteEx2Byte((int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

// BIASING STRETCH BLITTERS
#define BLITNAME stretchBlitBCoreRGBA8i
#undef SCALE
#define BIAS
#define SRCCVTTYPE int
#define VAL2BYTE(x) cvtByteEx2Byte((int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitBCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitBCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitBCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitBCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitBCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

// SCALING & BIASING STRETCH BLITTERS
#define BLITNAME stretchBlitSBCoreRGBA8i
#define SCALE
#define SRCCVTTYPE int
#define VAL2BYTE(x) cvtByteEx2Byte((int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSBCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSBCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSBCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSBCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME stretchBlitSBCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

#undef SCALE
#undef BIAS


//----------------------------------------------------------------------------
// STRAIGHT BLITTERS -- NO STRETCHING
//----------------------------------------------------------------------------
#undef STRETCH
#define BLITNAME blitCoreRGBA8i
#include "blittemplate.h"

#define BLITNAME blitCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME blitCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME blitCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME blitCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME blitCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

// SCALING BLITTERS
#define BLITNAME blitSCoreRGBA8i
#define SCALE
#define SRCCVTTYPE int
#define VAL2BYTE(x) cvtByteEx2Byte((int)x)
#include "blittemplate.h"

#define BLITNAME blitSCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME blitSCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME blitSCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME blitSCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME blitSCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

// BIASING BLITTERS
#define BLITNAME blitBCoreRGBA8i
#undef SCALE
#define BIAS
#define SRCCVTTYPE int
#define VAL2BYTE(x) cvtByteEx2Byte((int)x)
#include "blittemplate.h"

#define BLITNAME blitBCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME blitBCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME blitBCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME blitBCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME blitBCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

// SCALING & BIASING BLITTERS
#define BLITNAME blitSBCoreRGBA8i
#define SCALE
#define SRCCVTTYPE int
#define VAL2BYTE(x) cvtByteEx2Byte((int)x)
#include "blittemplate.h"

#define BLITNAME blitSBCoreRGBA16i
#define SRCCVTTYPE unsigned short
#define PTR2VAL(x,off) (*((unsigned short*)(x+off)))
#define VAL2BYTE(x) cvtUShort2Byte((unsigned short)x)
#include "blittemplate.h"

#define BLITNAME blitSBCoreRGBA32i
#define SRCCVTTYPE unsigned int
#define PTR2VAL(x,off) (*((unsigned int*)(x+off)))
#define VAL2BYTE(x) cvtUInt2Byte((unsigned int)x)
#include "blittemplate.h"

#define BLITNAME blitSBCoreRGBA32f
#define SRCCVTTYPE float
#define PTR2VAL(x,off) (*((float*)(x+off)))
#define VAL2BYTE(x) cvtFloat2Byte((float)x)
#include "blittemplate.h"

#define BLITNAME blitSBCoreRGBA64f
#define SRCCVTTYPE double
#define PTR2VAL(x,off) (*((double*)(x+off)))
#define VAL2BYTE(x) cvtDouble2Byte((double)x)
#include "blittemplate.h"

#define BLITNAME blitSBCoreRGBAByteColsBitOffsetsi
#define SRCCVTTYPE unsigned int
#define PTR2VAL_BIT_OFFSET(x,off,wid) bytePtr2Val_BitOffsets(x,off,wid)
#define VALBITS2BYTE(x,N) cvtNbits2Byte((unsigned int)x,N)
#include "blittemplate.h"

#undef SCALE
#undef BIAS
