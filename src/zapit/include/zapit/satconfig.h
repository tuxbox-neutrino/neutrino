#ifndef _SAT_CONFIG_H_
#define _SAT_CONFIG_H_

typedef struct sat_config {
	int diseqc;
	int commited;
	int uncommited;
	int motor_position;
	int diseqc_order;
	int lnbOffsetLow;
	int lnbOffsetHigh;
	int lnbSwitch;
	int use_in_scan;
	int use_usals;
	std::string name;
	int have_channels;
	t_satellite_position position;
	int input;
} sat_config_t;

typedef enum diseqc_cmd_order {
	UNCOMMITED_FIRST,
	COMMITED_FIRST
} diseqc_cmd_order_t;

typedef std::map<t_satellite_position, sat_config_t> satellite_map_t;
typedef std::map<t_satellite_position, sat_config_t>::iterator sat_iterator_t;

typedef std::map <int, std::string> scan_list_t;
typedef std::map <int, std::string>::iterator scan_list_iterator_t;

extern satellite_map_t satellitePositions;
extern scan_list_t scanProviders;
#endif
