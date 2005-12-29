/*
 *============================================================================
 * imdebugarg.c
 *   Argument parsing for the format string passed to imdebug()
 *============================================================================
 * Copyright (c) 2002-2005 William V. Baxter III
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
#include "imdebug_priv.h"
#include "imdebugarg.h"
#include <windows.h>
#include <float.h>
#include <stdio.h>

enum {
  TOK_WIDTH,    // w
  TOK_HEIGHT,   // h
  TOK_TITLE,    // t
  TOK_BPC,      // b
  TOK_COLSKIP,  // rs
  TOK_ROWSKIP,  // cs
  TOK_NCHAN,    // ?? not implemented 
  TOK_CHANMASK, // ?? not implemented

  TOK_INT_LIT,       // plain int
  TOK_FLOAT_LIT,     // plain float
  TOK_STR_LIT,       // 'str'  for future: \"str\", or otherwise unmatched text
  TOK_TYPED_INT_LIT, // like 32f

  TOK_FMT,      // rbg rgb lum, #5 etc.
  TOK_OUTFMT,   // rg_b rrr, #130 etc

  TOK_FLOATVAR, // %f
  TOK_INTVAR,   // %d
  TOK_STRVAR,   // %s
  TOK_PTRVAR,   // %p

  TOK_AUTO,     // the literal 'auto'

  TOK_EQUALS,   // =
  TOK_COMMA,    // ,
  TOK_PLUS,     // +
  TOK_MINUS,    // -
  TOK_TIMES,    // *
  TOK_DIVIDE,   // /
  TOK_SHARP,    // #

  TOK_ERROR,    // lexical error
  TOK_BEGIN,    // initial state
  TOK_EOF       // end of format string
};

static int currentToken=TOK_BEGIN;
static char inputBuf[255];
static char tokenBuf[100];
static int tokenVal;
static float tokenValf;

static void strcpytolower(char *dst, const char *src)
{
  while( *dst++ = tolower(*src++) ) 
    ;
}

static char* skipWhiteSpace(char *ptr)
{
  while(isspace(*ptr)) ptr++;
  return ptr;
}

static int nextToken(char **pptr)
{
  // char rgbchars[]="rgbalum";
  char *p = *pptr;
  char *b = tokenBuf;
  
  p = skipWhiteSpace(p);
  
  if (*p == 0) {
    currentToken=TOK_EOF;
  }
  else if (!strncmp(p, "auto", 4)) {
    while ( (*b++=*p++)!='o' ) ; // copy 'auto'
    currentToken=TOK_AUTO;
  }
  else if (isalpha(*p) || *p=='_') {
    int len;
    BOOL isrgba = TRUE;
    BOOL isrgba_ = TRUE;
    // Alphabetic token
    
    while (isalpha(*p)||*p=='_') {
      if (*p!='r'&&*p!='g'&&*p!='b'&&*p!='a') {
        isrgba = FALSE;
        if (*p!='_') 
          isrgba_ = FALSE;
      }
      *b++=*p++;
    }
    *b = 0;
    len = b-tokenBuf;

    if (!isrgba && isrgba_) {
      currentToken = TOK_OUTFMT;
    }
    else if (isrgba) {
      if (!strcmp("b",tokenBuf)) {
        p = skipWhiteSpace(p);
        if (*p == '=')
          currentToken = TOK_BPC;
        else 
          currentToken = TOK_FMT;
      }
      else if (currentToken == TOK_EQUALS)
        currentToken = TOK_OUTFMT;
      else 
        currentToken = TOK_FMT;
    }
    // it's alphabetic but not all 'rgba' letters
    else if (!strcmp("lum", tokenBuf) || !strcmp("l", tokenBuf) ) {
      currentToken = (currentToken==TOK_EQUALS)?TOK_OUTFMT:TOK_FMT;
    }
    else if (!strcmp("luma", tokenBuf) || !strcmp("la", tokenBuf) ) {
      currentToken = (currentToken==TOK_EQUALS)?TOK_OUTFMT:TOK_FMT;
    }
    else if (!strcmp("alum", tokenBuf) || !strcmp("al", tokenBuf) ) {
      currentToken = (currentToken==TOK_EQUALS)?TOK_OUTFMT:TOK_FMT;
    }
    else if (!strcmp("w", tokenBuf)) {
      currentToken = TOK_WIDTH;
    }
    else if (!strcmp("h", tokenBuf)) {
      currentToken = TOK_HEIGHT;
    }
    else if (!strcmp("t", tokenBuf)) {
      currentToken = TOK_TITLE;
    }
    else if (!strcmp("rs", tokenBuf)) {
      currentToken = TOK_ROWSKIP;
    }
    else if (!strcmp("cs", tokenBuf)) {
      currentToken = TOK_COLSKIP;
    }
    else {
      currentToken = TOK_ERROR;
    }
  }
  else if (*p == '#') {
    *b++ = *p++;
    if (currentToken==TOK_EQUALS) {
      currentToken = TOK_OUTFMT;
      while(isdigit(*p)||(*p>='a'&&*p<='f')) *b++ = *p++;
    }
    else {
      currentToken = TOK_FMT;
      while(isdigit(*p)) *b++ = *p++;
    }
  }
  else if (*p == '%') {
    *b++ = *p++;
    while (isalpha(*p)) *b++=*p++;
    *b = 0;
    if (!strcmp("%d", tokenBuf)) {
      currentToken= TOK_INTVAR;
    }
    else if (!strcmp("%f", tokenBuf)) {
      currentToken= TOK_FLOATVAR;
    }
    else if (!strcmp("%p", tokenBuf)) {
      currentToken= TOK_PTRVAR;
    }
    else if (!strcmp("%s", tokenBuf)) {
      currentToken= TOK_STRVAR;
    }
    else {
      currentToken= TOK_ERROR;
    }
  }
  else if (*p == '=') {
    *b++ = *p++;
    currentToken=TOK_EQUALS;
  }
  else if (*p >= '0' && *p <= '9') {
    tokenVal = atoi(p);
    while (isdigit(*p)) *b++=*p++;
    if (*p == 'f') {// currently the only allowed type
      currentToken=TOK_TYPED_INT_LIT;
      *b++=*p++;
    }
    else if (*p == '.') { // oh a float!
      *b++=*p++;
      // this is a little loose...
      while (isdigit(*p)||*p=='e') *b++=*p++;
      *b=0;
      tokenValf = (float)atof(tokenBuf);
      currentToken = TOK_FLOAT_LIT;
    }
    else 
      currentToken=TOK_INT_LIT;
  }
  else if (*p == '.') {
    tokenValf = (float)atof(p);
    *b++ = *p++;
    // this is a little loose...
    while (isdigit(*p)||*p=='e') *b++=*p++;
    currentToken = TOK_FLOAT_LIT;
  }
  else if (*p == '\'') {
    int nextislit = 0;
    p++;
    while (*p!='\'' || nextislit) {
      if (nextislit) { *b++ = *p; nextislit = 0; }
      else if (*p == '\\') { nextislit = 1; }
      else { *b++ = *p; }
      p++;
    }
    p++;
    currentToken = TOK_STR_LIT;
  }
  else if (*p == '+') { *b++=*p++; currentToken = TOK_PLUS; }
  else if (*p == '-') { *b++=*p++; currentToken = TOK_MINUS; }
  else if (*p == '*') { *b++=*p++; currentToken = TOK_TIMES; }
  else if (*p == '/') { *b++=*p++; currentToken = TOK_DIVIDE; }
  else if (*p == ',') { *b++=*p++; currentToken = TOK_COMMA; }

  else {
    *b++ = *p++;
    currentToken=TOK_ERROR;
  }
  // NULL-terminate tokenBuf
  *b = 0;


  *pptr = p;
  return currentToken;
}

static BOOL parseIntExpr(int *out, char **fmt, va_list *args)
{
  nextToken( fmt );
  switch( currentToken )
  {
  case TOK_INT_LIT:
    *out = tokenVal;
    break;
  case TOK_INTVAR:
  {
    int i = va_arg(*args, int);
    *out = i;
  }
  break;
  case TOK_FLOATVAR:
  {
    float f = va_arg(*args, float);
    *out = (int)f;
  }
  break;
  case TOK_STRVAR:
  {
    char *s = va_arg(*args, char*);
    *out = atoi(s);
  }
  break;
  default:
    // ERROR!
    return FALSE;
  }
  nextToken( fmt );
  return TRUE;
}


static BOOL parseStringExpr(char *out, char **fmt, va_list *args)
{
  int MAX_TITLE = MAX_TITLE_LENGTH; // from imdebug_priv.h
  nextToken( fmt );
  switch( currentToken )
  {
  case TOK_STR_LIT:
    _snprintf(out, MAX_TITLE, "%s", tokenBuf);
    break;
  case TOK_INT_LIT:
    _snprintf(out, MAX_TITLE, "%d", tokenVal);
    break;
  case TOK_INTVAR:
  {
    int i = va_arg(*args, int);
    _snprintf(out, MAX_TITLE, "%d", i);
  }
  break;
  case TOK_FLOATVAR:
  {
    float f = va_arg(*args, float);
    _snprintf(out, MAX_TITLE, "%f", f);
  }
  break;
  case TOK_STRVAR:
  {
    char *s = va_arg(*args, char*);
    _snprintf(out, MAX_TITLE, "%s", s);
  }
  break;
  default:
    // ERROR!
    return FALSE;
  }
  nextToken( fmt );
  return TRUE;
}


static BOOL parseFloatExpr(float *out, char **fmt, va_list *args)
{
//  nextToken( fmt ); caller does before parseFloatExpr now.
  switch( currentToken )
  {
  case TOK_FLOAT_LIT:
    *out = tokenValf;
    break;
  case TOK_INT_LIT:
    *out = (float)tokenVal;
    break;
  case TOK_INTVAR:
  {
    int i = va_arg(*args, int);
    *out = (float)i;
  }
  break;
  case TOK_FLOATVAR:
  {
    float f = va_arg(*args, float);
    *out = f;
  }
  break;
  case TOK_STRVAR:
  {
    char *s = va_arg(*args, char*);
    *out = (float)atof(s);
  }
  break;
  default:
    // ERROR!
    return FALSE;
  }
  nextToken( fmt );
  return TRUE;
}


static BOOL parseTypedIntExpr(int *out, char *type, char **fmt, va_list *args)
{
  nextToken( fmt );
  switch( currentToken )
  {
  case TOK_INT_LIT:
    *out = tokenVal;
    break;
  case TOK_INTVAR:
  {
    int i = va_arg(*args, int);
    *out = i;
  }
  break;
  case TOK_FLOATVAR:
  {
    float f = va_arg(*args, float);
    *out = (int)f;
  }
  break;
  case TOK_TYPED_INT_LIT:
  {
    char *p = tokenBuf;
    *out = tokenVal;
    while (isdigit(*p)) p++;
    if (*p=='f') {
      *type = IMDBG_FLOAT;
    }
    break;
  }
  case TOK_STRVAR:
  {
    char *p = va_arg(*args, char*);
    *out = atoi(p);
    while (isdigit(*p)) p++;
    if (*p=='f') {
      *type = IMDBG_FLOAT;
    }
  }
  break;
  default:
    // ERROR!
    return FALSE;
  }
  nextToken( fmt );
  return TRUE;
}

static void parseIntSetExpr(int *out, char **fmt, va_list *args)
{
  if (nextToken( fmt ) != TOK_EQUALS) {
    // ERROR!  TRY TO RECOVER
    return;
  }
  parseIntExpr(out, fmt, args);
}

static void parseStringSetExpr(char *out, char **fmt, va_list *args)
{
  if (nextToken( fmt ) != TOK_EQUALS) {
    // ERROR!  TRY TO RECOVER
    return;
  }
  parseStringExpr(out, fmt, args);
}

static BOOL parseScaleBiasValue(float *out, char **fmt, va_list *args, BOOL *isAuto)

{
  int op = currentToken;
  *isAuto = FALSE;
  nextToken(fmt);
  if (op == TOK_TIMES && currentToken == TOK_AUTO) {
    *out = IMDBG_AUTOSCALE;
    *isAuto=TRUE;
    nextToken(fmt);
  }
  else if (!parseFloatExpr(out, fmt, args)) {
    return FALSE;
  }
  return TRUE;
}

static int parseBPCArrayExpr(int *outsizes,
                             char *outtypes,
                             char **fmt, va_list *args)
{
  int listSize = 0;
  if (nextToken( fmt ) != TOK_EQUALS) {
    // ERROR!  TRY TO RECOVER
    return 0;
  }
  // nextToken(fmt);
  do {
    if (!parseTypedIntExpr( outsizes, outtypes, fmt, args )) 
      return listSize;
    outsizes++;
    outtypes++;
    listSize++;
  } while( currentToken == TOK_COMMA ); 

  return listSize;
}

static void parseInputFormatDecl(ImProps* props, char* fmt)
{
  // should be something like rgb or rbga or lum or luma, or #5
  if (!strcmp("lum",fmt)) {
    props->nchan = 1;
    props->chanmap[0] = 0;
    props->chanmap[1] = 0;
    props->chanmap[2] = 0;
    props->chanmap[3] = -1;
  }
  else if (!strcmp("luma",fmt)) {
    props->nchan = 2;
    props->chanmap[0] = 0;
    props->chanmap[1] = 0;
    props->chanmap[2] = 0;
    props->chanmap[3] = 1;
  }
  else if (!strcmp("alum",fmt)) {
    props->nchan = 2;
    props->chanmap[0] = 1;
    props->chanmap[1] = 1;
    props->chanmap[2] = 1;
    props->chanmap[3] = 0;
  }
  else if (*fmt == '#') {
    props->nchan = atoi(fmt+1);
    if (props->nchan == 1) {
      // treat as lum
      props->chanmap[0] = 0;
      props->chanmap[1] = 0;
      props->chanmap[2] = 0;
      props->chanmap[3] = -1;
    }
    else if (props->nchan == 2) {
      // treat as r+g
      props->chanmap[0] = 0;
      props->chanmap[1] = 1;
      props->chanmap[2] = -1;
      props->chanmap[3] = -1;
    }
    else if (props->nchan >=3 ) {
      // treat as rgb
      props->chanmap[0] = 0;
      props->chanmap[1] = 1;
      props->chanmap[2] = 2;
      props->chanmap[3] = -1;
    }
  }
  else {
    int inchan;
    props->chanmap[0] = -1;
    props->chanmap[1] = -1;
    props->chanmap[2] = -1;
    props->chanmap[3] = -1;

    props->nchan = strlen(fmt);
    for (inchan=0; inchan<props->nchan; inchan++) {
      switch(fmt[inchan]) {
      case 'r':
        props->chanmap[0] = inchan;  break;
      case 'g':
        props->chanmap[1] = inchan;  break;
      case 'b':
        props->chanmap[2] = inchan;  break;
      case 'a':
        props->chanmap[3] = inchan;  break;
      }
    }
  }
}

static void parseOutputFormatDecl(ImProps* props, char *lhs, char *rhs)
{
  // this is an extraction/swizzling expresion like 
  //      rgba=r__a
  //      luma=r__a
  //     

  // 'normalized' channel specifiers
  char lhsn[4] = { -1,-1,-1,-1 };
  char rhsn[4] = { -2,-2,-2,-2 }; // -2==novalue; -1=='_' specified
  int i;

  // get the lhsn (encodes destination values)
  // lhsn[0] = 2 means that the red value (0) is determined by the 2nd
  //           character of the rhs.
  {
    int inlen;
    inlen = strlen(lhs);
    // LHS should be something like rgb or rbga or lum or luma
    //      turn it into            012 or 01234   000 or 0001
    if (!strcmp("lum",lhs) || !strcmp("l",lhs) ) {
      // lum = x  expr
      lhsn[0] = 0; // r comes from rhs spec 0
      lhsn[1] = 1; // g comes from rhs spec 0
      lhsn[2] = 2; // b comes from rhs spec 0
    }
    else if (!strcmp("luma",lhs) || !strcmp("la",lhs) ) {
      lhsn[0] = 0;
      lhsn[1] = 1;
      lhsn[2] = 2;
      lhsn[3] = 3;
    }
    else if (!strcmp("alum",lhs) || !strcmp("al",lhs) ) {
      lhsn[0] = 1;
      lhsn[1] = 1;
      lhsn[2] = 1;
      lhsn[3] = 0;
    }
    else {
      int outchan;
      for (outchan=0; outchan<inlen; outchan++)
      {
        switch(lhs[outchan]) 
        {
        case 'r':
          lhsn[0] = outchan;  break;
        case 'g':
          lhsn[1] = outchan;  break;
        case 'b':
          lhsn[2] = outchan;  break;
        case 'a':
          lhsn[3] = outchan;  break;
        default:
          // ERROR!
          break;
        }
      }
    }
  }

  // get the rhsn (encodes source values)
  // rhsn[0] = 2 means that the component specified by lhsn[0]
  //           comes from the blue (2) value
  //           of the current channel map;
  // rhsn[0] = -1 means that the component specified by lhsn[0]
  //           is not set (will have zero intensity)
  if (*rhs=='#') {
    int i;
    int inlen;
    // numerical specifiers... so the mapping is simple to dermine
    //  rgb=#421  means: r comes from channel 4
    //                   g comes from channel 2
    //                   b comes from channel 1
    rhs++;
    inlen = strlen(rhs);
    for (i=0; i<inlen; i++) {
      int ord;
      if (rhs[i] >= 'a' && rhs[i] <= 'f') {
        ord = rhs[i]-'a'+10;
      }
      else {
        ord = rhs[i]-'0';
      }
      rhsn[i] = ord;
    }
    props->chanmap[lhsn[0]] = rhsn[0];
    props->chanmap[lhsn[1]] = rhsn[1];
    props->chanmap[lhsn[2]] = rhsn[2];
    props->chanmap[lhsn[3]] = rhsn[3];
    return;
  }

  
  {
    int inlen;
    inlen = strlen(rhs);
    // RHS should be something like rgb or rb_a or lum or luma
    //      turn it into            012 or 01-13   000 or 0001

    // RHS='lum' etc only really make sense if the source
    // image really is luminance image.  Otherwise pretend red channel
    // is luminance, since we're not going to compute a luminance from
    // multiple channels.
    if (!strcmp("lum",rhs) || !strcmp("l",rhs) ) {
      rhsn[0] = 0;
      rhsn[1] = 0;
      rhsn[2] = 0;
    }
    else if (!strcmp("luma",rhs) || !strcmp("la",rhs) ) {
      rhsn[0] = 0;
      rhsn[1] = 0;
      rhsn[2] = 0;
      rhsn[3] = 1;
    }
    else if (!strcmp("alum",rhs) || !strcmp("al",rhs) ) {
      rhsn[0] = 1;
      rhsn[1] = 1;
      rhsn[2] = 1;
      rhsn[3] = 0;
    }
    else {
      int outchan;
      for (outchan=0; outchan<inlen; outchan++)
      {
        switch(rhs[outchan]) 
        {
        case '_':
          rhsn[outchan] = -1;  break;
        case 'r':
          rhsn[outchan] = 0;  break;
        case 'g':
          rhsn[outchan] = 1;  break;
        case 'b':
          rhsn[outchan] = 2;  break;
        case 'a':
          rhsn[outchan] = 3;  break;
        default:
          // ERROR!
          break;
        }
      }
    }
  }

  {
    char chanmapcopy[4];
    char lastRHS = -1;
    for (i=0; i<4; i++) {
      chanmapcopy[i] = props->chanmap[i];
    }
    for (i=0; i<4; i++) {
      if (lhsn[i] < 0) continue;
      if (rhsn[i]==-2) 
        props->chanmap[ lhsn[i] ] = chanmapcopy[ lastRHS ];
      else if (rhsn[i]<0)
        props->chanmap[ lhsn[i] ] = -1;
      else 
        props->chanmap[ lhsn[i] ] = chanmapcopy[ rhsn[i] ];

      if (rhsn[i]>=-1) lastRHS = rhsn[i];
    }
  }
}


static void parseFormatExpr(ImProps* props, char** fmt, va_list *args)
{
  // could be either straight FMT          declaration or
  //                 or       FMT=OUT_FMT  (channel select & swizzle)
  //                 or       FMT*scale    (scale and bias op)
  //                 or       FMT/scale
  //                 or       FMT+bias
  //                 or       FMT-bias
  char infmt[5];
  BOOL whitespace = FALSE;
  strcpy(infmt,tokenBuf);
  
  if (isspace(**fmt)) whitespace = TRUE;

  nextToken( fmt );
  if ( currentToken == TOK_EQUALS ) {
    // channel swizzling case
    nextToken( fmt );
    if (currentToken == TOK_OUTFMT) {
      parseOutputFormatDecl(props, infmt, tokenBuf);
      nextToken( fmt );
      return;
    }
    else {
      // bad expression 'TOK_FMT = non-OUTFMT'
      // ignore equals bit and just take the first TOK_FMT as a declaration
      parseInputFormatDecl(props, infmt);
      return;
    }
  }
  else if ( !whitespace &&
            (currentToken == TOK_TIMES  ||
            currentToken == TOK_DIVIDE ||
            currentToken == TOK_PLUS   ||
            currentToken == TOK_MINUS) ) 
  {
    // scale and bias particular channels case
    // this is one case where the presence or absence of whitespace
    // makes a difference.
    //     'rgb *.5'  is not the samce as
    //      rgb*.5'
    int op=currentToken;
    float sb;
    BOOL isAuto;

    if ( parseScaleBiasValue(&sb, fmt, args, &isAuto) ) {
      char *pinfmt = infmt;
      float *pScaleOrBias;
      
      if (op==TOK_PLUS||op==TOK_MINUS){
        pScaleOrBias = props->bias;
        props->flags |= IMDBG_FLAG_BIAS_SET;
      }
      else {
        pScaleOrBias = props->scale;
        props->flags |= IMDBG_FLAG_SCALE_SET;
      }
      
      if (op==TOK_DIVIDE && sb!=0)  sb = 1.0f/sb;
      if (op==TOK_MINUS          )  sb = -sb;
      
      // apply specified scale/bias to specified channels
      while (*pinfmt) {
        if (!strncmp(pinfmt,"lum",3)) {
          pScaleOrBias[0] = sb;
          pScaleOrBias[1] = sb;
          pScaleOrBias[2] = sb;
          pinfmt+=3;
        }
        else if (*pinfmt == 'r') {
          pScaleOrBias[0] = sb;
          pinfmt++;
        }
        else if (*pinfmt == 'g') {
          pScaleOrBias[1] = sb;
          pinfmt++;
        }
        else if (*pinfmt == 'b') {
          pScaleOrBias[2] = sb;
          pinfmt++;
        }
        else if (*pinfmt == 'a') {
          pScaleOrBias[3] = sb;
          pinfmt++;
        }
      }
    }
  }
  else {
    // just a plain case of something like 'rgb'
    parseInputFormatDecl(props, infmt);
  }


}

void processArgs(ImProps *props, byte**data, 
                 const char* fmt, va_list args)
{
  char* pfmt = inputBuf;
  //strcpy(inputBuf, fmt);
  strcpytolower(inputBuf, fmt);

  currentToken=TOK_BEGIN;
  tokenBuf[0]=0;
  tokenVal=-1;

  nextToken( &pfmt );

  /* Process format string */
  while( currentToken != TOK_EOF )
  {
    switch(currentToken) 
    {
    case TOK_WIDTH:
      parseIntSetExpr(&props->width, &pfmt, &args);
      break;
    case TOK_HEIGHT:
      parseIntSetExpr(&props->height, &pfmt, &args);
      break;
    case TOK_COLSKIP:  
      parseIntSetExpr(&props->colstride, &pfmt, &args);
      break;
    case TOK_ROWSKIP:
      parseIntSetExpr(&props->rowstride, &pfmt, &args);
      break;
    case TOK_TITLE:
      parseStringSetExpr(props->title, &pfmt, &args);
      break;

    case TOK_PLUS:
    {
      float sb;
      BOOL isAuto;
      if ( parseScaleBiasValue(&sb, &pfmt, &args, &isAuto) ) {
        // apply specified scale to rgb channels
        props->bias[0]=props->bias[1]=props->bias[2]=sb;
        props->flags |= IMDBG_FLAG_BIAS_SET;
        if (isAuto) {
          props->flags |= IMDBG_FLAG_AUTOSCALE;
          props->flags |= IMDBG_FLAG_SCALE_SET;
        }
      }
    }
    break;
    case TOK_MINUS:
    {
      float sb;
      BOOL isAuto;
      if ( parseScaleBiasValue(&sb, &pfmt, &args, &isAuto) ) {
        sb *= -1.0f;
        // apply specified scale to rgb channels
        props->bias[0]=props->bias[1]=props->bias[2]=sb;
        props->flags |= IMDBG_FLAG_BIAS_SET;
        if (isAuto) {
          props->flags |= IMDBG_FLAG_AUTOSCALE;
          props->flags |= IMDBG_FLAG_SCALE_SET;
        }
      }
    }
    break;
    case TOK_TIMES:
    {
      float sb;
      BOOL isAuto;
      if ( parseScaleBiasValue(&sb, &pfmt, &args, &isAuto) ) {
        // apply specified scale to rgb channels
        props->scale[0]=props->scale[1]=props->scale[2]=sb;
        props->flags |= IMDBG_FLAG_SCALE_SET;
        if (isAuto) {
          props->flags |= IMDBG_FLAG_AUTOSCALE;
          props->flags |= IMDBG_FLAG_BIAS_SET;
        }
      }
    }
    break;
    case TOK_DIVIDE:
    {
      float sb;
      BOOL isAuto;
      if ( parseScaleBiasValue(&sb, &pfmt, &args, &isAuto) ) {
        if (sb!=0) sb = 1.0f/sb;
        // apply specified scale to rgb channels
        props->scale[0]=props->scale[1]=props->scale[2]=sb;
        props->flags |= IMDBG_FLAG_SCALE_SET;
        if (isAuto) {
          props->flags |= IMDBG_FLAG_AUTOSCALE;
          props->flags |= IMDBG_FLAG_BIAS_SET;
        }
      }
    }
    break;

    case TOK_BPC:
      {
        int numSet = parseBPCArrayExpr(&props->bpc[0], &props->type[0], 
                                       &pfmt, &args);
        int i;

        // fill out the rest of the channels with the same width
        // as the last channel set explicitly
        if (numSet>0) {
          for (i=numSet; i< props->nchan; i++) {
            props->type[i] = props->type[numSet-1];
            props->bpc[i]  = props->bpc[numSet-1];
          }
        }
      }
      break;

    case TOK_FMT:
      parseFormatExpr(props, &pfmt, &args);
      break;

    case TOK_PTRVAR:
      *data = va_arg( args, BYTE* );
      nextToken( &pfmt );
      break;

    default:
      // ERROR!  just keep going and ignore
      nextToken( &pfmt );
    }
  }
}

