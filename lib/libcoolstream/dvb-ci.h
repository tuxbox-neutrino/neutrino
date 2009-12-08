#ifndef __DVBCI_H
#define __DVBCI_H

#ifdef  __cplusplus
extern "C" {
#endif
void CI_MenuAnswer(unsigned char bSlotIndex,unsigned char choice);
void CI_Answer(unsigned char bSlotIndex,unsigned char *pBuffer,unsigned char nLength);
void CI_CloseMMI(unsigned char bSlotIndex);
void CI_EnterMenu(unsigned char bSlotIndex);
#ifdef  __cplusplus
}
#endif

#define MAX_MMI_ITEMS 20
#define MAX_MMI_TEXT_LEN 255
#define MAX_MMI_CHOICE_TEXT_LEN 255

typedef struct
{
        int slot;
        int choice_nb;
        char title[MAX_MMI_TEXT_LEN];
        char subtitle[MAX_MMI_TEXT_LEN];
        char bottom[MAX_MMI_TEXT_LEN];
        char choice_item[MAX_MMI_ITEMS][MAX_MMI_CHOICE_TEXT_LEN];
} MMI_MENU_LIST_INFO;

typedef struct
{
        int slot;
        int blind;
        int answerlen;
        char enguiryText[MAX_MMI_TEXT_LEN];

} MMI_ENGUIRY_INFO;

typedef void (*SEND_MSG_HOOK) (unsigned int msg, unsigned int data);

class cDvbCi {
	private:
		int slots;
		bool init;
		int pmtlen;
		unsigned char * pmtbuf;
		void SendPMT();
		pthread_mutex_t ciMutex;
	public:
		bool Init(void);
		bool SendPMT(unsigned char *data, int len);
		bool SendDateTime(void);
		//
		cDvbCi(int Slots);
		~cDvbCi();
		static cDvbCi * getInstance();
		SEND_MSG_HOOK SendMessage;
		void SetHook(SEND_MSG_HOOK _SendMessage) { SendMessage = _SendMessage; };
		bool CamPresent(int slot);
		bool GetName(int slot, char * name);
};

#endif //__DVBCI_H
