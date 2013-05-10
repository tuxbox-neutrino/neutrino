/*******************************************************************************/
/*                                                                             */
/* libcoolstream/mmi.h                                                         */
/*   Public header file for CoolStream Public CA MMI API                       */
/*                                                                             */
/* (C) 2010 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __MMI_H_
#define __MMI_H_

#define MAX_MMI_ITEMS			40
#define MAX_MMI_TEXT_LEN		255
#define MAX_MMI_CHOICE_TEXT_LEN		255

typedef enum {
	MMI_TOP_MENU_SUBS		= 1,
	MMI_TOP_MENU_EVENTS,
	MMI_TOP_MENU_TOKENS,
	MMI_TOP_MENU_PIN,
	MMI_TOP_MENU_MATURE,
	MMI_TOP_MENU_ABOUT
} MMI_MENU_CURRENT;

typedef enum {
	MMI_MENU_LEVEL_MAIN		= 0,
	MMI_MENU_LEVEL_MATURE,
	MMI_MENU_LEVEL_ASK_PIN_MATURE
} MMI_MENU_LEVEL;

typedef enum {
	MMI_PIN_LEVEL_ASK_OLD	= 0,
	MMI_PIN_LEVEL_CHECK_CURRENT,
	MMI_PIN_LEVEL_ASK_REPEAT,
	MMI_PIN_LEVEL_CHECK_AND_CHANGE
} MMI_PIN_LEVEL;

typedef struct {
	int choice_nb;
	char title[MAX_MMI_TEXT_LEN];
	char subtitle[MAX_MMI_TEXT_LEN];
	char bottom[MAX_MMI_TEXT_LEN];
	char choice_item[MAX_MMI_ITEMS][MAX_MMI_CHOICE_TEXT_LEN];
} MMI_MENU_LIST_INFO;

typedef struct {
	int blind;
	int answerlen;
	char enguiryText[MAX_MMI_TEXT_LEN];
} MMI_ENGUIRY_INFO;

#endif // __MMI_H_

