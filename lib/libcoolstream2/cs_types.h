/*******************************************************************************/
/*                                                                             */
/* libcoolstream/cs_types.h                                                    */
/*   Public header file for CoolStream Public API                              */
/*                                                                             */
/* (C) 2010 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __CS_TYPES_H_
#define __CS_TYPES_H_

typedef enum
{
	AVSYNC_DISABLED,
	AVSYNC_ENABLED,
	AVSYNC_AUDIO_IS_MASTER
} AVSYNC_TYPE;

typedef unsigned long long	u64;
typedef unsigned int		u32;
typedef unsigned short		u16;
typedef unsigned char		 u8;
typedef signed long long	s64;
typedef signed int		s32;
typedef signed short		s16;
typedef signed char		 s8;

#endif // __CS_TYPES_H_
