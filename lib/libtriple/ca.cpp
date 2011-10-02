#include <stdio.h>

#include "ca.h"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_CA, this, args)


static cCA *inst = NULL;

/* those are all dummies for now.. */
cCA::cCA(void)
{
	lt_debug("%s\n", __FUNCTION__);
}

cCA::~cCA()
{
	lt_debug("%s\n", __FUNCTION__);
}

cCA *cCA::GetInstance()
{
	_lt_debug(TRIPLE_DEBUG_CA, NULL, "%s\n", __FUNCTION__);
	if (inst == NULL)
		inst = new cCA();

	return inst;
}

void cCA::MenuEnter(enum CA_SLOT_TYPE, uint32_t p)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

void cCA::MenuAnswer(enum CA_SLOT_TYPE, uint32_t p, uint32_t /*choice*/)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

void cCA::InputAnswer(enum CA_SLOT_TYPE, uint32_t p, uint8_t * /*Data*/, int /*Len*/)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

void cCA::MenuClose(enum CA_SLOT_TYPE, uint32_t p)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

uint32_t cCA::GetNumberCISlots(void)
{
	lt_debug("%s\n", __FUNCTION__);
	return 0;
}

uint32_t cCA::GetNumberSmartCardSlots(void)
{
	lt_debug("%s\n", __FUNCTION__);
	return 0;
}

void cCA::ModuleName(enum CA_SLOT_TYPE, uint32_t p, char * /*Name*/)
{
	/* TODO: waht to do with *Name? */
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

bool cCA::ModulePresent(enum CA_SLOT_TYPE, uint32_t p)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
	return false;
}

void cCA::ModuleReset(enum CA_SLOT_TYPE, uint32_t p)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

bool cCA::SendPMT(int, unsigned char *, int, CA_SLOT_TYPE)
{
	lt_debug("%s\n", __FUNCTION__);
	return true;
}

bool cCA::SendMessage(const CA_MESSAGE *)
{
	lt_debug("%s\n", __FUNCTION__);
	return true;
}

bool cCA::Start(void)
{
	lt_debug("%s\n", __FUNCTION__);
	return true;
}

void cCA::Stop(void)
{
	lt_debug("%s\n", __FUNCTION__);
}

void cCA::Ready(bool p)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}

void cCA::SetInitMask(enum CA_INIT_MASK p)
{
	lt_debug("%s param:%d\n", __FUNCTION__, (int)p);
}
