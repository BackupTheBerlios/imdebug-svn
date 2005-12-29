/*
 *============================================================================
 * blitfuncdecl.h
 *   this file lists all the different blitter funcs, and some attribute
 *   codes for each.  This is included two different places with two
 *   different definitions for the DECLARE_BLITTER macro.  One place
 *   it generates all the function prototypes in blitfuncs.h, the other
 *   place it fills in a big table of all the function pointers in 
 *   imdbgdisplay.c.
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

//----------------------------------------------------------------------------
// STRETCH BLIT FUNCTIONS
//----------------------------------------------------------------------------


// Attribs legend:
// z - zoomed (stretched)
// s - scaled
// b - biased
// 8 -  8 bit channels
// 6 - 16 bit channels
// 2 - 32 bit channels
// 4 - 64 bit channels
// n - nonuniform sized channels, but byte-aligned pixels
// i - int channels
// f - float channels

DECLARE_BLITTER(stretchBlitCoreRGBA8i,  z8i)
DECLARE_BLITTER(stretchBlitCoreRGBA16i, z6i)
DECLARE_BLITTER(stretchBlitCoreRGBA32i, z2i)
DECLARE_BLITTER(stretchBlitCoreRGBA32f, z2f)
DECLARE_BLITTER(stretchBlitCoreRGBA64f, z4f)
DECLARE_BLITTER(stretchBlitCoreRGBAByteColsBitOffsetsi, zni)

// WITH SCALE
DECLARE_BLITTER(stretchBlitSCoreRGBA8i,  zs8i)
DECLARE_BLITTER(stretchBlitSCoreRGBA16i, zs6i)
DECLARE_BLITTER(stretchBlitSCoreRGBA32i, zs2i)
DECLARE_BLITTER(stretchBlitSCoreRGBA32f, zs2f)
DECLARE_BLITTER(stretchBlitSCoreRGBA64f, zs4f)
DECLARE_BLITTER(stretchBlitSCoreRGBAByteColsBitOffsetsi, zsni)

// WITH BIAS
DECLARE_BLITTER(stretchBlitBCoreRGBA8i,  zb8i)
DECLARE_BLITTER(stretchBlitBCoreRGBA16i, zb6i)
DECLARE_BLITTER(stretchBlitBCoreRGBA32i, zb2i)
DECLARE_BLITTER(stretchBlitBCoreRGBA32f, zb2f)
DECLARE_BLITTER(stretchBlitBCoreRGBA64f, zb4f)
DECLARE_BLITTER(stretchBlitBCoreRGBAByteColsBitOffsetsi, zbni)

// WITH SCALE & BIAS
DECLARE_BLITTER(stretchBlitSBCoreRGBA8i,  zsb8i)
DECLARE_BLITTER(stretchBlitSBCoreRGBA16i, zsb6i)
DECLARE_BLITTER(stretchBlitSBCoreRGBA32i, zsb2i)
DECLARE_BLITTER(stretchBlitSBCoreRGBA32f, zsb2f)
DECLARE_BLITTER(stretchBlitSBCoreRGBA64f, zsb4f)
DECLARE_BLITTER(stretchBlitSBCoreRGBAByteColsBitOffsetsi, zsbni)


/*
void stretchBlitCoreIrregulari()
void stretchBlitCoreIrregularf()
void stretchBlitCoreIrregularMix()
*/

//----------------------------------------------------------------------------
// STRAIGHT BLIT FUNCTIONS (NO STRETCH)
//----------------------------------------------------------------------------

// PLAIN
DECLARE_BLITTER(blitCoreRGBA8i,  8i)
DECLARE_BLITTER(blitCoreRGBA16i, 6i)
DECLARE_BLITTER(blitCoreRGBA32i, 2i)
DECLARE_BLITTER(blitCoreRGBA32f, 2f)
DECLARE_BLITTER(blitCoreRGBA64f, 4f)
DECLARE_BLITTER(blitCoreRGBAByteColsBitOffsetsi, ni)

// WITH SCALE
DECLARE_BLITTER(blitSCoreRGBA8i,  s8i)
DECLARE_BLITTER(blitSCoreRGBA16i, s6i)
DECLARE_BLITTER(blitSCoreRGBA32i, s2i)
DECLARE_BLITTER(blitSCoreRGBA32f, s2f)
DECLARE_BLITTER(blitSCoreRGBA64f, s4f)
DECLARE_BLITTER(blitSCoreRGBAByteColsBitOffsetsi, sni)

// WITH BIAS
DECLARE_BLITTER(blitBCoreRGBA8i,  b8i)
DECLARE_BLITTER(blitBCoreRGBA16i, b6i)
DECLARE_BLITTER(blitBCoreRGBA32i, b2i)
DECLARE_BLITTER(blitBCoreRGBA32f, b2f)
DECLARE_BLITTER(blitBCoreRGBA64f, b4f)
DECLARE_BLITTER(blitBCoreRGBAByteColsBitOffsetsi, bni)

// WITH SCALE & BIAS
DECLARE_BLITTER(blitSBCoreRGBA8i,  sb8i)
DECLARE_BLITTER(blitSBCoreRGBA16i, sb6i)
DECLARE_BLITTER(blitSBCoreRGBA32i, sb2i)
DECLARE_BLITTER(blitSBCoreRGBA32f, sb2f)
DECLARE_BLITTER(blitSBCoreRGBA64f, sb4f)
DECLARE_BLITTER(blitSBCoreRGBAByteColsBitOffsetsi, sbni)
