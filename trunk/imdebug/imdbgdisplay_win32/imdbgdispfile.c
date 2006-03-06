/*----------------------------------------------------
  imdbgdispfile.c -- File Functions for Image Debugger
  ----------------------------------------------------

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
  -----------------------------------------------------------*/

#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include <stdio.h>

static OPENFILENAME ofn ;

void FileInitialize (HWND hwnd)
{
  static char szFilter[] = "Matlab Files (*.M)\0*.m\0"  \
    "All Files (*.*)\0*.*\0\0" ;

  ofn.lStructSize       = sizeof (OPENFILENAME) ;
  ofn.hwndOwner         = hwnd ;
  ofn.hInstance         = NULL ;
  ofn.lpstrFilter       = szFilter ;
  ofn.lpstrCustomFilter = NULL ;
  ofn.nMaxCustFilter    = 0 ;
  ofn.nFilterIndex      = 0 ;
  ofn.lpstrFile         = NULL ;          // Set in Open and Close functions
  ofn.nMaxFile          = _MAX_PATH ;
  ofn.lpstrFileTitle    = NULL ;          // Set in Open and Close functions
  ofn.nMaxFileTitle     = _MAX_FNAME + _MAX_EXT ;
  ofn.lpstrInitialDir   = NULL ;
  ofn.lpstrTitle        = NULL ;
  ofn.Flags             = 0 ;             // Set in Open and Close functions
  ofn.nFileOffset       = 0 ;
  ofn.nFileExtension    = 0 ;
  ofn.lpstrDefExt       = "m" ;
  ofn.lCustData         = 0L ;
  ofn.lpfnHook          = NULL ;
  ofn.lpTemplateName    = NULL ;
}

BOOL FileSaveDlg (HWND hwnd, PSTR pstrFileName, PSTR pstrTitleName)
{
  ofn.hwndOwner         = hwnd ;
  ofn.lpstrFile         = pstrFileName ;
  ofn.lpstrFileTitle    = pstrTitleName ;
  ofn.Flags             = OFN_OVERWRITEPROMPT ;

  return GetSaveFileName (&ofn) ;
}


BOOL FileWrite (HWND hwndEdit, PSTR pstrFileName)
{
  FILE  *file ;
  TCHAR  buf[_MAX_PATH];
  TCHAR *name;
  TCHAR *p;

  if (NULL == (file = fopen (pstrFileName, "w")))
    return FALSE ;
  
  strcpy(buf, pstrFileName);
  if ( (p = strrchr(buf, '.')) != 0 ) {
    *p = 0;
  }
  if ( (name = strrchr(buf, '\\')) != 0 ) {
    name++;
  }
  p = name;

  while ( (p = strchr(p, ' ')) != 0) {
    *p++ = '_';
  }
  while ( (p = strchr(p, '.')) != 0) {
    *p++ = '_';
  }

  fprintf(file, "%s = [ ...\n", name);
  fprintf(file, "%% This is not actually implemented yet\n");
  fprintf(file, "];\n");

  fclose (file) ;

  return TRUE ;
}
