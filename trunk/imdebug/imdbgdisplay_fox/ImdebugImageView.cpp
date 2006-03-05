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
#include "ImdebugImageView.h"

FXDEFMAP(ImdebugImageView) ImdebugImageViewMap[]={
  FXMAPFUNC(SEL_QUERY_TIP,0,ImdebugImageView::onQueryTip),
  FXMAPFUNC(SEL_QUERY_HELP,0,ImdebugImageView::onQueryHelp)
  };


// Object implementation
FXIMPLEMENT(ImdebugImageView,FXImageFrame,ImdebugImageViewMap,ARRAYNUMBER(ImdebugImageViewMap))

// Deserialization
ImdebugImageView::ImdebugImageView()
{
}


// Construct it
ImdebugImageView::ImdebugImageView(
  FXComposite* p,FXImage *img,FXuint opts,
  FXint x,FXint y,FXint w,FXint h,
  FXint pl,FXint pr,FXint pt,FXint pb)
     :
  FXImageFrame(p,img,opts,x,y,w,h,pl,pr,pt,pb)
{
  
}

// Change help text
void ImdebugImageView::setHelpText(const FXString& text){
  statusHelpText=text;
  if (!text.empty()) flags |= FLAG_HELP;
}


// Change tip text
void ImdebugImageView::setTipText(const FXString& text){
  tooltipText=text;
  if (!text.empty()) flags |= FLAG_TIP;
  
}


// We were asked about status text
long ImdebugImageView::onQueryHelp(FXObject* sender,FXSelector,void*){
  if(!statusHelpText.empty() && (flags&FLAG_HELP)){
    sender->handle(this,FXSEL(SEL_COMMAND,ID_SETSTRINGVALUE),&statusHelpText);
    return 1;
  }
  return 0;
}


// We were asked about tip text
long ImdebugImageView::onQueryTip(FXObject* sender,FXSelector,void*){
  if(!tooltipText.empty() && (flags&FLAG_TIP)){
    FXString resetString("");
    // Send the empty string first, because the tooltip is too smart for its
    // own good and recognizes when we sent the same string twice.
    // Unfortunately that means the position of the tooltip doesn't get
    // updated every time, and the tooltip can disappear when we still want
    // to have it displayed.  
    // Sending the empty string first fixes that and causes the tooltip to 
    // move smoothly around with the pointer too.
    sender->handle(this,FXSEL(SEL_COMMAND,ID_SETSTRINGVALUE),&resetString);
    sender->handle(this,FXSEL(SEL_COMMAND,ID_SETSTRINGVALUE),&tooltipText);
    return 1;
  }
  return 0;
}


