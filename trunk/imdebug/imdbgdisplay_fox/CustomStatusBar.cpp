  /*-----------------------------------------------------------*

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
 
  *-----------------------------------------------------------*/

#include <fx.h>
#include "CustomStatusBar.h"

// Map
FXDEFMAP(CustomStatusLine) CustomStatusLineMap[]={
  FXMAPFUNC(SEL_PAINT,0,CustomStatusLine::onPaint)
  };

// Object implementation
FXIMPLEMENT(CustomStatusLine,FXStatusLine,CustomStatusLineMap,ARRAYNUMBER(CustomStatusLineMap))

CustomStatusLine::CustomStatusLine(FXComposite* p,FXObject* tgt,FXSelector sel)
  : FXStatusLine(p,tgt,sel)
{
}

CustomStatusLine::CustomStatusLine() : FXStatusLine()
{
}
CustomStatusLine::~CustomStatusLine() 
{
}



long CustomStatusLine::onPaint(FXObject*o,FXSelector s,void*p)
{
  // paint using some simple tags
  static FXColor textcolors[] =
  {
      FXRGB(180,50,50), // RED
      FXRGB(30,140,30),  // GREEN
      FXRGB(50,50,200),   // BLUE
      FXRGB(100,100,100),  // ALPHA
      FXRGB(0,0,0)          // BLACK
  };

  FXEvent *ev=(FXEvent*)p;
  FXDCWindow dc(this,ev);
  FXint ty=padtop+(height-padtop-padbottom-font->getFontHeight())/2;
  FXint pos,startpos,len;
  dc.setForeground(backColor);
  dc.fillRectangle(ev->rect.x,ev->rect.y,ev->rect.w,ev->rect.h);
  if(!status.empty()){
    dc.setFont(font);
    pos = 0;
    startpos = 0;
    len = status.length();
    FXint tx = padleft;
    FXColor curcolor = textColor;
    while (pos<len) {
      dc.setForeground(curcolor);
      pos=status.find('<',pos);
      if (pos<0) {
        dc.drawText(tx,ty+font->getFontAscent(),status.text()+startpos,len-startpos);
        // no more tags
        break;
      }
      else {
        // there's another tag after this
        dc.drawText(tx,ty+font->getFontAscent(),status.text()+startpos,pos-startpos);
        tx += font->getTextWidth(status.text()+startpos,pos-startpos);
        if (pos<len-1 && pos!=-1) {
          switch(status[pos+1]) {
          case 'R': curcolor = textcolors[0]; break;
          case 'G': curcolor = textcolors[1]; break;
          case 'B': curcolor = textcolors[2]; break;
          case 'A': curcolor = textcolors[3]; break;
          case 'K': curcolor = textcolors[4]; break;
          default:
            curcolor = textColor;
          }
          pos += 3;
        }
        startpos = pos;
      }
    }
  }
  drawFrame(dc,0,0,width,height);
  return 1;
  //  return FXStatusLine::onPaint(o,s,p);
}


//////////////////////////////////////////////////////////////////////////

// Object implementation
FXIMPLEMENT(CustomStatusBar, FXStatusBar,NULL,0)

CustomStatusBar::CustomStatusBar(
  FXComposite* p,FXuint opts,FXint x,FXint y,FXint w,FXint h,
  FXint pl,FXint pr,FXint pt,FXint pb,FXint hs,FXint vs
  ) :
  FXStatusBar(p,opts,x,y,w,h,pl,pr,pt,pb,hs,vs)
{
    delete status;
    status = new CustomStatusLine(this);
}

CustomStatusBar::~CustomStatusBar()
{
}
