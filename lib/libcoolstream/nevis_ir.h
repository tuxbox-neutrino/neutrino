/* 
 * public definitions for the CX2450x Infrared receiver driver
 *
 * Copyright (C) 2008-2011 Coolstream International Limited
 *
 */

#ifndef __NEVIS_IR_H__
#define __NEVIS_IR_H__

typedef enum
{
    IR_PROTOCOL_UNKNOWN    = 0x00000,
    IR_PROTOCOL_RMAP       = 0x00001,   /* Ruwido rMAP */
    IR_PROTOCOL_RMAP_E     = 0x00002,   /* Ruwido rMAP with extension for MNC Ltd sp. z o.o. */
    IR_PROTOCOL_NRC17      = 0x00004,   /* Nokia NRC17 */
    IR_PROTOCOL_JVC        = 0x00008,   /* JVC */
    IR_PROTOCOL_RCA        = 0x00010,   /* RCA */
    IR_PROTOCOL_PSD        = 0x00020,   /* Precision Squared (not yet supported) */
    IR_PROTOCOL_RC5        = 0x00040,   /* Philips RC5 */
    IR_PROTOCOL_RCMM       = 0x00080,   /* Philips RCMM */
    IR_PROTOCOL_RECS80     = 0x00100,   /* Philips RECS80 */
    IR_PROTOCOL_NEC        = 0x00200,   /* NEC */
    IR_PROTOCOL_NECE       = 0x00400,   /* NEC with 16 bit address capability */
    IR_PROTOCOL_SCA        = 0x00800,   /* Scientific Atlanta */
    IR_PROTOCOL_MATSUSHITA = 0x01000,   /* Matsushita (Technics/Panasonics) */
    IR_PROTOCOL_SONY       = 0x02000,   /* Sony SIRC 12 bit */
    IR_PROTOCOL_SONY15     = 0x04000,   /* Sony SIRC 15 bit */
    IR_PROTOCOL_SONY20     = 0x08000,   /* Sony SIRC 20 bit */
    IR_PROTOCOL_SONY24     = 0x10000,   /* Sony SIRC 24 bit */
    IR_PROTOCOL_ALL        = 0x1FFFF
} ir_protocol_t;

typedef enum
{
    FP_MODE_KEYS_DISABLED = 0,
    FP_MODE_KEYS_ENABLED  = 1
} fp_mode_t;

#define EVENT_KEY_UP			0
#define EVENT_KEY_DOWN			1
#define NEVIS_IR_DEVICE_NAME		"IR_NEVIS"

/*******************************************************************************/
/* DEBUG options                                                               */
/*******************************************************************************/

#define DBG_NONE		0x00000000				/* no debug at all */

#define DBG_IR_SYSTEM		0x00000001				/* IR: low level informations */
#define DBG_IR_DECODE		0x00000002				/* IR: informations from the IR protocol decoder */
#define DBG_OUTPUT_QUEUE	0x00000004				/* IR: show data from the outgoing queue (from driver to the client(s) ) */
#define DBG_IR_FOPS		0x00000008				/* IR: informations about filoe operations (ioctl's) */

#define DBG_VFD_SYSTEM		0x00010000				/* VFD: low level informations */
#define DBG_VFD_FOPS		0x00080000				/* VFD: informations about filoe operations (ioctl's) */

/*******************************************************************************/
/* ioctl's                                                                     */
/*******************************************************************************/

/* set the IR-protocols to listen for. */
#define IOC_IR_SET_PRI_PROTOCOL _IOW(0xDD,  1, ir_protocol_t)		/* set the primary IR-protocol */
#define IOC_IR_SET_SEC_PROTOCOL _IOW(0xDD,  2, ir_protocol_t)		/* set the secondary IR-protocol */

/* some IR-protocols can handle different device addresses. */
#define IOC_IR_SET_PRI_ADDRESS  _IOW(0xDD,  3, unsigned int)		/* set the primary IR-address */
#define IOC_IR_SET_SEC_ADDRESS  _IOW(0xDD,  4, unsigned int)		/* set the secondary IR-address */

/* defines the delay time between two pulses in milliseconds */
#define IOC_IR_SET_F_DELAY      _IOW(0xDD,  5, unsigned int)		/* set the delay time before the first repetition */
#define IOC_IR_SET_X_DELAY      _IOW(0xDD,  6, unsigned int)		/* set the delay time between all other repetitions */

/* load key mappings to translate from raw IR to Linux Input */
#define IOC_IR_SET_PRI_KEYMAP   _IOW(0xDD,  7, unsigned short *)	/* set the primary keymap [num entries],[entry 1],[entry 2],...,[entry n] (max 0xFFFF entries) */
#define IOC_IR_SET_SEC_KEYMAP   _IOW(0xDD,  8, unsigned short *)	/* set the secondary keymap [num entries],[entry 1],[entry 2],...,[entry n] (max 0xFFFF entries) */

/* frontpanel button options */
#define IOC_IR_SET_FP_MODE      _IOW(0xDD,  9, fp_mode_t)		/* enable/disable frontpanel buttons */

/* informative stuff */
#define IOC_IR_GET_PROTOCOLS	_IOR(0xDD, 10, ir_protocol_t)		/* reports a bitmask of all supported ir_protocols */

#endif /* __NEVIS_IR_H__ */
