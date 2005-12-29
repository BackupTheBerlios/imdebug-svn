/*============================================================================
 *  imdebug.c
 *  Main routines for imdebug.dll
 *============================================================================
 * Copyright (c) 2005 William V. Baxter III
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software. If you use
 *    this software in a product, an acknowledgment in the product
 *    documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *============================================================================
 */

#define IMDBG_EXPORT __declspec(dllexport)
#include "imdebug.h"

#include <stdarg.h>
#include <math.h>
#include "imdebug_priv.h"
#include "imdebugarg.h"

// #define WIN32_LEAN_AND_MEAN
#include <windows.h>

// This macro evaluates to the number of elements in an array. 
#define chDIMOF(Array) (sizeof(Array) / sizeof(Array[0]))
/////////////////////////// Quick MessageBox Macro ////////////////////////////
#define chMB(s) {                                                    \
  TCHAR szTMP[128];                                              \
  GetModuleFileName(NULL, szTMP, chDIMOF(szTMP));                \
  MessageBox(GetActiveWindow(), s, szTMP, MB_OK);                \
}

// INTERNAL ERROR CODES 
#define BAD_WIN_VERSION 1
#define CREATE_PROC_FAILED 2
#define CREATE_FILE_MAPPING_FAILED 3

static HANDLE g_FileMap = NULL;
static LPVOID g_MapView = NULL;
static HANDLE g_ImageReadyEvent = NULL;
static HANDLE g_ImageReceivedEvent = NULL;
static BOOL   g_bFOXDisplayer = FALSE;

static PROCESS_INFORMATION g_childInfo;
static HWND g_displayHWND = NULL;

static BOOL g_shareDisplay = TRUE; // on by default

int WINAPI DllMain (HINSTANCE hInstance, DWORD fdwReason, PVOID pvReserved)
{


  switch (fdwReason)
  {
  case DLL_PROCESS_ATTACH :
    break;

  case DLL_THREAD_DETACH :
    
    break ;

  case DLL_THREAD_ATTACH :
    ZeroMemory(&g_childInfo, sizeof(g_childInfo));
    break;
    // When process terminates, free any remaining blocks

  case DLL_PROCESS_DETACH :
    if (g_MapView) {
      UnmapViewOfFile (g_MapView) ;
    }
    if (g_FileMap) {
      CloseHandle(g_FileMap);
    }
    if (g_childInfo.hThread)
      CloseHandle(g_childInfo.hThread);
    if (g_childInfo.hProcess)
      CloseHandle(g_childInfo.hProcess);
    break ;
  }

  return TRUE ;
}


static void loadRegistryPrefs()
{
  HKEY myKey;
  LONG ret;
  DWORD size;
  DWORD type;
  BOOL found = FALSE;

  
  // Look for new style key:
  do {
  
    ret = RegOpenKeyEx(HKEY_CURRENT_USER,
                       TEXT("Software\\"
                            IMDBG_REG_KEY_VENDOR "\\"
                            IMDBG_REG_KEY_APP "\\SETTINGS"),
                       0,
                       KEY_READ,
                       &myKey);
    if (ERROR_SUCCESS != ret) {
     break;
    }

    {
      BYTE str[128];
      size = 127;
      ret = RegQueryValueEx( myKey, TEXT("shareDisplay"),
                             NULL, &type, str, &size);
      if (ERROR_SUCCESS != ret || REG_SZ != type) {
        RegCloseKey(myKey);
        break;
      }
      g_shareDisplay = str[0]=='1'?TRUE:FALSE;
      found = TRUE;
    }
    RegCloseKey(myKey);
  } while(0);

  if (found) return;

  // Look for new style key:
  do {
  
    ret = RegOpenKeyEx(HKEY_CURRENT_USER,
                       TEXT("Software\\" IMDBG_REG_KEY_APP),
                       0,
                       KEY_READ,
                       &myKey);
    if (ERROR_SUCCESS != ret) {
     break;
    }

    {
      BOOL flag;
      size = sizeof(BOOL);
      ret = RegQueryValueEx( myKey, TEXT("shareDisplay"),
        NULL, &type, (BYTE*)&flag, &size);
      if (ERROR_SUCCESS != ret || REG_BINARY != type) {
        RegCloseKey(myKey);
        break;
      }
      g_shareDisplay = flag;
      found = TRUE;
    }
    RegCloseKey(myKey);
  } while (0);
}

static int createFileMapping()
{
  g_FileMap = CreateFileMapping((HANDLE) 0xFFFFFFFF, 
                                NULL, PAGE_READWRITE, 0, 
                                IMDBG_MAX_DATA,
                                __TEXT(IMDBG_MEMMAP_FILENAME));
  if (g_FileMap != NULL) {
    
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      //Fine just use the one that already exists.
      int _dummy=0;
    }
    if (g_MapView==NULL) 
    {
      // File mapping created successfully.
      
      // Map a view of the file 
      // into the address space.
      g_MapView = MapViewOfFile(g_FileMap,
                                 FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
      
      if ((BYTE *) g_MapView == NULL) 
      {
        // chMB(__TEXT("Can't map view of file."));
        return 2;
      }
      
      //((improps *)g_MapView)->MESSAGE = 0;
    }

  } else {
    // chMB(__TEXT("Can't create file mapping."));
    return 1;
  }

  return 0;  // everything ok
}

static int createImageReadyEvent()
{
  g_ImageReadyEvent = CreateEvent( 
    NULL, FALSE, FALSE, __TEXT(IMDBG_SIGNAL_NAME));
  if (!g_ImageReadyEvent)
  {
    return 1;
  }
  g_ImageReceivedEvent = CreateEvent( 
    NULL, FALSE, FALSE, __TEXT(IMDBG_SIGNAL_RECEIVED_NAME));
  if (!g_ImageReadyEvent)
  {
    return 1;
  }
  return 0;
}


static BOOL CALLBACK enumThreadWndProc(HWND hwnd, LPARAM lParam)
{
  UINT ret;
  TCHAR buf[100];
  ret = GetClassName(hwnd, buf, 100);
  if (!strcmp(buf, IMDBG_DISPLAY_WINDOW_CLASS)){
    g_displayHWND = hwnd;
    g_bFOXDisplayer = FALSE;
    return FALSE; // stop enumeration
  }
  else if(!strcmp(buf, "FXTopWindow")) {
    GetWindowText(hwnd,buf,100);
    if (strstr(buf, IMDBG_DISPLAY_WINDOW_TITLE)) {
      g_displayHWND = hwnd;
      g_bFOXDisplayer = TRUE;
      return FALSE; // stop enumeration
    }
  }
  return TRUE;
}

static void findChildDisplayerWindow()
{
  // Looks for a child process that's a displayer
  int ret;
  ret = WaitForInputIdle(g_childInfo.hProcess, 20000);
  if (ret!=0) {
    chMB(__TEXT("Can't find window of displayer process."));
    g_displayHWND = NULL;
  }
  g_displayHWND = NULL;
  EnumThreadWindows( g_childInfo.dwThreadId, enumThreadWndProc, 0 );

}

static int startDisplayer()
{
  BOOL ret;
  STARTUPINFO si;
  PROCESS_INFORMATION *pi = &g_childInfo;
  static BOOL firsttime = TRUE;

  // Make sure Windows version is good enough
  if (firsttime)
  {
    OSVERSIONINFO osver;

    firsttime = FALSE;
    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    ret = GetVersionEx(&osver);
    if (osver.dwMajorVersion < 4) {
      return BAD_WIN_VERSION;
    }
  }

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  if (!CreateProcess(
        NULL,               // app name
        "ImDbgDisplay.exe", // command line
        NULL,               // proc sec attrib
        NULL,               // thrd sec attrib
        FALSE,              // inherit handles
        0,                  // create flags
        NULL,               // environment
        NULL,               // cur dir
        &si,                // start info
        pi                  // proc info
        ))
  {
    return CREATE_PROC_FAILED;
  }
  else {
    findChildDisplayerWindow();
  }

  return 0;
}

BOOL CALLBACK FindFoxEnumProc(HWND hwnd, LPARAM lParam)
{
  char name[256];
  GetClassName(hwnd,name,255);
  if (strcmp(name, "FXTopWindow")) return TRUE;
  GetWindowText(hwnd,name,255);
  if (!strstr(name, IMDBG_DISPLAY_WINDOW_TITLE)) return TRUE;
  *((HWND*)lParam) = hwnd;
  g_bFOXDisplayer = TRUE;
  return FALSE;
}


static HWND FindFoxWindow()
{
  HWND win=0;
  EnumWindows( FindFoxEnumProc, (LPARAM)&win);
  return win;
}

static int ensureDisplayerRunning()
{
  static launchFailed = FALSE;

  int ret;

  
  // create a mapping file if it doesn't exist yet
  if (!g_ImageReadyEvent)
  {
    ret = createImageReadyEvent();
  }
  if (!g_FileMap || !g_MapView) {
    ret = createFileMapping();
    if (ret != 0) {
      return CREATE_FILE_MAPPING_FAILED; // some error encountered
    }
    // also load registry prefs (no file mapping is a good indicator)
    loadRegistryPrefs();
  }

  // Make sure the display server process is up and running
  // The methodology here makes it so that each app using imdebug
  // will get it's own separate output window.
  {
    BOOL alive = FALSE;
    if (g_childInfo.hProcess != NULL) {
      BOOL bret;
      DWORD code;
      bret = GetExitCodeProcess(g_childInfo.hProcess, &code);
      if (code == STILL_ACTIVE) {
        alive = TRUE;
      }
    }
    if (!alive && g_shareDisplay ) {
      // find window
      char  szClassName[] = IMDBG_DISPLAY_WINDOW_CLASS ;
      HWND  prevAppWin = 0;
      prevAppWin = FindWindow( szClassName, NULL );
      if (prevAppWin) {
        alive = TRUE;
        g_bFOXDisplayer = FALSE;
        g_displayHWND = prevAppWin;
      }
      else {
        prevAppWin = FindFoxWindow();
        if (prevAppWin) {
          alive = TRUE;
          g_bFOXDisplayer = TRUE;
          g_displayHWND = prevAppWin;
        }
      }

    }
    if (!alive && !launchFailed) {
      do {
        ret = startDisplayer();
        if (ret == CREATE_PROC_FAILED) {
          ret = MessageBox (
            NULL,
            TEXT ("Unable to launch viewer application, \"imdbgdisplay.exe\"\n")
            TEXT ("(Please check that it is either on your PATH,\n")
            TEXT ("or in the same directory as your program.)"),
            "Image Debugger Error", MB_ICONEXCLAMATION | MB_RETRYCANCEL);
          launchFailed = TRUE;
        } 
      } while (ret == IDRETRY);
      if (ret != 0) {
        return ret;
      }
    }
  }

  return 0;
}

void initImprops(ImProps * p)
{
  int i;
  ZeroMemory(p,sizeof(ImProps));
  p->VERSION = IMDBG_PROTOCOL_VERSION;
  p->nchan = 3;
  for (i=0; i<IMDBG_MAX_CHANNELS; i++) {
    p->bpc[i] = 8;
    p->type[i] = IMDBG_UINT;
    p->bitoffset[i*8];
  }
  for (i=0; i<4; i++) {
    p->scale[i] = 1.0f;
    p->bias[i] = 0.0f;
  }

  p->chanmap[0] = 0;
  p->chanmap[1] = 1;
  p->chanmap[2] = 2;
  p->chanmap[3] =-1;

  p->colstride = 0;
  p->rowstride = 0;
  p->colgap = 0;
  p->rowgap = 0;
  p->width = 0;
  p->height = 0;
  p->title[0] = 0;
}

//============================================================================
// MAIN EXTERNAL DLL ENTRYPOINT
//============================================================================

void CALLBACK imdebug(const char* format, ...)
{
  BYTE * imp = 0;
  ImProps spec;
  va_list args;

  initImprops(&spec);

  va_start(args, format);
  processArgs(&spec, &imp, format, args);
  va_end(args);

  // start up client if not started yet
  if ( ensureDisplayerRunning() != 0 )
    return; // launch failed, so bail

  // put image specified into file mapping
  {
    int i;
    int pixsize;
    int imsize;
    int nbits = 0;
    BYTE *imtarget;

    for (i=0;i<spec.nchan; i++) {
      spec.bitoffset[i] = nbits;
      nbits += spec.bpc[i];
    }
    spec.colstride = nbits;
    spec.rowstride = nbits * spec.width;

    if (!imp) {
      // Image is the NULL pointer
      // use nchan < 0 to signal null pointer
      spec.nchan = -1;
    }

    CopyMemory(g_MapView, &spec, sizeof(spec));

    pixsize = (int)ceil(nbits / 8.0f);
    imsize = (int)ceil((nbits * spec.width*spec.height)/8.0f);

    imtarget = (BYTE*)g_MapView;
    imtarget += sizeof(spec);
    if (sizeof(spec)+imsize > IMDBG_MAX_DATA || !imp) {
      ZeroMemory(imtarget, 1024);
    }
    else {
      CopyMemory(imtarget, imp, imsize);
    }

  }
  // let our chum know it's ready
  if (g_bFOXDisplayer) {
    // Signal image ready with event, and wait for event in
    // return to tell us it was processed.
    // (FOX GUI doesn't know how to respond to WM_APP sent with
    // SendMessage.)
    if (g_ImageReadyEvent && g_ImageReceivedEvent) {
      SetEvent(g_ImageReadyEvent);
      WaitForSingleObject( g_ImageReceivedEvent, 5000 );
    }
  }
  else {
    if (g_displayHWND) {
      SendMessage(g_displayHWND, WM_APP, APPM_IMAGE_READY, 0);
    }
  }
}
