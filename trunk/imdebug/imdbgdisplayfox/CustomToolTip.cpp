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

#include "CustomToolTip.h"


/************************************************************************/
#define HSPACE  4
#define VSPACE  2


/************************************************************************/
FXDEFMAP(CustomToolTip) CustomToolTipMap[]={
  FXMAPFUNC(SEL_PAINT,0,CustomToolTip::onPaint)
};

// Object implementation
FXIMPLEMENT(CustomToolTip,FXToolTip,CustomToolTipMap,ARRAYNUMBER(CustomToolTipMap))

/************************************************************************/

inline int hexval(FXchar c)
{
  if (c>='0' && c<='9') return c-'0';
  else if (c>='a' && c<='f') return 10+c-'a';
  else if (c>='A' && c<='F') return 10+c-'A';
  else return 0;
}

long CustomToolTip::onPaint(
  FXObject*sender,
  FXSelector sel,
  void * p)
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
  dc.setForeground(backColor);
  dc.fillRectangle(ev->rect.x,ev->rect.y,ev->rect.w,ev->rect.h);
  dc.setForeground(textColor);
  dc.setFont(font);
  dc.drawRectangle(0,0,width-1,height-1);

  FXint pos,len;
  if(!label.empty()){
    FXString swatchcolor=label.section("\t",0);
    FXString thelabel   =label.section("\t",1);
    if (thelabel.empty()) { 
      thelabel=swatchcolor;
      swatchcolor = "";
    }
    dc.setFont(font);
    pos = 0;
    len = thelabel.length();
    const FXchar *strend,*strbeg;
    FXint txstart=1+HSPACE;
    FXint ty=1+VSPACE+font->getFontAscent();
    if (!swatchcolor.empty())
    {
      FXint dim = font->getFontHeight()*2 - 2;
      int r = 16*hexval(swatchcolor[2])+hexval(swatchcolor[3]);
      int g = 16*hexval(swatchcolor[4])+hexval(swatchcolor[5]);
      int b = 16*hexval(swatchcolor[6])+hexval(swatchcolor[7]);
      // draw the swatch color
      dc.setForeground(FXRGB(r,g,b));
      dc.fillRectangle(ev->rect.x+HSPACE+2,ev->rect.y+VSPACE+2,dim,dim);
      dc.setForeground(FXRGB(0,0,0));
      dc.drawRectangle(ev->rect.x+HSPACE+2,ev->rect.y+VSPACE+2,dim,dim);
      dc.setForeground(textColor);
      txstart+=HSPACE+1+dim;
    }
    FXColor curcolor = textColor;
    FXint tx=txstart;
    strbeg = thelabel.text();
    while (pos>=0) {
      dc.setForeground(curcolor);
      strbeg = thelabel.text()+pos;
      pos=thelabel.find_first_of("<\n",2,pos);
      if (pos<0) {
        // no more tags or newlines
        strend = thelabel.text()+len;
        dc.drawText(tx,ty,strbeg,strend-strbeg);
      }
      else if (thelabel[pos] == '\n') { 
        strend = thelabel.text()+pos;
        dc.drawText(tx,ty,strbeg,strend-strbeg);
        ty+=font->getFontHeight();
        tx = txstart;
        pos++;
      }
      else {
        // next thing is another tag
        strend = thelabel.text()+pos;
        dc.drawText(tx,ty,strbeg,strend-strbeg);
        tx += font->getTextWidth(strbeg,strend-strbeg);
        if (pos<len-1 && pos!=-1) {
          switch(thelabel[pos+1]) {
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
      }
    }
  }
  return 1;
  //  return FXStatusLine::onPaint(o,s,p);
}



// Get default width
FXint CustomToolTip::getDefaultWidth(){
  const FXchar *beg,*end;
  FXint w,tw=0;
  FXString slabel(label);
  FXint swatchwidth = 0;
  if ( slabel.find('\t') >= 0 )
  {
    slabel = label.section('\t',1);
    swatchwidth = HSPACE + font->getFontHeight()*2 + VSPACE;
  }
  FXint pos=0;
  // strip label of special <> tags
  while((pos=slabel.find('<',pos))>=0) {
    FXint epos = slabel.find('>',pos);
    slabel.remove(pos,epos-pos+1);
  }
  beg=slabel.text();
  if(beg){
    do{
      end=beg;
      while(*end!='\0' && *end!='\n') end++;
      if((w=font->getTextWidth(beg,end-beg))>tw) tw=w;
      beg=end+1;
    }
    while(*end!='\0');
  }
  return tw+HSPACE+HSPACE+2 + swatchwidth;
}
