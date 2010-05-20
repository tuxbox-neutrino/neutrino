#ifndef __CS_API_H_
#define __CS_API_H_

typedef void (*cs_messenger) (unsigned int msg, unsigned int data);

// Initialization
void cs_api_init(void);
void cs_api_exit(void);

// Memory helpers
void *cs_malloc_uncached(size_t size);
void cs_free_uncached(void *ptr);
void *cs_phys_addr(void *ptr);

// Callback function helpers
void cs_register_messenger(cs_messenger messenger);
void cs_deregister_messenger(void);
cs_messenger cs_get_messenger(void);

// Logging functions
void cs_log_enable(void);
void cs_log_disable(void);
void cs_log_message(const char *module, const char *fmt, ...);

// TS Routing
unsigned int cs_get_ts_output(void);
int cs_set_ts_output(unsigned int port);

// Serial nr and revision accessors
unsigned long long cs_get_serial(void);
unsigned int cs_get_revision(void);

#endif //__CS_API_H_
