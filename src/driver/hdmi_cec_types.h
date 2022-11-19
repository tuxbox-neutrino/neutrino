#ifndef __HDMI_CEC_TYPES_H__
#define __HDMI_CEC_TYPES_H__

typedef enum cec_version
{
	CEC_OP_CEC_VERSION_1_3A = 4,
	CEC_OP_CEC_VERSION_1_4  = 5,
	CEC_OP_CEC_VERSION_2_0  = 6
} cec_version;

typedef enum cec_vendor_id
{
	CEC_VENDOR_TOSHIBA        = 0x000039,
	CEC_VENDOR_SAMSUNG        = 0x0000F0,
	CEC_VENDOR_DENON          = 0x0005CD,
	CEC_VENDOR_MARANTZ        = 0x000678,
	CEC_VENDOR_LOEWE          = 0x000982,
	CEC_VENDOR_ONKYO          = 0x0009B0,
	CEC_VENDOR_MEDION         = 0x000CB8,
	CEC_VENDOR_TOSHIBA2       = 0x000CE7,
	CEC_VENDOR_PULSE_EIGHT    = 0x001582,
	CEC_VENDOR_HARMAN_KARDON2 = 0x001950,
	CEC_VENDOR_GOOGLE         = 0x001A11,
	CEC_VENDOR_AKAI           = 0x0020C7,
	CEC_VENDOR_AOC            = 0x002467,
	CEC_VENDOR_PANASONIC      = 0x008045,
	CEC_VENDOR_PHILIPS        = 0x00903E,
	CEC_VENDOR_DAEWOO         = 0x009053,
	CEC_VENDOR_YAMAHA         = 0x00A0DE,
	CEC_VENDOR_GRUNDIG        = 0x00D0D5,
	CEC_VENDOR_PIONEER        = 0x00E036,
	CEC_VENDOR_LG             = 0x00E091,
	CEC_VENDOR_SHARP          = 0x08001F,
	CEC_VENDOR_SONY           = 0x080046,
	CEC_VENDOR_BROADCOM       = 0x18C086,
	CEC_VENDOR_SHARP2         = 0x534850,
	CEC_VENDOR_VIZIO          = 0x6B746D,
	CEC_VENDOR_BENQ           = 0x8065E9,
	CEC_VENDOR_HARMAN_KARDON  = 0x9C645E,
	CEC_VENDOR_UNKNOWN        = 0
} cec_vendor_id;

typedef enum cec_user_control_code
{
	CEC_USER_CONTROL_CODE_SELECT                      = 0x00,
	CEC_USER_CONTROL_CODE_UP                          = 0x01,
	CEC_USER_CONTROL_CODE_DOWN                        = 0x02,
	CEC_USER_CONTROL_CODE_LEFT                        = 0x03,
	CEC_USER_CONTROL_CODE_RIGHT                       = 0x04,
	CEC_USER_CONTROL_CODE_RIGHT_UP                    = 0x05,
	CEC_USER_CONTROL_CODE_RIGHT_DOWN                  = 0x06,
	CEC_USER_CONTROL_CODE_LEFT_UP                     = 0x07,
	CEC_USER_CONTROL_CODE_LEFT_DOWN                   = 0x08,
	CEC_USER_CONTROL_CODE_ROOT_MENU                   = 0x09,
	CEC_USER_CONTROL_CODE_SETUP_MENU                  = 0x0A,
	CEC_USER_CONTROL_CODE_CONTENTS_MENU               = 0x0B,
	CEC_USER_CONTROL_CODE_FAVORITE_MENU               = 0x0C,
	CEC_USER_CONTROL_CODE_EXIT                        = 0x0D,
	// reserved: 0x0E, 0x0F
	CEC_USER_CONTROL_CODE_TOP_MENU                    = 0x10,
	CEC_USER_CONTROL_CODE_DVD_MENU                    = 0x11,
	// reserved: 0x12 ... 0x1C
	CEC_USER_CONTROL_CODE_NUMBER_ENTRY_MODE           = 0x1D,
	CEC_USER_CONTROL_CODE_NUMBER11                    = 0x1E,
	CEC_USER_CONTROL_CODE_NUMBER12                    = 0x1F,
	CEC_USER_CONTROL_CODE_NUMBER0                     = 0x20,
	CEC_USER_CONTROL_CODE_NUMBER1                     = 0x21,
	CEC_USER_CONTROL_CODE_NUMBER2                     = 0x22,
	CEC_USER_CONTROL_CODE_NUMBER3                     = 0x23,
	CEC_USER_CONTROL_CODE_NUMBER4                     = 0x24,
	CEC_USER_CONTROL_CODE_NUMBER5                     = 0x25,
	CEC_USER_CONTROL_CODE_NUMBER6                     = 0x26,
	CEC_USER_CONTROL_CODE_NUMBER7                     = 0x27,
	CEC_USER_CONTROL_CODE_NUMBER8                     = 0x28,
	CEC_USER_CONTROL_CODE_NUMBER9                     = 0x29,
	CEC_USER_CONTROL_CODE_DOT                         = 0x2A,
	CEC_USER_CONTROL_CODE_ENTER                       = 0x2B,
	CEC_USER_CONTROL_CODE_CLEAR                       = 0x2C,
	CEC_USER_CONTROL_CODE_NEXT_FAVORITE               = 0x2F,
	CEC_USER_CONTROL_CODE_CHANNEL_UP                  = 0x30,
	CEC_USER_CONTROL_CODE_CHANNEL_DOWN                = 0x31,
	CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL            = 0x32,
	CEC_USER_CONTROL_CODE_SOUND_SELECT                = 0x33,
	CEC_USER_CONTROL_CODE_INPUT_SELECT                = 0x34,
	CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION         = 0x35,
	CEC_USER_CONTROL_CODE_HELP                        = 0x36,
	CEC_USER_CONTROL_CODE_PAGE_UP                     = 0x37,
	CEC_USER_CONTROL_CODE_PAGE_DOWN                   = 0x38,
	// reserved: 0x39 ... 0x3F
	CEC_USER_CONTROL_CODE_POWER                       = 0x40,
	CEC_USER_CONTROL_CODE_VOLUME_UP                   = 0x41,
	CEC_USER_CONTROL_CODE_VOLUME_DOWN                 = 0x42,
	CEC_USER_CONTROL_CODE_MUTE                        = 0x43,
	CEC_USER_CONTROL_CODE_PLAY                        = 0x44,
	CEC_USER_CONTROL_CODE_STOP                        = 0x45,
	CEC_USER_CONTROL_CODE_PAUSE                       = 0x46,
	CEC_USER_CONTROL_CODE_RECORD                      = 0x47,
	CEC_USER_CONTROL_CODE_REWIND                      = 0x48,
	CEC_USER_CONTROL_CODE_FAST_FORWARD                = 0x49,
	CEC_USER_CONTROL_CODE_EJECT                       = 0x4A,
	CEC_USER_CONTROL_CODE_FORWARD                     = 0x4B,
	CEC_USER_CONTROL_CODE_BACKWARD                    = 0x4C,
	CEC_USER_CONTROL_CODE_STOP_RECORD                 = 0x4D,
	CEC_USER_CONTROL_CODE_PAUSE_RECORD                = 0x4E,
	// reserved: 0x4F
	CEC_USER_CONTROL_CODE_ANGLE                       = 0x50,
	CEC_USER_CONTROL_CODE_SUB_PICTURE                 = 0x51,
	CEC_USER_CONTROL_CODE_VIDEO_ON_DEMAND             = 0x52,
	CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE    = 0x53,
	CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING           = 0x54,
	CEC_USER_CONTROL_CODE_INITIAL_CONFIGURATION       = 0x55,
	CEC_USER_CONTROL_CODE_SELECT_BROADCAST_TYPE       = 0x56,
	CEC_USER_CONTROL_CODE_SELECT_SOUND_PRESENTATION   = 0x57,
	// reserved: 0x58 ... 0x5F
	CEC_USER_CONTROL_CODE_PLAY_FUNCTION               = 0x60,
	CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION         = 0x61,
	CEC_USER_CONTROL_CODE_RECORD_FUNCTION             = 0x62,
	CEC_USER_CONTROL_CODE_PAUSE_RECORD_FUNCTION       = 0x63,
	CEC_USER_CONTROL_CODE_STOP_FUNCTION               = 0x64,
	CEC_USER_CONTROL_CODE_MUTE_FUNCTION               = 0x65,
	CEC_USER_CONTROL_CODE_RESTORE_VOLUME_FUNCTION     = 0x66,
	CEC_USER_CONTROL_CODE_TUNE_FUNCTION               = 0x67,
	CEC_USER_CONTROL_CODE_SELECT_MEDIA_FUNCTION       = 0x68,
	CEC_USER_CONTROL_CODE_SELECT_AV_INPUT_FUNCTION    = 0x69,
	CEC_USER_CONTROL_CODE_SELECT_AUDIO_INPUT_FUNCTION = 0x6A,
	CEC_USER_CONTROL_CODE_POWER_TOGGLE_FUNCTION       = 0x6B,
	CEC_USER_CONTROL_CODE_POWER_OFF_FUNCTION          = 0x6C,
	CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION           = 0x6D,
	// reserved: 0x6E ... 0x70
	CEC_USER_CONTROL_CODE_F1_BLUE                     = 0x71,
	CEC_USER_CONTROL_CODE_F2_RED                      = 0X72,
	CEC_USER_CONTROL_CODE_F3_GREEN                    = 0x73,
	CEC_USER_CONTROL_CODE_F4_YELLOW                   = 0x74,
	CEC_USER_CONTROL_CODE_F5                          = 0x75,
	CEC_USER_CONTROL_CODE_DATA                        = 0x76,
	// reserved: 0x77 ... 0xFF
	CEC_USER_CONTROL_CODE_AN_RETURN                   = 0x91, // return (Samsung)
	CEC_USER_CONTROL_CODE_AN_CHANNELS_LIST            = 0x96, // channels list (Samsung)
	CEC_USER_CONTROL_CODE_MAX                         = 0x96,
	CEC_USER_CONTROL_CODE_UNKNOWN                     = 0xFF
} cec_user_control_code;

typedef enum cec_opcode
{
	CEC_OPCODE_ACTIVE_SOURCE                 = 0x82,
	CEC_OPCODE_IMAGE_VIEW_ON                 = 0x04,
	CEC_OPCODE_TEXT_VIEW_ON                  = 0x0D,
	CEC_OPCODE_INACTIVE_SOURCE               = 0x9D,
	CEC_OPCODE_REQUEST_ACTIVE_SOURCE         = 0x85,
	CEC_OPCODE_ROUTING_CHANGE                = 0x80,
	CEC_OPCODE_ROUTING_INFORMATION           = 0x81,
	CEC_OPCODE_SET_STREAM_PATH               = 0x86,
	CEC_OPCODE_STANDBY                       = 0x36,
	CEC_OPCODE_RECORD_OFF                    = 0x0B,
	CEC_OPCODE_RECORD_ON                     = 0x09,
	CEC_OPCODE_RECORD_STATUS                 = 0x0A,
	CEC_OPCODE_RECORD_TV_SCREEN              = 0x0F,
	CEC_OPCODE_CLEAR_ANALOGUE_TIMER          = 0x33,
	CEC_OPCODE_CLEAR_DIGITAL_TIMER           = 0x99,
	CEC_OPCODE_CLEAR_EXTERNAL_TIMER          = 0xA1,
	CEC_OPCODE_SET_ANALOGUE_TIMER            = 0x34,
	CEC_OPCODE_SET_DIGITAL_TIMER             = 0x97,
	CEC_OPCODE_SET_EXTERNAL_TIMER            = 0xA2,
	CEC_OPCODE_SET_TIMER_PROGRAM_TITLE       = 0x67,
	CEC_OPCODE_TIMER_CLEARED_STATUS          = 0x43,
	CEC_OPCODE_TIMER_STATUS                  = 0x35,
	CEC_OPCODE_CEC_VERSION                   = 0x9E,
	CEC_OPCODE_GET_CEC_VERSION               = 0x9F,
	CEC_OPCODE_GIVE_PHYSICAL_ADDRESS         = 0x83,
	CEC_OPCODE_GET_MENU_LANGUAGE             = 0x91,
	CEC_OPCODE_REPORT_PHYSICAL_ADDRESS       = 0x84,
	CEC_OPCODE_SET_MENU_LANGUAGE             = 0x32,
	CEC_OPCODE_DECK_CONTROL                  = 0x42,
	CEC_OPCODE_DECK_STATUS                   = 0x1B,
	CEC_OPCODE_GIVE_DECK_STATUS              = 0x1A,
	CEC_OPCODE_PLAY                          = 0x41,
	CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS      = 0x08,
	CEC_OPCODE_SELECT_ANALOGUE_SERVICE       = 0x92,
	CEC_OPCODE_SELECT_DIGITAL_SERVICE        = 0x93,
	CEC_OPCODE_TUNER_DEVICE_STATUS           = 0x07,
	CEC_OPCODE_TUNER_STEP_DECREMENT          = 0x06,
	CEC_OPCODE_TUNER_STEP_INCREMENT          = 0x05,
	CEC_OPCODE_DEVICE_VENDOR_ID              = 0x87,
	CEC_OPCODE_GIVE_DEVICE_VENDOR_ID         = 0x8C,
	CEC_OPCODE_VENDOR_COMMAND                = 0x89,
	CEC_OPCODE_VENDOR_COMMAND_WITH_ID        = 0xA0,
	CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN     = 0x8A,
	CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP       = 0x8B,
	CEC_OPCODE_SET_OSD_STRING                = 0x64,
	CEC_OPCODE_GIVE_OSD_NAME                 = 0x46,
	CEC_OPCODE_SET_OSD_NAME                  = 0x47,
	CEC_OPCODE_MENU_REQUEST                  = 0x8D,
	CEC_OPCODE_MENU_STATUS                   = 0x8E,
	CEC_OPCODE_USER_CONTROL_PRESSED          = 0x44,
	CEC_OPCODE_USER_CONTROL_RELEASE          = 0x45,
	CEC_OPCODE_GIVE_DEVICE_POWER_STATUS      = 0x8F,
	CEC_OPCODE_REPORT_POWER_STATUS           = 0x90,
	CEC_OPCODE_FEATURE_ABORT                 = 0x00,
	CEC_OPCODE_ABORT                         = 0xFF,
	CEC_OPCODE_GIVE_AUDIO_STATUS             = 0x71,
	CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS = 0x7D,
	CEC_OPCODE_REPORT_AUDIO_STATUS           = 0x7A,
	CEC_OPCODE_SET_SYSTEM_AUDIO_MODE         = 0x72,
	CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST     = 0x70,
	CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS      = 0x7E,
	CEC_OPCODE_SET_AUDIO_RATE                = 0x9A,

	/* CEC 1.4 */
	CEC_OPCODE_START_ARC                     = 0xC0,
	CEC_OPCODE_REPORT_ARC_STARTED            = 0xC1,
	CEC_OPCODE_REPORT_ARC_ENDED              = 0xC2,
	CEC_OPCODE_REQUEST_ARC_START             = 0xC3,
	CEC_OPCODE_REQUEST_ARC_END               = 0xC4,
	CEC_OPCODE_END_ARC                       = 0xC5,
	CEC_OPCODE_CDC                           = 0xF8,
	/* when this opcode is set, no opcode will be sent to the device. this is one of the reserved numbers */
	CEC_OPCODE_NONE                          = 0xFD
} cec_opcode;

typedef enum cec_logical_address
{
	CECDEVICE_UNKNOWN          = -1, //not a valid logical address
	CECDEVICE_TV               = 0,
	CECDEVICE_RECORDINGDEVICE1 = 1,
	CECDEVICE_RECORDINGDEVICE2 = 2,
	CECDEVICE_TUNER1           = 3,
	CECDEVICE_PLAYBACKDEVICE1  = 4,
	CECDEVICE_AUDIOSYSTEM      = 5,
	CECDEVICE_TUNER2           = 6,
	CECDEVICE_TUNER3           = 7,
	CECDEVICE_PLAYBACKDEVICE2  = 8,
	CECDEVICE_RECORDINGDEVICE3 = 9,
	CECDEVICE_TUNER4           = 10,
	CECDEVICE_PLAYBACKDEVICE3  = 11,
	CECDEVICE_RESERVED1        = 12,
	CECDEVICE_RESERVED2        = 13,
	CECDEVICE_FREEUSE          = 14,
	CECDEVICE_UNREGISTERED     = 15,
	CECDEVICE_BROADCAST        = 15
} cec_logical_address;

typedef enum cec_power_status
{
	CEC_POWER_STATUS_ON                          = 0x00,
	CEC_POWER_STATUS_STANDBY                     = 0x01,
	CEC_POWER_STATUS_IN_TRANSITION_STANDBY_TO_ON = 0x02,
	CEC_POWER_STATUS_IN_TRANSITION_ON_TO_STANDBY = 0x03,
	CEC_POWER_STATUS_UNKNOWN                     = 0x99
} cec_power_status;

typedef	unsigned char cec_error_id;

static const char *ToString(const cec_opcode opcode)
{
	switch (opcode)
	{
		case CEC_OPCODE_ACTIVE_SOURCE:
			return "active source";

		case CEC_OPCODE_IMAGE_VIEW_ON:
			return "image view on";

		case CEC_OPCODE_TEXT_VIEW_ON:
			return "text view on";

		case CEC_OPCODE_INACTIVE_SOURCE:
			return "inactive source";

		case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
			return "request active source";

		case CEC_OPCODE_ROUTING_CHANGE:
			return "routing change";

		case CEC_OPCODE_ROUTING_INFORMATION:
			return "routing information";

		case CEC_OPCODE_SET_STREAM_PATH:
			return "set stream path";

		case CEC_OPCODE_STANDBY:
			return "standby";

		case CEC_OPCODE_RECORD_OFF:
			return "record off";

		case CEC_OPCODE_RECORD_ON:
			return "record on";

		case CEC_OPCODE_RECORD_STATUS:
			return "record status";

		case CEC_OPCODE_RECORD_TV_SCREEN:
			return "record tv screen";

		case CEC_OPCODE_CLEAR_ANALOGUE_TIMER:
			return "clear analogue timer";

		case CEC_OPCODE_CLEAR_DIGITAL_TIMER:
			return "clear digital timer";

		case CEC_OPCODE_CLEAR_EXTERNAL_TIMER:
			return "clear external timer";

		case CEC_OPCODE_SET_ANALOGUE_TIMER:
			return "set analogue timer";

		case CEC_OPCODE_SET_DIGITAL_TIMER:
			return "set digital timer";

		case CEC_OPCODE_SET_EXTERNAL_TIMER:
			return "set external timer";

		case CEC_OPCODE_SET_TIMER_PROGRAM_TITLE:
			return "set timer program title";

		case CEC_OPCODE_TIMER_CLEARED_STATUS:
			return "timer cleared status";

		case CEC_OPCODE_TIMER_STATUS:
			return "timer status";

		case CEC_OPCODE_CEC_VERSION:
			return "cec version";

		case CEC_OPCODE_GET_CEC_VERSION:
			return "get cec version";

		case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
			return "give physical address";

		case CEC_OPCODE_GET_MENU_LANGUAGE:
			return "get menu language";

		case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:
			return "report physical address";

		case CEC_OPCODE_SET_MENU_LANGUAGE:
			return "set menu language";

		case CEC_OPCODE_DECK_CONTROL:
			return "deck control";

		case CEC_OPCODE_DECK_STATUS:
			return "deck status";

		case CEC_OPCODE_GIVE_DECK_STATUS:
			return "give deck status";

		case CEC_OPCODE_PLAY:
			return "play";

		case CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS:
			return "give tuner status";

		case CEC_OPCODE_SELECT_ANALOGUE_SERVICE:
			return "select analogue service";

		case CEC_OPCODE_SELECT_DIGITAL_SERVICE:
			return "set digital service";

		case CEC_OPCODE_TUNER_DEVICE_STATUS:
			return "tuner device status";

		case CEC_OPCODE_TUNER_STEP_DECREMENT:
			return "tuner step decrement";

		case CEC_OPCODE_TUNER_STEP_INCREMENT:
			return "tuner step increment";

		case CEC_OPCODE_DEVICE_VENDOR_ID:
			return "device vendor id";

		case CEC_OPCODE_GIVE_DEVICE_VENDOR_ID:
			return "give device vendor id";

		case CEC_OPCODE_VENDOR_COMMAND:
			return "vendor command";

		case CEC_OPCODE_VENDOR_COMMAND_WITH_ID:
			return "vendor command with id";

		case CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN:
			return "vendor remote button down";

		case CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP:
			return "vendor remote button up";

		case CEC_OPCODE_SET_OSD_STRING:
			return "set osd string";

		case CEC_OPCODE_GIVE_OSD_NAME:
			return "give osd name";

		case CEC_OPCODE_SET_OSD_NAME:
			return "set osd name";

		case CEC_OPCODE_MENU_REQUEST:
			return "menu request";

		case CEC_OPCODE_MENU_STATUS:
			return "menu status";

		case CEC_OPCODE_USER_CONTROL_PRESSED:
			return "user control pressed";

		case CEC_OPCODE_USER_CONTROL_RELEASE:
			return "user control release";

		case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
			return "give device power status";

		case CEC_OPCODE_REPORT_POWER_STATUS:
			return "report power status";

		case CEC_OPCODE_FEATURE_ABORT:
			return "feature abort";

		case CEC_OPCODE_ABORT:
			return "abort";

		case CEC_OPCODE_GIVE_AUDIO_STATUS:
			return "give audio status";

		case CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS:
			return "give audio mode status";

		case CEC_OPCODE_REPORT_AUDIO_STATUS:
			return "report audio status";

		case CEC_OPCODE_SET_SYSTEM_AUDIO_MODE:
			return "set system audio mode";

		case CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST:
			return "system audio mode request";

		case CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS:
			return "system audio mode status";

		case CEC_OPCODE_SET_AUDIO_RATE:
			return "set audio rate";

		case CEC_OPCODE_START_ARC:
			return "start ARC";

		case CEC_OPCODE_REPORT_ARC_STARTED:
			return "report ARC started";

		case CEC_OPCODE_REPORT_ARC_ENDED:
			return "report ARC ended";

		case CEC_OPCODE_REQUEST_ARC_START:
			return "request ARC start";

		case CEC_OPCODE_REQUEST_ARC_END:
			return "request ARC end";

		case CEC_OPCODE_END_ARC:
			return "end ARC";

		case CEC_OPCODE_CDC:
			return "CDC";

		case CEC_OPCODE_NONE:
			return "poll";

		default:
			return "UNKNOWN";
	}
}

static const char *ToString(const cec_vendor_id vendor)
{
	switch (vendor)
	{
		case CEC_VENDOR_SAMSUNG:
			return "Samsung";

		case CEC_VENDOR_LG:
			return "LG";

		case CEC_VENDOR_PANASONIC:
			return "Panasonic";

		case CEC_VENDOR_PIONEER:
			return "Pioneer";

		case CEC_VENDOR_ONKYO:
			return "Onkyo";

		case CEC_VENDOR_YAMAHA:
			return "Yamaha";

		case CEC_VENDOR_PHILIPS:
			return "Philips";

		case CEC_VENDOR_SONY:
			return "Sony";

		case CEC_VENDOR_TOSHIBA:
		case CEC_VENDOR_TOSHIBA2:
			return "Toshiba";

		case CEC_VENDOR_AKAI:
			return "Akai";

		case CEC_VENDOR_AOC:
			return "AOC";

		case CEC_VENDOR_BENQ:
			return "Benq";

		case CEC_VENDOR_DAEWOO:
			return "Daewoo";

		case CEC_VENDOR_GRUNDIG:
			return "Grundig";

		case CEC_VENDOR_MEDION:
			return "Medion";

		case CEC_VENDOR_SHARP:
		case CEC_VENDOR_SHARP2:
			return "Sharp";

		case CEC_VENDOR_VIZIO:
			return "Vizio";

		case CEC_VENDOR_BROADCOM:
			return "Broadcom";

		case CEC_VENDOR_LOEWE:
			return "Loewe";

		case CEC_VENDOR_DENON:
			return "Denon";

		case CEC_VENDOR_MARANTZ:
			return "Marantz";

		case CEC_VENDOR_HARMAN_KARDON:
		case CEC_VENDOR_HARMAN_KARDON2:
			return "Harman/Kardon";

		case CEC_VENDOR_PULSE_EIGHT:
			return "Pulse Eight";

		case CEC_VENDOR_GOOGLE:
			return "Google";

		default:
			return "Unknown";
	}
}

static const char *ToString(const cec_user_control_code key)
{
	switch (key)
	{
		case CEC_USER_CONTROL_CODE_SELECT:
			return "select";

		case CEC_USER_CONTROL_CODE_UP:
			return "up";

		case CEC_USER_CONTROL_CODE_DOWN:
			return "down";

		case CEC_USER_CONTROL_CODE_LEFT:
			return "left";

		case CEC_USER_CONTROL_CODE_RIGHT:
			return "right";

		case CEC_USER_CONTROL_CODE_RIGHT_UP:
			return "right+up";

		case CEC_USER_CONTROL_CODE_RIGHT_DOWN:
			return "right+down";

		case CEC_USER_CONTROL_CODE_LEFT_UP:
			return "left+up";

		case CEC_USER_CONTROL_CODE_LEFT_DOWN:
			return "left+down";

		case CEC_USER_CONTROL_CODE_ROOT_MENU:
			return "root menu";

		case CEC_USER_CONTROL_CODE_SETUP_MENU:
			return "setup menu";

		case CEC_USER_CONTROL_CODE_CONTENTS_MENU:
			return "contents menu";

		case CEC_USER_CONTROL_CODE_FAVORITE_MENU:
			return "favourite menu";

		case CEC_USER_CONTROL_CODE_EXIT:
			return "exit";

		case CEC_USER_CONTROL_CODE_TOP_MENU:
			return "top menu";

		case CEC_USER_CONTROL_CODE_DVD_MENU:
			return "dvd menu";

		case CEC_USER_CONTROL_CODE_NUMBER_ENTRY_MODE:
			return "number entry mode";

		case CEC_USER_CONTROL_CODE_NUMBER11:
			return "11";

		case CEC_USER_CONTROL_CODE_NUMBER12:
			return "12";

		case CEC_USER_CONTROL_CODE_NUMBER0:
			return "0";

		case CEC_USER_CONTROL_CODE_NUMBER1:
			return "1";

		case CEC_USER_CONTROL_CODE_NUMBER2:
			return "2";

		case CEC_USER_CONTROL_CODE_NUMBER3:
			return "3";

		case CEC_USER_CONTROL_CODE_NUMBER4:
			return "4";

		case CEC_USER_CONTROL_CODE_NUMBER5:
			return "5";

		case CEC_USER_CONTROL_CODE_NUMBER6:
			return "6";

		case CEC_USER_CONTROL_CODE_NUMBER7:
			return "7";

		case CEC_USER_CONTROL_CODE_NUMBER8:
			return "8";

		case CEC_USER_CONTROL_CODE_NUMBER9:
			return "9";

		case CEC_USER_CONTROL_CODE_DOT:
			return ".";

		case CEC_USER_CONTROL_CODE_ENTER:
			return "enter";

		case CEC_USER_CONTROL_CODE_CLEAR:
			return "clear";

		case CEC_USER_CONTROL_CODE_NEXT_FAVORITE:
			return "next favourite";

		case CEC_USER_CONTROL_CODE_CHANNEL_UP:
			return "channel up";

		case CEC_USER_CONTROL_CODE_CHANNEL_DOWN:
			return "channel down";

		case CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL:
			return "previous channel";

		case CEC_USER_CONTROL_CODE_SOUND_SELECT:
			return "sound select";

		case CEC_USER_CONTROL_CODE_INPUT_SELECT:
			return "input select";

		case CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION:
			return "display information";

		case CEC_USER_CONTROL_CODE_HELP:
			return "help";

		case CEC_USER_CONTROL_CODE_PAGE_UP:
			return "page up";

		case CEC_USER_CONTROL_CODE_PAGE_DOWN:
			return "page down";

		case CEC_USER_CONTROL_CODE_POWER:
			return "power";

		case CEC_USER_CONTROL_CODE_VOLUME_UP:
			return "volume up";

		case CEC_USER_CONTROL_CODE_VOLUME_DOWN:
			return "volume down";

		case CEC_USER_CONTROL_CODE_MUTE:
			return "mute";

		case CEC_USER_CONTROL_CODE_PLAY:
			return "play";

		case CEC_USER_CONTROL_CODE_STOP:
			return "stop";

		case CEC_USER_CONTROL_CODE_PAUSE:
			return "pause";

		case CEC_USER_CONTROL_CODE_RECORD:
			return "record";

		case CEC_USER_CONTROL_CODE_REWIND:
			return "rewind";

		case CEC_USER_CONTROL_CODE_FAST_FORWARD:
			return "Fast forward";

		case CEC_USER_CONTROL_CODE_EJECT:
			return "eject";

		case CEC_USER_CONTROL_CODE_FORWARD:
			return "forward";

		case CEC_USER_CONTROL_CODE_BACKWARD:
			return "backward";

		case CEC_USER_CONTROL_CODE_STOP_RECORD:
			return "stop record";

		case CEC_USER_CONTROL_CODE_PAUSE_RECORD:
			return "pause record";

		case CEC_USER_CONTROL_CODE_ANGLE:
			return "angle";

		case CEC_USER_CONTROL_CODE_SUB_PICTURE:
			return "sub picture";

		case CEC_USER_CONTROL_CODE_VIDEO_ON_DEMAND:
			return "video on demand";

		case CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE:
			return "electronic program guide";

		case CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING:
			return "timer programming";

		case CEC_USER_CONTROL_CODE_INITIAL_CONFIGURATION:
			return "initial configuration";

		case CEC_USER_CONTROL_CODE_SELECT_BROADCAST_TYPE:
			return "select broadcast type";

		case CEC_USER_CONTROL_CODE_SELECT_SOUND_PRESENTATION:
			return "select sound presentation";

		case CEC_USER_CONTROL_CODE_PLAY_FUNCTION:
			return "play (function)";

		case CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION:
			return "pause play (function)";

		case CEC_USER_CONTROL_CODE_RECORD_FUNCTION:
			return "record (function)";

		case CEC_USER_CONTROL_CODE_PAUSE_RECORD_FUNCTION:
			return "pause record (function)";

		case CEC_USER_CONTROL_CODE_STOP_FUNCTION:
			return "stop (function)";

		case CEC_USER_CONTROL_CODE_MUTE_FUNCTION:
			return "mute (function)";

		case CEC_USER_CONTROL_CODE_RESTORE_VOLUME_FUNCTION:
			return "restore volume";

		case CEC_USER_CONTROL_CODE_TUNE_FUNCTION:
			return "tune";

		case CEC_USER_CONTROL_CODE_SELECT_MEDIA_FUNCTION:
			return "select media";

		case CEC_USER_CONTROL_CODE_SELECT_AV_INPUT_FUNCTION:
			return "select AV input";

		case CEC_USER_CONTROL_CODE_SELECT_AUDIO_INPUT_FUNCTION:
			return "select audio input";

		case CEC_USER_CONTROL_CODE_POWER_TOGGLE_FUNCTION:
			return "power toggle";

		case CEC_USER_CONTROL_CODE_POWER_OFF_FUNCTION:
			return "power off";

		case CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION:
			return "power on";

		case CEC_USER_CONTROL_CODE_F1_BLUE:
			return "F1 (blue)";

		case CEC_USER_CONTROL_CODE_F2_RED:
			return "F2 (red)";

		case CEC_USER_CONTROL_CODE_F3_GREEN:
			return "F3 (green)";

		case CEC_USER_CONTROL_CODE_F4_YELLOW:
			return "F4 (yellow)";

		case CEC_USER_CONTROL_CODE_F5:
			return "F5";

		case CEC_USER_CONTROL_CODE_DATA:
			return "data";

		case CEC_USER_CONTROL_CODE_AN_RETURN:
			return "return (Samsung)";

		case CEC_USER_CONTROL_CODE_AN_CHANNELS_LIST:
			return "channels list (Samsung)";

		default:
			return "unknown";
	}
}

static const char *ToString(cec_logical_address la)
{
	switch (la & 0xf)
	{
		case CECDEVICE_TV:
			return "TV";

		case CECDEVICE_RECORDINGDEVICE1:
			return "Recording Device 1";

		case CECDEVICE_RECORDINGDEVICE2:
			return "Recording Device 2";

		case CECDEVICE_TUNER1:
			return "Tuner 1";

		case CECDEVICE_PLAYBACKDEVICE1:
			return "Playback Device 1";

		case CECDEVICE_AUDIOSYSTEM:
			return "Audio System";

		case CECDEVICE_TUNER2:
			return "Tuner 2";

		case CECDEVICE_TUNER3:
			return "Tuner 3";

		case CECDEVICE_PLAYBACKDEVICE2:
			return "Playback Device 2";

		case CECDEVICE_RECORDINGDEVICE3:
			return "Recording Device 3";

		case CECDEVICE_TUNER4:
			return "Tuner 4";

		case CECDEVICE_PLAYBACKDEVICE3:
			return "Playback Device 3";

		case CECDEVICE_RESERVED1:
			return "Reserved 1";

		case CECDEVICE_RESERVED2:
			return "Reserved 2";

		case CECDEVICE_FREEUSE:
			return "Free use";

		case CECDEVICE_UNREGISTERED:
		default:
			return "Unregistered";
	}
}

static const char *ToString(cec_error_id reason)
{
	switch (reason & 0x0f)
	{
		case 0x00:
			return "unrecognized opcode";

		case 0x01:
			return "not in correct mode to response";

		case 0x02:
			return "cannot provide source";

		case 0x03:
			return "invalid operand";

		case 0x04:
		default:
			return "refused";
	}
}

static cec_opcode GetResponseOpcode(cec_opcode opcode)
{
	switch (opcode)
	{
		case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
			return CEC_OPCODE_ACTIVE_SOURCE;

		case CEC_OPCODE_GET_CEC_VERSION:
			return CEC_OPCODE_CEC_VERSION;

		case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
			return CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;

		case CEC_OPCODE_GET_MENU_LANGUAGE:
			return CEC_OPCODE_SET_MENU_LANGUAGE;

		case CEC_OPCODE_GIVE_DECK_STATUS:
			return CEC_OPCODE_DECK_STATUS;

		case CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS:
			return CEC_OPCODE_TUNER_DEVICE_STATUS;

		case CEC_OPCODE_GIVE_DEVICE_VENDOR_ID:
			return CEC_OPCODE_DEVICE_VENDOR_ID;

		case CEC_OPCODE_GIVE_OSD_NAME:
			return CEC_OPCODE_SET_OSD_NAME;

		case CEC_OPCODE_MENU_REQUEST:
			return CEC_OPCODE_MENU_STATUS;

		case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
			return CEC_OPCODE_REPORT_POWER_STATUS;

		case CEC_OPCODE_GIVE_AUDIO_STATUS:
			return CEC_OPCODE_REPORT_AUDIO_STATUS;

		case CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS:
			return CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS;

		case CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST:
			return CEC_OPCODE_SET_SYSTEM_AUDIO_MODE;

		default:
			break;
	}

	return CEC_OPCODE_NONE;
}

#endif // __HDMI_CEC_TYPES_H__
