/// \file version.h
/// \brief Version information
///
/// \author Kevin Gema
/// \version $Id$
//-------------------------------------------------------------------------------------------------

#ifndef VERSION_H_
#define VERSION_H_

#ifdef __cplusplus
extern "C"
{
#endif

static const char* const BUILD_DATE = __DATE__;
static const char* const BUILD_TIME = __TIME__;
static const char* const APP_NAME = "RaPi iThemba";

#define MAJOR_VERSION 1
#define MINOR_VERSION 1

#ifdef __cplusplus
}
#endif

#endif /* VERSION_H_ */
