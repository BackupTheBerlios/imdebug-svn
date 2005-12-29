/*---------------------------------------------------------------------------
     File name    : ToolBarButton.cpp
     Author       : William Baxter
     Created      : 11/2/2003
  
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
#include <fx.h>
#include "ToolBarButton.h"

//FXIMPLEMENT(ToolBarButton,FXButton,NULL,0)

ToolBarButton::ToolBarButton() : FXButton() {}

ToolBarButton::ToolBarButton(
 FXComposite*  p,
 const FXString&  text,
 FXIcon*  ic,
 FXObject*  tgt,
 FXSelector  sel,
 FXuint  opts,
 FXint  x,
 FXint  y,
 FXint  w,
 FXint  h,
 FXint  pl,
 FXint  pr,
 FXint  pt,
 FXint  pb) 
 : FXButton(p,text,ic,tgt,sel,opts,
            x,y,w,h,pl,pr,pt,pb)

{
}

FXbool ToolBarButton::canFocus() const
{
  return false;
}
