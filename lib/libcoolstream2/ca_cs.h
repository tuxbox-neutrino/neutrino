/*******************************************************************************/
/*                                                                             */
/* libcoolstream/ca.h                                                          */
/*   Public header for CA interface                                            */
/*                                                                             */
/* (C) 2010 CoolStream International Ltd.                                      */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __CA_CS_H_
#define __CA_CS_H_

#include <OpenThreads/Thread>
#include <OpenThreads/Mutex>
#include <vector>
#include "cs_types.h"

enum CA_INIT_MASK {
	CA_INIT_SC	= 1,
	CA_INIT_CI,
	CA_INIT_BOTH
};

enum CA_SLOT_TYPE {
	CA_SLOT_TYPE_SMARTCARD,
	CA_SLOT_TYPE_CI,
	CA_SLOT_TYPE_ALL
};

enum CA_DVBCI_TS_INPUT {
	CA_DVBCI_TS_INPUT_0 = 0,
	CA_DVBCI_TS_INPUT_1,
	CA_DVBCI_TS_INPUT_2,
	CA_DVBCI_TS_INPUT_3,
	CA_DVBCI_TS_INPUT_DISABLED
};

enum CA_MESSAGE_FLAGS {
	CA_MESSAGE_EMPTY		= (1 << 0),
	CA_MESSAGE_HAS_PARAM1_DATA	= (1 << 1), /// Free after use!
	CA_MESSAGE_HAS_PARAM1_INT	= (1 << 2),
	CA_MESSAGE_HAS_PARAM1_PTR	= (1 << 3),
	CA_MESSAGE_HAS_PARAM2_INT	= (1 << 4),
	CA_MESSAGE_HAS_PARAM2_PTR	= (1 << 5),
	CA_MESSAGE_HAS_PARAM2_DATA	= (1 << 6),
	CA_MESSAGE_HAS_PARAM3_DATA	= (1 << 7), /// Free after use!
	CA_MESSAGE_HAS_PARAM3_INT	= (1 << 8),
	CA_MESSAGE_HAS_PARAM3_PTR	= (1 << 9),
	CA_MESSAGE_HAS_PARAM4_INT	= (1 << 10),
	CA_MESSAGE_HAS_PARAM4_PTR	= (1 << 11),
	CA_MESSAGE_HAS_PARAM4_DATA	= (1 << 12),
	CA_MESSAGE_HAS_PARAM5_INT	= (1 << 13),
	CA_MESSAGE_HAS_PARAM5_PTR	= (1 << 14),
	CA_MESSAGE_HAS_PARAM5_DATA	= (1 << 15),
	CA_MESSAGE_HAS_PARAM6_INT	= (1 << 16),
	CA_MESSAGE_HAS_PARAM6_PTR	= (1 << 17),
	CA_MESSAGE_HAS_PARAM6_DATA	= (1 << 18),
	CA_MESSAGE_HAS_PARAM1_LONG	= (1 << 19),
	CA_MESSAGE_HAS_PARAM2_LONG	= (1 << 20),
	CA_MESSAGE_HAS_PARAM3_LONG	= (1 << 21),
	CA_MESSAGE_HAS_PARAM4_LONG	= (1 << 22)
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
	CA_MESSAGE_MSG_CAPMT_ARRIVED,
	CA_MESSAGE_MSG_CAT_ARRIVED,
	CA_MESSAGE_MSG_ECM_ARRIVED,
	CA_MESSAGE_MSG_EMM_ARRIVED,
	CA_MESSAGE_MSG_CHANNEL_CHANGE,
	CA_MESSAGE_MSG_GUI_READY,
	CA_MESSAGE_MSG_EXIT
};

typedef struct CA_MESSAGE {
	u32 MsgId;
	enum CA_SLOT_TYPE SlotType;
	int Slot;
	u32 Flags;
	union {
		u8 *Data[8];
		u32 Param[8];
		void *Ptr[8];
		u64 ParamLong[4];
	} Msg;
} CA_MESSAGE;

typedef std::vector<u16>			CaIdVector;
typedef std::vector<u16>::iterator		CaIdVectorIterator;
typedef std::vector<u16>::const_iterator	CaIdVectorConstIterator;

#define CA_MESSAGE_SIZE		sizeof(CA_MESSAGE)
#define CA_MESSAGE_ENTRIES	256
#define CA_MESSAGE_ENTRIES_CI	128
#define CA_MESSAGE_ENTRIES_SC	64

#ifndef CS_CA_PDATA
#define CS_CA_PDATA		void
#endif

/// CA module class
class cCA : public OpenThreads::Thread {
private:
	/// Static instance of the CA module
	static cCA *inst;
	static OpenThreads::Mutex lock;
	/// Private constructor (singleton method)
	cCA(void);
	/// Private data for the CA module
	CS_CA_PDATA		*privateData;
	enum CA_INIT_MASK	initMask;
	bool exit;
	bool started;
	bool guiReady;
	/// Thread method
	virtual void run(void);
public:
	/// Returns the number of CA slots (CI+SC, CI, SC)
	u32 GetNumberSlots(enum CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
	u32 GetNumberCISlots(void) { return GetNumberSlots(CA_SLOT_TYPE_CI); }
	u32 GetNumberSmartCardSlots(void) { return GetNumberSlots(CA_SLOT_TYPE_SMARTCARD); }
	/// Singleton
	static cCA *GetInstance(void);
	/// Send PMT to a individual or to all available modules (DEPRECATED)
	bool SendPMT(int Unit, unsigned char *Data, int Len, enum CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
	/// Sends a message to the CA thread
	bool SendMessage(const CA_MESSAGE *Msg);
	/// Sets which modules to initialize. It is only
	/// possible to change this once!
	void SetInitMask(enum CA_INIT_MASK InitMask);
	/// Sets the tuner input  (only valid for CI)
	void SetTS(enum CA_DVBCI_TS_INPUT TsInput);
	/// Sets the frequency (in Hz) of the TS stream input (only valid for CI)
	void SetTSClock(u32 Speed);
	/// Start the CA module
	bool Start(void);
	/// Stops the CA module
	void Stop(void);
	/// Notify that the GUI is ready to receive messages
	/// (CA messages coming from a module)
	void Ready(bool Set);
	/// Resets a module (if possible)
	void ModuleReset(enum CA_SLOT_TYPE, u32 Slot);
	/// Checks if a module is present
	bool ModulePresent(enum CA_SLOT_TYPE, u32 Slot);
	/// Returns the module name in array Name
	void ModuleName(enum CA_SLOT_TYPE, u32 Slot, char *Name);
	/// Notify the module we want to enter menu
	void MenuEnter(enum CA_SLOT_TYPE, u32 Slot);
	/// Notify the module with our answer (choice nr)
	void MenuAnswer(enum CA_SLOT_TYPE, u32 Slot, u32 choice);
	/// Notify the module with our answer (binary)
	void InputAnswer(enum CA_SLOT_TYPE, u32 Slot, u8 * Data, int Len, bool Cancelled = false);
	/// Notify the module we closed the menu
	void MenuClose(enum CA_SLOT_TYPE, u32 Slot);
	/// Get the supported CAIDs
	int GetCAIDS(CaIdVector & Caids, enum CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
	/// Send a CA-PMT object and Raw unparsed PMT to the CA layer
	bool SendCAPMT(u64 ChannelId, u8 DemuxSource, u8 DemuxMask, const unsigned char *CAPMT, u32 CAPMTLen, const unsigned char *RawPMT, u32 RawPMTLen, enum CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
	/// Virtual destructor
	virtual ~cCA();
};

#endif ///__CA_H_
