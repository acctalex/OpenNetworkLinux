/**************************************************************************//**
 *
 * @file
 * @brief x86_64_accton_es8632bt4 Porting Macros.
 *
 * @addtogroup x86_64_accton_es8632bt4-porting
 * @{
 *
 *****************************************************************************/
#ifndef __X86_64_ACCTON_ES8632BT4_PORTING_H__
#define __X86_64_ACCTON_ES8632BT4_PORTING_H__


/* <auto.start.portingmacro(ALL).define> */
#if X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_INCLUDE_STDLIB_HEADERS == 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <memory.h>
#endif

#ifndef X86_64_ACCTON_ES8632BT4_MALLOC
	#if defined(GLOBAL_MALLOC)
		#define X86_64_ACCTON_ES8632BT4_MALLOC GLOBAL_MALLOC
	#elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
		#define X86_64_ACCTON_ES8632BT4_MALLOC malloc
	#else
		#error The macro X86_64_ACCTON_ES8632BT4_MALLOC is required but cannot be defined.
	#endif
#endif

#ifndef X86_64_ACCTON_ES8632BT4_FREE
	#if defined(GLOBAL_FREE)
		#define X86_64_ACCTON_ES8632BT4_FREE GLOBAL_FREE
	#elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
		#define X86_64_ACCTON_ES8632BT4_FREE free
	#else
		#error The macro X86_64_ACCTON_ES8632BT4_FREE is required but cannot be defined.
	#endif
#endif

#ifndef X86_64_ACCTON_ES8632BT4_MEMSET
    #if defined(GLOBAL_MEMSET)
	#define X86_64_ACCTON_ES8632BT4_MEMSET GLOBAL_MEMSET
    #elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
	#define X86_64_ACCTON_ES8632BT4_MEMSET memset
    #else
	#error The macro X86_64_ACCTON_ES8632BT4_MEMSET is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_ACCTON_ES8632BT4_MEMCPY
	#if defined(GLOBAL_MEMCPY)
		#define X86_64_ACCTON_ES8632BT4_MEMCPY GLOBAL_MEMCPY
	#elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
		#define X86_64_ACCTON_ES8632BT4_MEMCPY memcpy
	#else
		#error The macro X86_64_ACCTON_ES8632BT4_MEMCPY is required but cannot be defined.
	#endif
#endif

#ifndef X86_64_ACCTON_ES8632BT4_VSNPRINTF
	#if defined(GLOBAL_VSNPRINTF)
		#define X86_64_ACCTON_ES8632BT4_VSNPRINTF GLOBAL_VSNPRINTF
	#elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
		#define X86_64_ACCTON_ES8632BT4_VSNPRINTF vsnprintf
	#else
		#error The macro X86_64_ACCTON_ES8632BT4_VSNPRINTF is required but cannot be defined.
	#endif
#endif

#ifndef X86_64_ACCTON_ES8632BT4_SNPRINTF
	#if defined(GLOBAL_SNPRINTF)
		#define X86_64_ACCTON_ES8632BT4_SNPRINTF GLOBAL_SNPRINTF
	#elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
		#define X86_64_ACCTON_ES8632BT4_SNPRINTF snprintf
	#else
		#error The macro X86_64_ACCTON_ES8632BT4_SNPRINTF is required but cannot be defined.
	#endif
#endif

#ifndef X86_64_ACCTON_ES8632BT4_STRLEN
	#if defined(GLOBAL_STRLEN)
		#define X86_64_ACCTON_ES8632BT4_STRLEN GLOBAL_STRLEN
	#elif X86_64_ACCTON_ES8632BT4_CONFIG_PORTING_STDLIB == 1
		#define X86_64_ACCTON_ES8632BT4_STRLEN strlen
	#else
		#error The macro X86_64_ACCTON_ES8632BT4_STRLEN is required but cannot be defined.
	#endif
#endif

/* <auto.end.portingmacro(ALL).define> */


#endif /* __X86_64_ACCTON_ES8632BT4_PORTING_H__ */
/* @} */
