/*---------------------------------------------------------------------------
     File name    : ImdebugImageView.h
     Author       : William Baxter
     Created      : 8/5/2003
  
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
#ifndef _IMDEBUGIMAGEVIEW_H
#define _IMDEBUGIMAGEVIEW_H


#include <fx.h>

class ImdebugImageView : public FXImageFrame
{
  FXDECLARE(ImdebugImageView)
protected:
  FXString statusHelpText;
  FXString tooltipText;
protected:
  ImdebugImageView();
private:
  ImdebugImageView(const ImdebugImageView&);
  ImdebugImageView &operator=(const ImdebugImageView&);
public:
  
  /// Construct Imdebug image frame
  ImdebugImageView(
    FXComposite* p,FXImage *img,
    FXuint opts=FRAME_SUNKEN|FRAME_THICK,
    FXint x=0,FXint y=0,FXint w=0,FXint h=0,
    FXint pl=0,FXint pr=0,FXint pt=0,FXint pb=0
    );
  
  void setHelpText(const FXString &);
  void setTipText(const FXString &);
public:
  long onQueryTip(FXObject*sender,FXSelector sel, void*ptr);
  long onQueryHelp(FXObject*sender,FXSelector sel, void*ptr);
  
};


#endif // _IMDEBUGIMAGEVIEW_H
