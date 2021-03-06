/*============================================================================
 * testapp.cpp
 *   Very basic console program that calls imdebug() in a few different ways
 *   with a few different image formats.  Hit enter to cycle through the
 *   images.
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
#include "imdebug.h"
#include <stdio.h>
#include <stdlib.h>

struct Bits565 {
  unsigned short r : 5;
  unsigned short g : 6;
  unsigned short b : 5;
};

unsigned char testRGB[16*17*3];
unsigned char testRGBA[16*17*4];
unsigned short testRGB16[16*17*3];
float testRGBf[16*17*3];
unsigned char testBigRGBA[512*512*4];
Bits565 testRGB565[16*17];

int display(int which)
{
  switch (which)
  {

  case 0: imdebug("rgb w=16 h=17 t='first' %p", testRGB);
    break;
//  case 0: imdebug("rgb b=5,6,5 w=%d h=%d t='foo' %p", 16, 17, testRGB565);
//    break;
  case 1: imdebug("rgb rbga=__gg w=%d h=%d t='can\\'t touch dis' %p", 16, 17, testRGB);
    break;
  case 2: imdebug("rgb rbga=gggg *auto b=32f w=%d h=%d t=%s %p", 16, 17, "purty huh?", testRGBf);
    break;
  case 3: imdebug("rgb *0.2 b=32f w=%d h=%d t=12 %p", 16, 17, testRGBf);
    break;
  case 4: imdebug("rgb w=%d h=%d %p", 9000, 9000, testRGB);//error too big
    break;
  case 5: imdebug("rgb w=%d h=%d %p", 10, 10, 0);// error null ptr
    break;
  case 6: imdebug("rgb rbga=__gg b=16 w=%d h=%d t=%d %p", 16, 17, 42, testRGB16);
    break;
  case 7: imdebug("lum b=32f w=%d h=%d %p", 16*3, 17, testRGBf);
    break;
  case 8: imdebug("rgba  t='rando' w=%d h=%d %p", 512, 512, testBigRGBA);
    break;
  case 9: imdebug("#6 rgba=#0145 w=%d h=%d t='last' %p", 8, 17, testRGB);
    return 0;
  }
  return which + 1;
}

void setup()
{
  // make up some images
  for (int i=0; i<16*17; i++) {
    int off = 3 * i;
    if (i % 3) {
      testRGB[off+0] = 255;
      testRGB[off+1] = 128;
      testRGB[off+2] = 128;

      testRGBf[off+0] = 1.0;
      testRGBf[off+1] = 0.5;
      testRGBf[off+2] = 0.5;

      testRGB16[off+0] = ~0;
      testRGB16[off+1] = 1<<15;
      testRGB16[off+2] = 1<<15;

      off = 4*i;
      testRGBA[off+0] = 0;
      testRGBA[off+1] = 255;
      testRGBA[off+2] = 0;
      testRGBA[off+3] = 255;

      //                   r:5      g:6       b:5
      //                   1.0      0.5       0.5
      // testRGB565[i] = ( (31) | (32<<5) | (16<<11) );
      // Same result as this:
      testRGB565[i].r = 31;
      testRGB565[i].g = 32;
      testRGB565[i].b = 16;
    } else {
      testRGB[off+0] = 0;
      testRGB[off+1] = 255;
      testRGB[off+2] = 255;

      testRGBf[off+0] = 0;
      testRGBf[off+1] = 1.0;
      testRGBf[off+2] = 1.0;

      testRGB16[off+0] = 0;
      testRGB16[off+1] = ~0;
      testRGB16[off+2] = ~0;

      off = 4*i;
      testRGBA[off+0] = 255;
      testRGBA[off+1] = 0;
      testRGBA[off+2] = 128;
      testRGBA[off+3] = 255;

      //                    r:5      g:6       b:5
      //                    0.0      1.0       1.0
      // testRGB565[i] = (  (0) |  (63<<5) | (31<<11) |
      testRGB565[i].r = 0;
      testRGB565[i].g = 63;
      testRGB565[i].b = 31;
    }
  }

  for (i=0; i<512*512*4; i++) {
    testBigRGBA[i] = rand()%255;
  }
}

int main()
{
  setup();
  printf("Hit enter to step through images\n");
  int n = 0;
  while (1) {
    int ret = getc(stdin);
    n = display(n);
    printf("Sent image %d\n",n);
    if (n==0) setup(); // rerandomize random image
  }

  return 0;
}
