/*---------------------------------------------------------------------------
     File name    : memorymappedfile.h
     Author       : William Baxter
     Created      : 7/31/2003
  
     Description  : Platform independent wrapper for memory mapped file.
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
#ifndef _MEMORYMAPPEDFILE_H
#define _MEMORYMAPPEDFILE_H

class _MemoryMappedFilePriv;

class MemoryMappedFile

{
public:
  MemoryMappedFile(const char* name, unsigned int create_flags);
  ~MemoryMappedFile();

  void *data();

  enum { OPEN_READ=0x1, OPEN_WRITE=0x2, OPEN_READWRITE=0x3 };
protected:
private:
  _MemoryMappedFilePriv *priv;
};

#endif // _MEMORYMAPPEDFILE_H
