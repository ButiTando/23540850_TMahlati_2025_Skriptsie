/// \file
/// \brief Standard types used throughout the code.
///
/// \details This driver defines specific type sizes.
///
/// \author Rijan de Nysschen
/// \version $Id$
//-------------------------------------------------------------------------------------------------
#ifndef __INT_TYPES_H__
#define __INT_TYPES_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned char         uchar;
typedef unsigned short        ushort;
typedef unsigned long         ulong;
typedef unsigned int          uint;
typedef signed char           schar;
typedef signed short          sshort;
typedef signed long           slong;
typedef signed int            sint;

#ifdef __GNUC__ /// GNU C Compiler
    #include <stdbool.h>

    typedef unsigned char       U8;
    typedef signed char         S8;
    typedef unsigned int        U16;
    typedef signed int          S16;
    typedef unsigned long int   U32;
    typedef signed long int     S32;
    typedef unsigned long long  U64;
    typedef signed long long    S64;
#endif

#define TRUE        1
#define FALSE       0

#define READ        1
#define WRITE       0

#define ON          1
#define OFF         0

#define SUCCESS     0
#define FAILURE     1

#define POSITIVE    0
#define NEGATIVE    1

#ifndef NULL
    #define NULL    0
#endif

//-------------------------------------------------------------------------------------------------
// Bit operations
#define bit_get(p,m)        ((p) & (m))
#define bit_set(p,m)        ((p) |= (m))
#define bit_clear(p,m)      ((p) &= ~(m))
#define bit_flip(p,m)       ((p) ^= (m))
#define bit_write(c,p,m)    (c ? bit_set(p,m) : bit_clear(p,m))
#define BIT(x)              (0x01 << (x))
#define LONGBIT(x)          ((unsigned long)0x00000001 << (x))

#define LOW_BYTE(x)         (unsigned char)(x & 0x00ff) 
#define HIGH_BYTE(x)        (unsigned char)((x >> 8) & 0x00ff)
	
#ifdef __cplusplus
}
#endif
   
#endif /* __INT_TYPES_H__ */
