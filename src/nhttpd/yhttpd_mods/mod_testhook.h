//=============================================================================
// YHTTPD
// TestHook
//=============================================================================
#ifndef TESTHOOK_H_
#define TESTHOOK_H_


#include "yhook.h"
class CTesthook : public Cyhook
{
public:
	THandleStatus Hook_SendResponse(CyhookHandler *hh); 
	CTesthook(){};
	~CTesthook(){};
	
	virtual std::string getHookName(void) {return std::string("Testhook");}
	
};
#endif /*TESTHOOK_H_*/

