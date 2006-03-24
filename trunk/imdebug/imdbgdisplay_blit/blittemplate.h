/*
 *============================================================================
 * blittemplate.h
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
 *============================================================================
 * This file is included many times with different #defines surrounding
 * it to create a variety of different blitting routines.
 *
 * Things to #define ahead of time
 *    STRETCH     (def or undef)    - perform zooming while blitting
 *    SCALE       (def or undef)    - scale values while blitting
 *    BIAS        (def or undef)    - bias values while blitting
 *    BLITNAME    stretchBlitRBGA8i, etc
 *    SRCCVTTYPE  type of single channel's data
 *
 * Also define one of:
 *    PTR2VAL(x,off)
 *              convert from BYTE pointer and #of BYTES offset to value
 *              type (e.g. by casting)
 *    PTR2VAL_BIT_OFFSET(x,off,width)
 *              convert from BYTE pointer and #of BITS offset and BIT width
 *              to value type (e.g. by casting)
 *
 * And define one of:
 *    VAL2BYTE(x)        converts raw number, x, into a [0,255] value
 *    VALBITS2BYTE(x,w)  converts a w-bit number, x, into a [0,255] value
 *
 * If desired, these channel specializations can be defined:
 *    RVAL2BYTE(x)       same as VAL2BYTE, but just for red channel
 *    GVAL2BYTE(x)       same as VAL2BYTE, but just for green channel
 *    BVAL2BYTE(x)       same as VAL2BYTE, but just for blue channel
 *    AVAL2BYTE(x)       same as VAL2BYTE, but just for alpha channel
 *    RVALBITS2BYTE(x)   same as VALBITS2BYTE, but just for red channel
 *    GVALBITS2BYTE(x)   same as VALBITS2BYTE, but just for green channel
 *    BVALBITS2BYTE(x)   same as VALBITS2BYTE, but just for blue channel
 *    AVALBITS2BYTE(x)   same as VALBITS2BYTE, but just for alpha channel
 *
 * Overall pixel conversion given a byte pointer, p, looks generally like
 *     VAL2BYTE( DOBIAS( DOSCALE( PTR2VAL(p) ) ) )
 *
 *==========================================================================*/

#if !defined(STRETCH)
#define SRCPOSTYPE int
#else
#define SRCPOSTYPE float
#endif

#if !defined(SCALE)
#define DOSCALE(ch,x) (x)
#else
#define DOSCALE(ch,x) ((x)*(pProps->scale[ch]))
#endif

#if !defined(BIAS)
#define DOBIAS(ch,x) (x)
#else
#define DOBIAS(ch,x) ((x)+(pProps->bias[ch]))
#endif

#if !defined(PTR2VAL) && !defined(PTR2VAL_BIT_OFFSET)
// default assumes BYTE-sized channel values and whole-byte offsets
#define PTR2VAL(x,off) (*(x+off))
#endif

#if !defined(VAL2BYTE) && !defined(VALBITS2BYTE)
// default assumes cast data values really are bytes
#if !defined (RVAL2BYTE) && !defined(RVALBITS2BYTE)
#define RVAL2BYTE(x) ((BYTE)(x))
#endif
#if !defined(GVAL2BYTE) && !defined(GVALBITS2BYTE)
#define GVAL2BYTE(x) ((BYTE)(x))
#endif
#if !defined(BVAL2BYTE) && !defined(BVALBITS2BYTE)
#define BVAL2BYTE(x) ((BYTE)(x))
#endif
#if !defined(AVAL2BYTE) && !defined(AVAL2BITSBYTE)
#define AVAL2BYTE(x) ((BYTE)(x))
#endif
#endif

#if !defined(SRCCVTTYPE)
#define SRCCVTTYPE BYTE
#endif

void BLITNAME
 (
  BYTE* pDiData,
  int dstDataW, // W in pixels of bitmap we're copying into
  int dstDataH, // H in pixels of bitmap we're copying into
  int dstW, int dstH, int dstX, int dstY, // rect of region to copy to
  BYTE* pSrc,
  int srcDataW, // W in pixels of src data we're copying from
  int srcDataColBytes, // W in bytes of single pixel of data we're copying from
  int srcW, int srcH, int srcX, int srcY,  // subrect of region to copy from
  char *srcChanOffsets, // this is 4 offsets, in BYTES (or <0 for unused chan)
  ImProps *pProps,
  BlitStats *stat
  )
{
  BYTE r=0,g=0,b=0,a=255;
  BYTE *src, *dst;

  SRCCVTTYPE rc, gc, bc, ac;

  // channel offsets
  int rch = pProps->chanmap[0];
  int gch = pProps->chanmap[1];
  int bch = pProps->chanmap[2];
  int ach = pProps->chanmap[3];
#if defined(PTR2VAL)
  int ro = srcChanOffsets[0];
  int go = srcChanOffsets[1];
  int bo = srcChanOffsets[2];
  int ao = srcChanOffsets[3];
#elif defined(PTR2VAL_BIT_OFFSET)
  int ro = (rch>=0) ? pProps->bitoffset[rch] : -1;
  int go = (gch>=0) ? pProps->bitoffset[gch] : -1;
  int bo = (bch>=0) ? pProps->bitoffset[bch] : -1;
  int ao = (ach>=0) ? pProps->bitoffset[ach] : -1;
  int rw = (rch>=0) ? pProps->bpc[rch] : 0;
  int gw = (gch>=0) ? pProps->bpc[gch] : 0;
  int bw = (bch>=0) ? pProps->bpc[bch] : 0;
  int aw = (ach>=0) ? pProps->bpc[ach] : 0;
#endif

  SRCPOSTYPE xSrc,ySrc;
  int xDst,yDst;
  int dstR,dstB;

  int dstDataColBytes = BLITTER_DEST_CHANNELS;
  int dstDataRowBytes;

  int srcDataRowBytes;

  float af, bf; // alpha blending factors;

#if STRETCH
  BYTE *srcRowStart;
  float xInc = (srcW) / (float)(dstW);
  float yInc = (srcH) / (float)(dstH);
  float srcXT = (float)srcX;
  float srcYT = (float)srcY;
#else
  const int xInc = 1;
  const int yInc = 1;
  int srcXT = srcX;
  int srcYT = srcY;
#endif

#if STRETCH
  // compute right and bottom stopping conditions (in pixels)
  dstR = bbMIN( dstDataW, dstX + dstW );
  dstB = bbMIN( dstDataH, dstY + dstH );
#else
  // compute right and bottom stopping conditions (in pixels)
  dstR = bbMIN( dstDataW, dstX + bbMIN( dstW, srcW ) );
  dstB = bbMIN( dstDataH, dstY + bbMIN( dstH, srcH ) );
#endif

  // Compute right and top clipping
  if (dstX < 0) {
    srcXT += (-dstX) * xInc;
    dstX = 0;
  }
  if (dstY < 0) {
    srcYT += (-dstY) * yInc;
    dstY = 0;
  }

  // this >>, << wierdness has to do with DIB alignment I think ...
  dstDataRowBytes = ((dstDataColBytes * dstDataW + 3L) >> 2) << 2;

  srcDataRowBytes = srcDataW * srcDataColBytes;


  for (ySrc=srcYT, yDst=dstY; yDst<dstB; yDst++)
  {

    // compute row starting addresses
#if STRETCH
    srcRowStart = pSrc + ((int)ySrc) * srcDataRowBytes;
#else
    src = pSrc    + ySrc * srcDataRowBytes + srcXT * srcDataColBytes;
#endif
    dst = pDiData + yDst * dstDataRowBytes + dstX * dstDataColBytes;

    for (xSrc=srcXT, xDst=dstX; xDst<dstR; xDst++)
    {
#if STRETCH
      src = srcRowStart + ((int)xSrc) * srcDataColBytes;
#endif

      // ------------ EXTRACT RED VALUE ---------------
      if (ro>=0) {
#if defined(PTR2VAL)
        rc = PTR2VAL(src,ro);
#elif defined(PTR2VAL_BIT_OFFSET)
        rc = PTR2VAL_BIT_OFFSET(src,ro,rw);
#endif

#if defined(RVAL2BYTE)
        r = RVAL2BYTE( DOBIAS(0, DOSCALE(0, rc ) ) );
#elif defined(RVALBITS2BYTE)
        r = RVALBITS2BYTE( DOBIAS(0, DOSCALE(0, rc ) ), rw );
#elif defined(VAL2BYTE)
        r = VAL2BYTE( DOBIAS(0, DOSCALE(0, rc ) ) );
#elif defined(VALBITS2BYTE)
        r = VALBITS2BYTE( DOBIAS(0, DOSCALE(0, rc ) ), rw );
#endif
      }
      // ------------ EXTRACT GREEN VALUE ---------------
      if (go>=0) {
#ifdef PTR2VAL
        gc = PTR2VAL(src,go);
#elif defined(PTR2VAL_BIT_OFFSET)
        gc = PTR2VAL_BIT_OFFSET(src,go,gw);
#endif

#if defined(GVAL2BYTE)
        g = GVAL2BYTE( DOBIAS(1, DOSCALE(1, gc ) ) );
#elif defined(GVALBITS2BYTE)
        g = GVALBITS2BYTE( DOBIAS(1, DOSCALE(1, gc ) ), gw );
#elif defined(VAL2BYTE)
        g = VAL2BYTE( DOBIAS(1, DOSCALE(1, gc ) ) );
#elif defined(VALBITS2BYTE)
        g = VALBITS2BYTE( DOBIAS(1, DOSCALE(1, gc ) ), gw );
#endif
      }
      // ------------ EXTRACT BLUE VALUE ---------------
      if (bo>=0) {
#ifdef PTR2VAL
        bc = PTR2VAL(src,bo);
#elif defined(PTR2VAL_BIT_OFFSET)
        bc = PTR2VAL_BIT_OFFSET(src,bo,bw);
#endif

#if defined(BVAL2BYTE)
        b = BVAL2BYTE( DOBIAS(2, DOSCALE(2, bc ) ) );
#elif defined(BVALBITS2BYTE)
        b = BVALBITS2BYTE( DOBIAS(2, DOSCALE(2, bc ) ), bw );
#elif defined(VAL2BYTE)
        b = VAL2BYTE( DOBIAS(2, DOSCALE(2, bc ) ) );
#elif defined(VALBITS2BYTE)
        b = VALBITS2BYTE( DOBIAS(2, DOSCALE(2, bc ) ), bw );
#endif
      }
      // ------------ EXTRACT ALPHA VALUE ---------------
      if (ao>=0)
      {
#ifdef PTR2VAL
        ac = PTR2VAL(src,ao);
#elif defined(PTR2VAL_BIT_OFFSET)
        ac = PTR2VAL_BIT_OFFSET(src,ao,aw);
#endif

#if defined(AVAL2BYTE)
        a = AVAL2BYTE( DOBIAS(3, DOSCALE(3, ac ) ) );
#elif defined(AVALBITS2BYTE)
        a = AVALBITS2BYTE( DOBIAS(3, DOSCALE(3, ac ) ), aw );
#elif defined (VAL2BYTE)
        a = VAL2BYTE( DOBIAS(3, DOSCALE(3, ac ) ) );
#elif defined(VALBITS2BYTE)
        a = VALBITS2BYTE( DOBIAS(3, DOSCALE(3, ac ) ), aw );
#endif
      }
      if (stat) {
        if ((float)rc < stat->min[0]) stat->min[0] = (float)rc;
        if ((float)gc < stat->min[1]) stat->min[1] = (float)gc;
        if ((float)bc < stat->min[2]) stat->min[2] = (float)bc;
        if ((float)ac < stat->min[3]) stat->min[3] = (float)ac;
        if ((float)rc > stat->max[0]) stat->max[0] = (float)rc;
        if ((float)gc > stat->max[1]) stat->max[1] = (float)gc;
        if ((float)bc > stat->max[2]) stat->max[2] = (float)bc;
        if ((float)ac > stat->max[3]) stat->max[3] = (float)ac;
      }
      else {
        if (ao>=0)
        {
          // compute alpha blend w/ background
          af = a / 255.0f;
          bf = 1.0f - af;
#ifdef BLITTER_DEST_BGR
          dst[2] = (BYTE)(af * r + bf * dst[2] + 0.5f);
          dst[1] = (BYTE)(af * g + bf * dst[1] + 0.5f);
          dst[0] = (BYTE)(af * b + bf * dst[0] + 0.5f);
#else
          dst[0] = (BYTE)(af * r + bf * dst[2] + 0.5f);
          dst[1] = (BYTE)(af * g + bf * dst[1] + 0.5f);
          dst[2] = (BYTE)(af * b + bf * dst[0] + 0.5f);
#endif
        }
        else {
#ifdef BLITTER_DEST_BRG
          dst[2] = r;
          dst[1] = g;
          dst[0] = b;
#else
          dst[0] = r;
          dst[1] = g;
          dst[2] = b;
#endif
        }
      }
      src += srcDataColBytes;
      dst += dstDataColBytes;
      xSrc+=xInc;
    }
    ySrc+=yInc;
  }
}


#undef SRCPOSTYPE
#undef BLITNAME
#undef VAL2BYTE
#undef VALBITS2BYTE
#undef RVAL2BYTE
#undef GVAL2BYTE
#undef BVAL2BYTE
#undef AVAL2BYTE
#undef RVALBITS2BYTE
#undef GVALBITS2BYTE
#undef BVALBITS2BYTE
#undef AVALBITS2BYTE
#undef PTR2VAL
#undef PTR2VAL_BIT_OFFSET
#undef SRCCVTTYPE
#undef DOSCALE
#undef DOBIAS
// Don't undef these
//#undef STRETCH
//#undef BIAS
//#undef SCALE
