//=============================================================================
// YHTTPD
// TestHook
//=============================================================================

#include "mod_testhook.h"

THandleStatus CTesthook::Hook_SendResponse(CyhookHandler *hh)
{
	THandleStatus status = HANDLED_NONE;
	if(hh->UrlData["filename"] == "test2")
	{
		hh->yresult = "test it 2222\n";
		status = HANDLED_READY;
	}
	return status;
}
