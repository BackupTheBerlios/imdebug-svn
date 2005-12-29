/*---------------------------------------------------------------------------
     File name    : CustomToolTip.h
     Author       : William Baxter
     Created      : 8/5/2003
  
     Description  : Tooltip subclass that supports maybe not rich text, but
                    at least upper-middle class text.
  --------------------------------------------------------------------------*
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

#ifndef _CUSTOMTOOLTIP_H
#define _CUSTOMTOOLTIP_H

#include <fx.h>

class CustomToolTip : public FXToolTip
{
  FXDECLARE(CustomToolTip)
protected:
  CustomToolTip() : FXToolTip() {}
private:
  CustomToolTip(const CustomToolTip&);
  CustomToolTip& operator=(const CustomToolTip&);
public:
  long onPaint(FXObject*,FXSelector,void*);

public:
  /// Construct a tool tip
  CustomToolTip(FXApp* a,FXuint opts=TOOLTIP_NORMAL,FXint x=0,FXint y=0,FXint w=0,FXint h=0)
    : FXToolTip(a,opts,x,y,w,h) {}
  /// Return default width
  virtual FXint getDefaultWidth();

  void setStyle( FXuint toolstyle ) {
    options &=  ~(TOOLTIP_NORMAL|TOOLTIP_PERMANENT|TOOLTIP_VARIABLE);
    options |=   toolstyle&(TOOLTIP_NORMAL|TOOLTIP_PERMANENT|TOOLTIP_VARIABLE);
  }
  FXuint getStyle( ) const { 
    return options&(TOOLTIP_NORMAL|TOOLTIP_PERMANENT|TOOLTIP_VARIABLE);
  }


};
 

#endif // _CUSTOMTOOLTIP_H
