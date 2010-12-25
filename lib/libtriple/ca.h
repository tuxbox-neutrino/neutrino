/*
 * dummy functions to implement ca_cs.h interface
 */
#ifndef __CA_LIBTRIPLE_H_
#define __CA_LIBTRIPLE_H_

#include <stdint.h>
/* used in cam_menu.cpp */
typedef uint32_t u32;

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
	uint32_t MsgId;
	enum CA_SLOT_TYPE SlotType;
	int Slot;
	uint32_t Flags;
	union {
		uint8_t *Data[4];
		uint32_t Param[4];
		void *Ptr[4];
		uint64_t ParamLong;
	} Msg;
} CA_MESSAGE;

class cCA {
private:
	cCA(void);
public:
	uint32_t GetNumberCISlots(void);
	uint32_t GetNumberSmartCardSlots(void);
	static cCA *GetInstance(void);
	bool SendPMT(int Unit, unsigned char *Data, int Len, CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
	bool SendMessage(const CA_MESSAGE *Msg);
	void SetInitMask(enum CA_INIT_MASK InitMask);
	bool Start(void);
	void Stop(void);
	void Ready(bool Set);
	void ModuleReset(enum CA_SLOT_TYPE, uint32_t Slot);
	bool ModulePresent(enum CA_SLOT_TYPE, uint32_t Slot);
	void ModuleName(enum CA_SLOT_TYPE, uint32_t Slot, char *Name);
	void MenuEnter(enum CA_SLOT_TYPE, uint32_t Slot);
	void MenuAnswer(enum CA_SLOT_TYPE, uint32_t Slot, uint32_t choice);
	void InputAnswer(enum CA_SLOT_TYPE, uint32_t Slot, uint8_t * Data, int Len);
	void MenuClose(enum CA_SLOT_TYPE, uint32_t Slot);
	virtual ~cCA();
};

#endif // __CA_LIBTRIPLE_H_
