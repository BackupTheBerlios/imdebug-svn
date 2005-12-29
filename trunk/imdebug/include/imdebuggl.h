/*============================================================================
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
 *
 *==========================================================================*/

#ifndef __IMDEBUGGL_H__
#define __IMDEBUGGL_H__

#include <imdebug.h>
#include <gl/gl.h>
#include <string>

int format2components(GLenum format);
string format2string(GLenum format);


//-----------------------------------------------------------------------------
// Function     	  : imdebugTexImage
// Description	    : 
//-----------------------------------------------------------------------------
/**
 * @fn imdebugTexImage(GLenum target, GLuint texture, GLint level, GLenum format, char *argstring = NULL)
 * @brief Gets a texture level from OpenGL, and displays it using imdebug.
 * 
 * If argstring is non-NULL, then it is prepended to the string " w=%d h=%d %p"
 * and passed as the imdebug format string.  Otherwise, the correct format
 * ("rgb", "lum", etc.) is determined from the @a format argument, and 
 * prepended to the same string and used as the imdebug format string.
 */ 
void imdebugTexImage(GLenum target, GLuint texture, GLenum format = GL_RGB, 
                     GLint level = 0, char *argstring = NULL)
{  
  int w, h;
  int prevTexBind;
  glGetIntegerv(target, &prevTexBind);

  glBindTexture(target, texture);
  glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &w);
  glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &h);
  GLubyte *p = new GLubyte[w * h * format2components(format)];
  
  glGetTexImage(target, level, format, GL_UNSIGNED_BYTE, p);
 
  string s = (NULL == argstring) ? format2string(format) : argstring;
  imdebug(s.append(" w=%d h=%d %p").c_str(), w, h, p);
  
  delete [] p;

  glBindTexture(target, prevTexBind);
}

//-----------------------------------------------------------------------------
// Function     	  : imdebugTexImagef
// Description	    : 
//-----------------------------------------------------------------------------
/**
 * @fn imdebugTexImage(GLenum target, GLuint texture, GLint level, GLenum format, char *argstring = NULL)
 * @brief Gets a texture level from OpenGL, and displays it as floats using imdebug.
 * 
 * If argstring is non-NULL, then it is prepended to the string " w=%d h=%d %p"
 * and passed as the imdebug format string.  Otherwise, the correct format
 * ("rgb", "lum", etc.) is determined from the @a format argument, and 
 * prepended to the same string and used as the imdebug format string.
 */ 
void imdebugTexImagef(GLenum target, GLuint texture, GLenum format = GL_RGB, 
                      GLint level = 0, char *argstring = NULL)
{  
  int w, h;
  int prevTexBind;
  glGetIntegerv(target, &prevTexBind);

  glBindTexture(target, texture);
  glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &w);
  glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &h);
  float *p = new float[w * h * format2components(format)];
  
  glGetTexImage(target, level, format, GL_FLOAT, p);
 
  string s = (NULL == argstring) ? format2string(format) : argstring;
  imdebug(s.append(" b=32f w=%d h=%d %p").c_str(), w, h, p);
  
  delete [] p;

  glBindTexture(target, prevTexBind);
}


//-----------------------------------------------------------------------------
// Function     	  : imdebugPixels
// Description	    : 
//-----------------------------------------------------------------------------
/**
 * @fn imdebugPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, char *argstring = NULL)
 * @brief performs a glReadPixels(), and then displays the results in imdebug.
 * 
 * If argstring is non-NULL, then it is prepended to the string " w=%d h=%d %p"
 * and passed as the imdebug format string.  Otherwise, the correct format
 * ("rgb", "lum", etc.) is determined from the @a format argument, and 
 * prepended to the same string and used as the imdebug format string.
 */ 
void imdebugPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
                   GLenum format = GL_RGB, char *argstring = NULL)
{
  GLubyte *p = new GLubyte[width * height * format2components(format)];
  glReadPixels(x, y, width, height, format, GL_UNSIGNED_BYTE, p);
 
  string s = (NULL == argstring) ? format2string(format) : argstring;
  imdebug(s.append(" w=%d h=%d %p").c_str(), width, height, p);

    
  delete [] p;
}

//-----------------------------------------------------------------------------
// Function     	  : imdebugPixels
// Description	    : 
//-----------------------------------------------------------------------------
/**
 * @fn imdebugPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, char *argstring = NULL)
 * @brief performs a glReadPixels(), and then displays the results in imdebug.
 * 
 * If argstring is non-NULL, then it is prepended to the string " w=%d h=%d %p"
 * and passed as the imdebug format string.  Otherwise, the correct format
 * ("rgb", "lum", etc.) is determined from the @a format argument, and 
 * prepended to the same string and used as the imdebug format string.
 */ 
void imdebugPixelsf(GLint x, GLint y, GLsizei width, GLsizei height, 
                    GLenum format = GL_RGB, char *argstring = NULL)
{
  float *p = new float[width * height * format2components(format)];
  glReadPixels(x, y, width, height, format, GL_FLOAT, p);
 
  string s = (NULL == argstring) ? format2string(format) : argstring;
  imdebug(s.append(" b=32f w=%d h=%d %p").c_str(), width, height, p);
    
  delete [] p;
}

int format2components(GLenum format)
{
  switch(format) 
  {
  case GL_LUMINANCE:
  case GL_INTENSITY:
  case GL_ALPHA:
    return 1;
  	break;
  case GL_LUMINANCE_ALPHA:
    return 2;
  	break;
  case GL_RGB:
  case GL_BGR:
    return 3;
    break;
  case GL_RGBA:
  case GL_BGRA:
  case GL_ABGR_EXT:
    return 4;
    break;
  default:
    return 4;
    break;
  }
}

string format2string(GLenum format)
{
  switch(format) 
  {
  case GL_LUMINANCE:
  case GL_INTENSITY:
  case GL_ALPHA:
    return "lum";
  	break;
  case GL_LUMINANCE_ALPHA:
    return "luma";
  	break;
  case GL_RGB:
  case GL_BGR:
    return "rgb";
    break;
  case GL_RGBA:
  case GL_BGRA:
  case GL_ABGR_EXT:
    return "rgba";
    break;
  default:
    return "rgb";
    break;
  }
}
#endif //__IMDEBUGGL_H__
