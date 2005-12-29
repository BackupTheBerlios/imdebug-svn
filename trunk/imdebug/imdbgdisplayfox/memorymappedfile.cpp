/*---------------------------------------------------------------------------
     File name    : memorymappedfile.h
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
#include "memorymappedfile.h"

#define MEMMAPFILE_MAX_SIZE  (16 * 2048 * 2048 + 1024)

#if defined(_WIN32)
////////////// BEGIN WIN32 IMPLEMENTATION //////////////////////
#include <windows.h>
#include <stdio.h>
class _MemoryMappedFilePriv
{
private:
  _MemoryMappedFilePriv() : pMapView(0),pFileMap(0) {}
  LPVOID pMapView;
  HANDLE pFileMap;

  int create(const char *name, unsigned int flags) {
    pFileMap = CreateFileMapping(
      (HANDLE) 0xFFFFFFFF, 
      NULL, PAGE_READWRITE, 0, 
      MEMMAPFILE_MAX_SIZE,
      name
      );
    if (pFileMap != NULL) {
      // File mapping created successfully (or already existed).
      if (pMapView == NULL)
      {
        // Map a (read-only) view of the file into our address space.
        unsigned int f = 0;
        if (flags&MemoryMappedFile::OPEN_READ)  f |= FILE_MAP_READ;
        if (flags&MemoryMappedFile::OPEN_WRITE) f |= FILE_MAP_WRITE;

        pMapView = MapViewOfFile(pFileMap, f, 0, 0, 0);
          
          if ((BYTE *) pMapView == NULL) 
          {
            fprintf(stderr,"Can't map view of file.");
            return 0;
          }
      }
      
    } else {
      fprintf(stderr,"Can't create file mapping.");
      return 0;
    }
    return 1;
  }
  void destroy() {
    if (pMapView) {  
      UnmapViewOfFile (pMapView) ;  
      pMapView = NULL;
    }
    if (pFileMap) {
      CloseHandle(pFileMap);
      pFileMap = NULL;
    }
  }
  void *data() {
    return pMapView;
  }
  friend class MemoryMappedFile;
};

////////////// END WIN32 IMPLEMENTATION //////////////////////
#else
#error Platform not supported!  Please port me!
#endif


MemoryMappedFile::MemoryMappedFile(const char* name, unsigned int create_flags)
{
   priv = new _MemoryMappedFilePriv();
   int ret = priv->create(name, create_flags);
}

MemoryMappedFile::~MemoryMappedFile()
{
  priv->destroy();
  delete priv;
  priv=0;
}


void *MemoryMappedFile::data()
{
  return priv->data();
}
