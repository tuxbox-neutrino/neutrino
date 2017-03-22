#ifndef __HELPERS_CS_H__
#define __HELPERS_CS_H__

enum {
	CS_EXTRA_DEBUG_OFF	= 0x00000000,
	CS_EXTRA_DEBUG_VIDEO1	= 0x00000001,
	CS_EXTRA_DEBUG_ALL	= 0xFFFFFFFF
};

void cs_set_extra_debug(uint32_t mode);

const char* __func_ext__f(const char* _func_, int _line_, const char* _file_, bool havePathFile);
#define __func_ext__      __func_ext__f(__PRETTY_FUNCTION__, __LINE__, NULL, true)
#define __func_ext_file__ __func_ext__f(__PRETTY_FUNCTION__, __LINE__, __path_file__, true)

#endif // __HELPERS_CS_H__
