/*---------------------------------------------------------------------------
     File name    : CustomStatusBar.h
     Author       : William Baxter
     Created      : 8/3/2003
  
     Description  : 
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
#ifndef _CUSTOMSTATUSBAR_H
#define _CUSTOMSTATUSBAR_H

#include <FXStatusBar.h>

class CustomStatusLine : public FXStatusLine
{
  FXDECLARE(CustomStatusLine)
public:
  CustomStatusLine(FXComposite* p,FXObject* tgt=NULL,FXSelector sel=0);
  ~CustomStatusLine();

  long onPaint(FXObject*,FXSelector,void*);
protected:
  CustomStatusLine();

private:
};


class CustomStatusBar : public FXStatusBar
{
  FXDECLARE(CustomStatusBar)
public:
  CustomStatusBar(
    FXComposite* p,FXuint opts=0,
    FXint x=0,FXint y=0,FXint w=0,FXint h=0,
    FXint pl=3,FXint pr=3,FXint pt=2,FXint pb=2,FXint hs=4,FXint vs=0
    );
  ~CustomStatusBar();
protected:
  CustomStatusBar() {};
  
private:
};


#endif // _CUSTOMSTATUSBAR_H
