/*---------------------------------------------------------------------------
     File name    : win32event.cpp
     Author       : William Baxter
  ---------------------------------------------------------------------------
  $Id:$
  ---------------------------------------------------------------------------
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
#include "win32event.h"

#include <windows.h>
#include <limits.h>


void *CreateImageReadySignalEvent(const char*evname)
{
  return CreateEvent(
    NULL,  //LPSECURITY_ATTRIBUTES lpEventAttributes,
    FALSE, //bManualReset,
    FALSE, // bInitialState,
    TEXT(evname) 
    );
}

void *CreateImageReadySemaphore(const char*evname, long initialVal)
{
  return CreateSemaphore(
    NULL,       //LPSECURITY_ATTRIBUTES lpEventAttributes,
    initialVal, //initial value,
    LONG_MAX,    //maximum value
    TEXT(evname) 
    );
}
