/*******************************************************************************/
/*                                                                             */
/* libcoolstream/ca.h                                                          */
/*   Public header for CA interface                                            */
/*                                                                             */
/* (C) 2010 CoolStream International Ltd.                                      */
/*                                                                             */
/*******************************************************************************/
#ifndef __CA_CS_H_
#define __CA_CS_H_

#include <OpenThreads/Thread>
#include "cs_types.h"

enum CA_INIT_MASK {
	CA_INIT_SC	= 1,
	CA_INIT_CI,
	CA_INIT_BOTH
};

enum CA_SLOT_TYPE {
	CA_SLOT_TYPE_SMARTCARD,
	CA_SLOT_TYPE_CI,
	CA_SLOT_TYPE_ALL,
};

enum CA_MESSAGE_FLAGS {
	CA_MESSAGE_EMPTY		= (1 << 0),
	CA_MESSAGE_HAS_PARAM1_DATA	= (1 << 1), // Free after use!
	CA_MESSAGE_HAS_PARAM1_INT	= (1 << 2),
	CA_MESSAGE_HAS_PARAM1_PTR	= (1 << 3),
	CA_MESSAGE_HAS_PARAM2_INT	= (1 << 4),
	CA_MESSAGE_HAS_PARAM2_PTR	= (1 << 5),
	CA_MESSAGE_HAS_PARAM2_DATA	= (1 << 6),
	CA_MESSAGE_HAS_PARAM3_DATA	= (1 << 7), // Free after use!
	CA_MESSAGE_HAS_PARAM3_INT	= (1 << 8),
	CA_MESSAGE_HAS_PARAM3_PTR	= (1 << 9),
	CA_MESSAGE_HAS_PARAM4_INT	= (1 << 10),
	CA_MESSAGE_HAS_PARAM4_PTR	= (1 << 11),
	CA_MESSAGE_HAS_PARAM4_DATA	= (1 << 12),
	CA_MESSAGE_HAS_PARAM_LONG	= (1 << 13),
};

enum CA_MESSAGE_MSGID {
	CA_MESSAGE_MSG_INSERTED,
	CA_MESSAGE_MSG_REMOVED,
	CA_MESSAGE_MSG_INIT_OK,
	CA_MESSAGE_MSG_INIT_FAILED,
	CA_MESSAGE_MSG_MMI_MENU,
	CA_MESSAGE_MSG_MMI_MENU_ENTER,
	CA_MESSAGE_MSG_MMI_MENU_ANSWER,
	CA_MESSAGE_MSG_MMI_LIST,
	CA_MESSAGE_MSG_MMI_TEXT,
	CA_MESSAGE_MSG_MMI_REQ_INPUT,
	CA_MESSAGE_MSG_MMI_CLOSE,
	CA_MESSAGE_MSG_INTERNAL,
	CA_MESSAGE_MSG_PMT_ARRIVED,
	CA_MESSAGE_MSG_CAT_ARRIVED,
	CA_MESSAGE_MSG_ECM_ARRIVED,
	CA_MESSAGE_MSG_EMM_ARRIVED,
	CA_MESSAGE_MSG_CHANNEL_CHANGE,
	CA_MESSAGE_MSG_EXIT,
};

typedef struct CA_MESSAGE {
	u32 MsgId;
	enum CA_SLOT_TYPE SlotType;
	int Slot;
	u32 Flags;
	union {
		u8 *Data[4];
		u32 Param[4];
		void *Ptr[4];
		u64 ParamLong;
	} Msg;
} CA_MESSAGE;

#define CA_MESSAGE_SIZE		sizeof(CA_MESSAGE)
#define CA_MESSAGE_ENTRIES	256
#define CA_MESSAGE_ENTRIES_CI	128
#define CA_MESSAGE_ENTRIES_SC	64

#ifndef CS_CA_PDATA
#define CS_CA_PDATA		void
#endif

class cCA : public OpenThreads::Thread {
private:
	static cCA *inst;
	//
	cCA(void);
	//
	CS_CA_PDATA		*privateData;
	enum CA_INIT_MASK	initMask;
	bool exit;
	bool started;
	bool guiReady;
	virtual void run(void);
public:
	u32 GetNumberCISlots(void);
	u32 GetNumberSmartCardSlots(void);	//
	static cCA *GetInstance(void);
	bool SendPMT(int Unit, unsigned char *Data, int Len, enum CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
	bool SendMessage(const CA_MESSAGE *Msg);
	void SetInitMask(enum CA_INIT_MASK InitMask);
	bool Start(void);
	void Stop(void);
	void Ready(bool Set);
	void ModuleReset(enum CA_SLOT_TYPE, u32 Slot);
	bool ModulePresent(enum CA_SLOT_TYPE, u32 Slot);
	void ModuleName(enum CA_SLOT_TYPE, u32 Slot, char *Name);
	void MenuEnter(enum CA_SLOT_TYPE, u32 Slot);
	void MenuAnswer(enum CA_SLOT_TYPE, u32 Slot, u32 choice);
	void InputAnswer(enum CA_SLOT_TYPE, u32 Slot, u8 * Data, int Len);
	void MenuClose(enum CA_SLOT_TYPE, u32 Slot);
	virtual ~cCA();
};

#endif //__CA_H_
