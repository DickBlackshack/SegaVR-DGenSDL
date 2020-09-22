#ifndef _PLATFORM_H
#define _PLATFORM_H

#if defined (__MINGW32__) || defined(_MSC_VER)
	#define MING_OR_MSC
#endif

#if defined(_MSC_VER)
	#pragma warning(disable : 4996)
	#pragma warning(disable : 4244)

	#include <stdio.h>
	#include <direct.h>
	#include <stdio.h>
	#include <sys/types.h>
	
	#ifndef snprintf 
		#define snprintf _snprintf
	#endif
	#define strncasecmp strnicmp
	#define strcasecmp stricmp
#endif

#ifndef INLINE
#define INLINE inline
#endif

#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif

#endif //_PLATFORM_H
