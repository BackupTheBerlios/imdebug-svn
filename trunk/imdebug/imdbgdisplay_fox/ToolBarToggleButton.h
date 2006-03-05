/*---------------------------------------------------------------------------
     File name    : ToolBarToggleButton.h
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

#ifndef _TOOLBARTOGGLEBUTTON_H
#define _TOOLBARTOGGLEBUTTON_H

#include <FXToggleButton.h>

class ToolBarToggleButton : public FXToggleButton
{
  //FXDECLARE(ToolBarToggleButton)
protected:
  ToolBarToggleButton();
public:

  /// Construct toggle button with two text labels, and two icons, one for each state
  ToolBarToggleButton(FXComposite* p,const FXString& text1,const FXString& text2,FXIcon* icon1=NULL,FXIcon* icon2=NULL,FXObject* tgt=NULL,FXSelector sel=0,FXuint opts=TOGGLEBUTTON_NORMAL,FXint x=0,FXint y=0,FXint w=0,FXint h=0,FXint pl=DEFAULT_PAD,FXint pr=DEFAULT_PAD,FXint pt=DEFAULT_PAD,FXint pb=DEFAULT_PAD);

  /// Returns false because a toggle button can't receive focus
  virtual FXbool canFocus() const;
};

#endif // _TOOLBARTOGGLEBUTTON_H
