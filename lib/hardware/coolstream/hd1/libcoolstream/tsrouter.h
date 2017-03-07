/*******************************************************************************/
/*                                                                             */
/* control\tsrouter.h                                                          */
/*   Public header file for CoolStream Kernel TS Router API                    */
/*                                                                             */
/* (C) 2010 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __TSROUTER_H
#define __TSROUTER_H

#define HSDP_CI_DATA_OUT		2
#define HSDP_CI_DATA_IN			3
#define HSDP_SAT_PORT			4
#define HSDP_CAB_PORT			5

typedef struct _tsrouter_hsdp_config {
	u8 port;
	u32 port_ctrl;
	u32 pkt_ctrl;
	u32 clk_ctrl;
	u32 mux;
	u32 int_en;
	/* more ? */
} tsrouter_hsdp_config_t;

typedef struct _tsrouter_tsp_config {
	u8 port;
	u32 port_ctrl;
	u32 mux;
} tsrouter_tsp_config_t;

#ifdef __KERNEL__
extern int cs_tsrouter_init(void);
extern void cs_tsrouter_exit(void);

extern void cs_tsx_hsdp_init_port(u32 port, u32 ctrl);
extern void cs_tsx_tsp_init_port(u32 port, u32 hsdp_port);

extern void cs_tsx_hsdp_get_port_config(tsrouter_hsdp_config_t *conf);
extern void cs_tsx_tsp_get_port_config(tsrouter_tsp_config_t *conf);

extern void cs_tsx_hsdp_set_port_config(const tsrouter_hsdp_config_t *conf);
extern void cs_tsx_tsp_set_port_config(const tsrouter_tsp_config_t *conf);

extern void cs_tsx_hsdp_get_port_pll(unsigned int port, unsigned int *pll_index);
extern void cs_tsx_hsdp_set_port_pll(unsigned int port, unsigned int pll_index);

extern void cs_tsx_hsdp_get_port_speed(unsigned int port, unsigned int *speed);
extern void cs_tsx_hsdp_set_port_speed(unsigned int port, unsigned int speed);
#endif

#endif /* __TSROUTER_H */
