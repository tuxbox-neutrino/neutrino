/*
 * Settings Registry — Central field definitions for settings.html, read
 * once the texts are in, since every label is stated as the file loads
 * Each category has: id, label, sections with fields
 * Field types: select, bool, bool01, number, text, password,
 *   info   { label, text | key }                    — readonly display
 *   action { id, label, button, confirm?, success?, refresh?,
 *            api: {script, args?} }                 — an extension's button:
 *            runs a script its manifest names (settings.js)
 * Optional: showIf: {key, value} for conditional visibility
 * v1section: the section of GET /api/v1/settings/sections this category
 *   goes into when the box declares it (settings-v1.js); its fields the
 *   schema lacks stay here and are drawn below that section's rows
 */
// The groups of the menu, in the order it shows them (Phase 4 C, as the
// acceptance asked): the web interface and its server first, then what
// Neutrino does, the system, picture and sound, the displays, the network,
// the extensions. Each registry category names its group (group:'<id>');
// an extension's category (EXT_SETTINGS_CATS) goes under Extensions.
var SETTINGS_GROUPS = [
	{ id: 'web',      label: webuiL('set.grp_web', 'Web UI and web server') },
	{ id: 'neutrino', label: webuiL('set.grp_neutrino', 'Neutrino') },
	{ id: 'sys',      label: webuiL('set.grp_sys', 'System') },
	{ id: 'av',       label: webuiL('set.grp_av', 'Picture & Sound') },
	{ id: 'disp',     label: webuiL('set.grp_disp', 'Displays') },
	{ id: 'net',      label: webuiL('set.grp_net', 'Network & Sources') },
	{ id: 'ext',      label: webuiL('set.grp_ext', 'Extensions') }
];

// The order of the categories within their group, the box's sections and
// the registry's alike; one not named here follows in the order it comes.
var SETTINGS_ORDER = ['webui', 'nhttpd',
	'general', 'osd', 'channel', 'player', 'recording', 'parental', 'personalize', 'keybindings', 'buttons', 'misc', 'system',
	'hdd', 'update', 'backup', 'zapitupload', 'cam',
	'video', 'audio', 'cec',
	'display', 'lcd', 'glcd',
	'network', 'weather', 'wol', 'xmltv', 'mounts',
	'plugins', 'extensions'];

// Personalisierung — die Wertedomaenen aus src/gui/personalize.h:185-231.
// Welche Domaene ein Eintrag hat, entscheidet in Neutrino das item_mode-
// Argument von personalize.addItem(): SHOW_AS_ITEM_OPTION -> MODE,
// SHOW_AS_ACCESS_OPTION -> ACCESS. Jeder Key deklariert sie hier genau einmal.
var PERS_MODE_OPTS = [                                  // PERSONALIZE_MODE
	{ v: '0', l: webuiL('pers.notvisible', 'Not Visible') },
	{ v: '1', l: webuiL('pers.visible', 'Visible') },
	{ v: '2', l: webuiL('pers.pin', 'PIN') }
];
var PERS_KEY_OPTS = [                                   // PERSONALIZE_FEAT_KEY
	{ v: '0', l: webuiL('pers.col_red', 'red') },
	{ v: '1', l: webuiL('pers.col_green', 'green') },
	{ v: '2', l: webuiL('pers.col_yellow', 'yellow') },
	{ v: '3', l: webuiL('pers.col_blue', 'blue') },
	{ v: '4', l: webuiL('pers.col_auto', 'auto') }
];

// MODE-Eintrag; dep = Key des Observers, der diesen Eintrag sperrt, sobald
// er selbst sichtbar oder PIN-geschuetzt ist (personalize.cpp:604-616)
function persMode(key, label, dep) {
	var f = { key: 'personalize_' + key, label: label, type: 'select', options: PERS_MODE_OPTS };
	if (dep)
		f.disabledIf = { key: 'personalize_' + dep, unless: '0' };
	return f;
}
// ACCESS-Eintrag: immer sichtbar, die Option steuert nur die PIN-Abfrage.
// Beschriftung wie in Neutrino: '<Eintrag> mit PIN' (personalize.cpp:626-628)
function persAccess(key, label) {
	return { key: 'personalize_' + key, type: 'access',
		 label: label + ' ' + webuiL('pers.pinstatus', 'Require PIN') };
}
function persBool(key, label) {                         // PERSONALIZE_ACTIVE_MODE
	return { key: 'personalize_' + key, label: label, type: 'bool01' };
}
function persFeatKey(key, label) {
	return { key: 'personalize_feat_key_' + key, label: label,
		 type: 'select', options: PERS_KEY_OPTS };
}

// 13x13-Sidebar-Icons (feather-artig, stroke=currentColor erbt den
// Button-Zustand muted/aktiv)
function regIcon(paths) {
	return '<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' + paths + '</svg>';
}
// Fallback fuer Kategorien ohne eigenes Icon (Extension-Kategorien)
var SETTINGS_DEFAULT_ICON = regIcon('<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/>');

var SETTINGS_REGISTRY = [
	{
		id: 'video',
		v1section: 'video',
		group: 'av',
		icon: regIcon('<polygon points="23 7 16 12 23 17 23 7"/><rect x="1" y="5" width="15" height="14" rx="2" ry="2"/>'),
		label: 'Video',
		sections: [
			{
				title: webuiL('reg.video_format', 'Video format'),
				fields: [
					{ key: 'video_Format', label: webuiL('reg.video_format', 'Video format'), type: 'select',
					  options: [{v:'0',l:'Auto'},{v:'1',l:'16:9'},{v:'2',l:'4:3 (LB)'},{v:'3',l:'4:3 (PS)'}] },
					{ key: 'video_43mode', label: webuiL('reg.mode43', '4:3 mode'), type: 'select',
					  options: [{v:'0',l:'Panscan'},{v:'1',l:'Panscan2'},{v:'2',l:'Letterbox'},{v:'3',l: webuiL('reg.fullscreen', 'Full screen')}] },
					{ key: 'vcr_AutoSwitch', label: webuiL('reg.vcr_switch', 'VCR auto switch'), type: 'bool' }
				]
			},
			{
				title: webuiL('reg.output', 'Output'),
				fields: [
					{ key: 'video_dbdr', label: 'Deblocking/Deringing', type: 'select',
					  options: [{v:'0',l: webuiL('reg.off', 'Off')},{v:'1',l:'Deblock'},{v:'2',l: webuiL('reg.both', 'Both')}] },
					{ key: 'zappingmode', label: webuiL('reg.zap_mode', 'Zapping mode'), type: 'select',
					  options: [{v:'0',l: webuiL('reg.mute', 'Mute')},{v:'1',l: webuiL('reg.hold_picture', 'Hold picture')},{v:'2',l: webuiL('reg.mute_until_lock', 'Mute until lock')},{v:'3',l: webuiL('reg.hold_until_lock', 'Hold until lock')}] },
					{ key: 'hdmi_colorimetry', label: webuiL('reg.hdmi_colorspace', 'HDMI color space'), type: 'select',
					  options: [{v:'0',l:'Auto'},{v:'1',l:'BT.709'},{v:'2',l:'BT.2020'}] }
				]
			}
		]
	},
	{
		id: 'audio',
		v1section: 'audio',
		group: 'av',
		icon: regIcon('<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><path d="M15.54 8.46a5 5 0 0 1 0 7.07"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14"/>'),
		label: 'Audio',
		sections: [
			{
				title: webuiL('reg.analog_out', 'Analog output'),
				fields: [
					{ key: 'audio_AnalogMode', label: webuiL('reg.analog_mode', 'Analog mode'), type: 'select',
					  options: [{v:'0',l:'Stereo'},{v:'1',l: webuiL('reg.mono_left', 'Mono left')},{v:'2',l: webuiL('reg.mono_right', 'Mono right')}] },
					{ key: 'audio_DolbyDigital', label: 'Dolby Digital', type: 'bool01' }
				]
			},
			{
				title: webuiL('reg.digital_pass', 'Digital passthrough'),
				fields: [
					{ key: 'ac3_pass', label: 'AC3 Passthrough', type: 'bool01' },
					{ key: 'dts_pass', label: 'DTS Passthrough', type: 'bool01' },
					{ key: 'hdmi_dd', label: 'HDMI Dolby Digital', type: 'select',
					  options: [{v:'0',l: webuiL('reg.off', 'Off')},{v:'1',l:'Auto'},{v:'2',l: webuiL('reg.force', 'Force')}] },
					{ key: 'spdif_dd', label: 'SPDIF Dolby Digital', type: 'bool01' }
				]
			},
			{
				title: webuiL('reg.av_sync', 'A/V synchronization'),
				fields: [
					{ key: 'avsync', label: 'A/V-Sync', type: 'select',
					  options: [{v:'0',l: webuiL('reg.off', 'Off')},{v:'1',l: webuiL('reg.on', 'On')},{v:'2',l:'Audio Master'}] }
				]
			},
			{
				title: webuiL('reg.volume', 'Volume'),
				fields: [
					{ key: 'current_volume_step', label: webuiL('reg.volume_step', 'Volume step'), type: 'number', min: 1, max: 25 },
					{ key: 'start_volume', label: webuiL('reg.start_volume', 'Start volume'), type: 'number', min: -1, max: 100, hint: webuiL('reg.start_volume_hint', '-1 keeps the last volume') },
					{ key: 'audio_volume_percent_ac3', label: webuiL('reg.ac3_vol', 'AC3 volume (%)'), type: 'number', min: 0, max: 100 },
					{ key: 'audio_volume_percent_pcm', label: webuiL('reg.pcm_vol', 'PCM volume (%)'), type: 'number', min: 0, max: 100 },
					{ key: 'audiochannel_up_down_enable', label: webuiL('reg.audio_ch_selectable', 'Audio channel selectable'), type: 'bool' },
					{ key: 'audio_left_right_selectable', label: webuiL('reg.lr_selectable', 'Left/right selectable'), type: 'bool' }
				]
			},
			{
				title: webuiL('reg.srs', 'SRS TruVolume'),
				fields: [
					{ key: 'srs_enable', label: webuiL('reg.srs_enable', 'SRS enabled'), type: 'bool01' },
					{ key: 'srs_algo', label: webuiL('reg.srs_algo', 'SRS mode'), type: 'select',
					  options: [{v:'0',l: webuiL('reg.srs_light', 'light')},{v:'1',l: webuiL('reg.srs_normal', 'normal')},{v:'2',l: webuiL('reg.srs_heavy', 'heavy')}] },
					{ key: 'srs_ref_volume', label: webuiL('reg.srs_ref_volume', 'Reference volume'), type: 'number', min: 0, max: 100 },
					{ key: 'srs_nmgr_enable', label: webuiL('reg.srs_nmgr', 'Volume manager'), type: 'bool01' }
				]
			}
		]
	},
	{
		id: 'lcd',
		v1section: 'display',
		group: 'disp',
		icon: regIcon('<rect x="5" y="2" width="14" height="20" rx="2" ry="2"/><line x1="12" y1="18" x2="12.01" y2="18"/>'),
		label: 'LCD',
		sections: [
			{
				title: 'LCD Display',
				fields: [
					{ key: 'lcd_power', label: webuiL('reg.lcd_power', 'LCD on/off'), type: 'bool01' },
					{ key: 'lcd_inverse', label: webuiL('reg.lcd_invert', 'Invert the LCD'), type: 'bool01' },
					{ key: 'lcd_brightness', label: webuiL('reg.brightness', 'Brightness'), type: 'number', min: 0, max: 255 },
					{ key: 'lcd_standbybrightness', label: webuiL('reg.standby_brightness', 'Standby brightness'), type: 'number', min: 0, max: 255 },
					{ key: 'lcd_contrast', label: webuiL('reg.contrast', 'Contrast'), type: 'number', min: 0, max: 63 },
					{ key: 'lcd_dim_time', label: webuiL('reg.dim_timeout', 'Dim timeout (sec.)'), type: 'number', min: 0 },
					{ key: 'lcd_dim_brightness', label: webuiL('reg.dim_brightness', 'Dim brightness'), type: 'number', min: 0, max: 255 },
					{ key: 'lcd_epgmode', label: webuiL('reg.epg_display', 'EPG display'), type: 'select',
					  options: [{v:'0',l:'Standard'},{v:'1',l: webuiL('reg.epg_prog', 'Programme')},{v:'2',l: webuiL('reg.epg_ch_prog', 'Channel/programme')},
					            {v:'3',l: webuiL('reg.epg_ch_sep_prog', 'Channel/sep./programme')},{v:'7',l: webuiL('reg.epg_chs_prog', 'Channel (short)/programme')},
					            {v:'11',l: webuiL('reg.epg_chs_sep_prog', 'Channel (short)/sep./programme')},{v:'15',l: webuiL('reg.all', 'Everything')}] },
					{ key: 'lcd_show_volume', label: webuiL('reg.status_line', 'Status line'), type: 'select',
					  options: [{v:'0',l: webuiL('reg.progress', 'Progress')},{v:'1',l: webuiL('reg.volume', 'Volume')},{v:'2',l: webuiL('reg.both', 'Both')},{v:'3',l:'+Audio'}] }
				]
			}
		]
	},
	{
		id: 'glcd',
		v1section: 'display',
		group: 'disp',
		icon: regIcon('<rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>'),
		label: 'GLCD',
		sections: [
			{
				title: 'Graphic LCD',
				fields: [
					{ key: 'glcd_enable', label: webuiL('reg.glcd_power', 'GLCD on/off'), type: 'bool01' },
					{ key: 'glcd_inverse', label: webuiL('reg.glcd_invert', 'Invert the graphic LCD'), type: 'bool01' },
					{ key: 'glcd_brightness', label: webuiL('reg.brightness', 'Brightness'), type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_standby', label: webuiL('reg.standby_brightness', 'Standby brightness'), type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_dim', label: webuiL('reg.dim_brightness', 'Dim brightness'), type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_dim_time', label: webuiL('reg.dim_timeout', 'Dim timeout (sec.)'), type: 'number', min: 0, max: 60 }
				]
			}
		]
	},
	{
		id: 'cec',
		v1section: 'video',
		group: 'av',
		icon: regIcon('<rect x="2" y="7" width="20" height="15" rx="2" ry="2"/><polyline points="17 2 12 7 7 2"/>'),
		label: 'HDMI/CEC',
		sections: [
			{
				title: 'HDMI-CEC',
				fields: [
					{ key: 'hdmi_cec_mode', label: webuiL('reg.cec_mode', 'CEC mode'), type: 'select',
					  options: [{v:'0',l: webuiL('reg.off', 'Off')},{v:'1',l:'Tuner'},{v:'2',l:'Recorder'}] },
					{ key: 'hdmi_cec_view_on', label: webuiL('reg.tv_on', 'Turn TV on'), type: 'bool01' },
					{ key: 'hdmi_cec_standby', label: 'TV Standby', type: 'bool01' },
					{ key: 'hdmi_cec_volume', label: webuiL('reg.volume', 'Volume'), type: 'select',
					  options: [{v:'0',l: webuiL('reg.off', 'Off')},{v:'1',l:'Audiosystem'},{v:'2',l:'TV'}] }
				]
			}
		]
	},
	{
		id: 'system',
		v1section: 'misc',
		group: 'sys',
		icon: regIcon('<rect x="4" y="4" width="16" height="16" rx="2" ry="2"/><rect x="9" y="9" width="6" height="6"/><line x1="9" y1="1" x2="9" y2="4"/><line x1="15" y1="1" x2="15" y2="4"/><line x1="9" y1="20" x2="9" y2="23"/><line x1="15" y1="20" x2="15" y2="23"/><line x1="20" y1="9" x2="23" y2="9"/><line x1="20" y1="14" x2="23" y2="14"/><line x1="1" y1="9" x2="4" y2="9"/><line x1="1" y1="14" x2="4" y2="14"/>'),
		label: webuiL('reg.system', 'System'),
		sections: [
			{
				title: webuiL('reg.cpu_fan', 'CPU and fan (box dependent)'),
				fields: [
					{ key: 'cpufreq', label: webuiL('reg.cpufreq', 'CPU frequency (MHz)'), type: 'number', min: 0,
					  hint: webuiL('reg.cpufreq_hint', '0 keeps the default; box dependent') },
					{ key: 'standby_cpufreq', label: webuiL('reg.standby_cpufreq', 'Standby CPU frequency (MHz)'), type: 'number', min: 0 },
					{ key: 'fan_speed', label: webuiL('reg.fan_speed', 'Fan speed'), type: 'number', min: 0, max: 15 }
				]
			}
		]
	},
	{
		id: 'mounts',
		group: 'net',
		icon: regIcon('<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/><line x1="10" y1="16" x2="10.01" y2="16"/>'),
		label: webuiL('reg.mounts', 'Network mounts'),
		custom: true
	},
	{
		id: 'plugins',
		group: 'ext',
		icon: regIcon('<path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/><polyline points="3.27 6.96 12 12.01 20.73 6.96"/><line x1="12" y1="22.08" x2="12" y2="12"/>'),
		label: webuiL('reg.plugins', 'Neutrino plugins'),
		custom: true
	},
	{
		id: 'wol',
		group: 'net',
		icon: regIcon('<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>'),
		label: 'Wake-on-LAN',
		custom: true
	},
	{
		id: 'xmltv',
		group: 'net',
		icon: regIcon('<path d="M2 16.1A5 5 0 0 1 5.9 20M2 12.05A9 9 0 0 1 9.95 20M2 8V6a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2h-6"/><line x1="2" y1="20" x2="2.01" y2="20"/>'),
		label: webuiL('reg.webtv_sources', 'WebTV sources'),
		custom: true
	},
	{
		id: 'zapitupload',
		group: 'sys',
		icon: regIcon('<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>'),
		label: 'Zapit-Upload',
		custom: true
	},
	{
		id: 'extensions',
		group: 'ext',
		icon: regIcon('<polygon points="12 2 2 7 12 12 22 7 12 2"/><polyline points="2 17 12 22 22 17"/><polyline points="2 12 12 17 22 12"/>'),
		label: webuiL('set.ext_title', 'Neutrino-Web-UI extensions'),
		custom: true
	},
	{
		id: 'backup',
		group: 'sys',
		icon: regIcon('<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>'),
		label: webuiL('reg.backup', 'Backup'),
		custom: true
	},
	{
		id: 'personalize',
		group: 'neutrino',
		icon: regIcon('<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>'),
		label: webuiL('reg.personalize', 'Personalization'),
		// Gruppen und Reihenfolge wie Neutrinos Menuebaum (neutrino_menue.cpp:
		// 137-506, mediaplayer.cpp:150-207, personalize.cpp:463-532).
		// requireKeys: Neutrino schreibt beim Speichern immer die volle
		// Key-Tabelle (neutrino.cpp:1920) — fehlt ein Key in der Config, kennt
		// diese Box ihn nicht (ENABLE_PIP, ENABLE_UPNP, HAVE_CST_HARDWARE) und
		// das Feld entfaellt.
		// Nicht angeboten, weil im C++ wirkungslos: personalize_media_menu
		// (nirgends ausgewertet), personalize_tools und personalize_scripts
		// (Menuepunkte stehen in #if 0, neutrino_menue.cpp:198-222).
		requireKeys: true,
		sections: [
			{
				title: webuiL('pers.access', 'Access options'),
				collapsible: true, open: true,
				fields: [
					persBool('pinstatus', webuiL('pers.pin_in_use', 'PIN access for personalize setup')),
					{ key: 'personalize_pincode', label: webuiL('pers.pincode', 'Personalization PIN'),
					  type: 'password', pattern: '[0-9]{4}', maxlength: 4, rule: webuiL('pers.pincode_rule', 'Enter four digits') }
				]
			},
			{
				title: webuiL('pers.grp_main', 'Main menu'),
				collapsible: true,
				fields: [
					persMode('tv_radio_mode', webuiL('pers.tv_radio_mode', 'TV-Radio switch')),
					persMode('tv_mode', webuiL('pers.tv_mode', 'TV-Mode'), 'tv_radio_mode'),
					persMode('radio_mode', webuiL('pers.radio_mode', 'Radio mode'), 'tv_radio_mode'),
					persMode('timer', webuiL('pers.timer', 'Timer list')),
					persMode('media', webuiL('pers.media', 'Media')),
					persMode('avinput', webuiL('pers.avinput', 'HDMI Input (Mode)')),
					persMode('avinput_pip', webuiL('pers.avinput_pip', 'HDMI Input (PiP)')),
					persMode('games', webuiL('pers.games', 'Games')),
					persMode('lua', webuiL('pers.lua', 'Lua Plugins')),
					persAccess('settings', webuiL('pers.settings', 'System settings')),
					persAccess('service', webuiL('pers.service', 'Installation (service)')),
					persMode('poweroff_menu', webuiL('pers.poweroff_menu', 'Power off')),
					persMode('sleeptimer', webuiL('pers.sleeptimer', 'Sleeptimer'), 'poweroff_menu'),
					persMode('reboot', webuiL('pers.reboot', 'Reboot'), 'poweroff_menu'),
					persMode('standby', webuiL('pers.standby', 'Standby'), 'poweroff_menu'),
					persMode('shutdown', webuiL('pers.shutdown', 'Shutdown (Deep-standby)'), 'poweroff_menu'),
					persMode('infomenu_main', webuiL('pers.infomenu', 'Information')),
					persMode('cisettings_main', webuiL('pers.cisettings', 'Conditional access'))
				]
			},
			{
				title: webuiL('pers.grp_settings', 'Settings menu'),
				collapsible: true,
				fields: [
					persAccess('settingsmager', webuiL('pers.settingsmager', 'Manage settings')),
					persMode('misc', webuiL('pers.misc', 'Extended settings')),
					persMode('video', webuiL('pers.video', 'Video')),
					persMode('audio', webuiL('pers.audio', 'Audio')),
					persMode('parentallock', webuiL('pers.parentallock', 'Parental lock')),
					persMode('network', webuiL('pers.network', 'Network')),
					persMode('recording', webuiL('pers.recording', 'Recording')),
					persMode('osdlang', webuiL('pers.osdlang', 'Language and timezone')),
					persMode('osd', webuiL('pers.osd', 'OSD')),
					persMode('vfd', webuiL('pers.vfd', 'Front-Panel')),
					persMode('drives', webuiL('pers.drives', 'Data storage')),
					persMode('cisettings_settings', webuiL('pers.cisettings', 'Conditional access')),
					persMode('keybindings', webuiL('pers.keybindings', 'Key setup')),
					persMode('mediaplayer', webuiL('pers.mediaplayer', 'Multimedia'))
				]
			},
			{
				title: webuiL('pers.grp_service', 'Service menu'),
				collapsible: true,
				fields: [
					persMode('tuner', webuiL('pers.tuner', 'Setup tuner')),
					persMode('scants', webuiL('pers.scants', 'Servicescan')),
					persMode('reload_channels', webuiL('pers.reload_channels', 'Reload channel lists')),
					persMode('bouquet_edit', webuiL('pers.bouquet_edit', 'Bouquet editor')),
					persMode('reset_channels', webuiL('pers.reset_channels', 'Delete channels')),
					persMode('restart', webuiL('pers.restart', 'Soft restart')),
					persMode('reload_plugins', webuiL('pers.reload_plugins', 'Reload plugins')),
					persMode('infomenu_service', webuiL('pers.infomenu', 'Information')),
					persMode('softupdate', webuiL('pers.softupdate', 'Software Update'))
				]
			},
			{
				title: webuiL('pers.grp_media', 'Multimedia'),
				collapsible: true,
				fields: [
					persMode('media_audio', webuiL('pers.media_audio', 'Audioplayer')),
					persMode('media_intetplay', webuiL('pers.media_intetplay', 'Internetradio')),
					persAccess('media_movieplayer', webuiL('pers.media_movieplayer', 'Mediaplayer')),
					persMode('media_pviewer', webuiL('pers.media_pviewer', 'Picture viewer')),
					persMode('media_upnp', webuiL('pers.media_upnp', 'UPnP browser'))
				]
			},
			{
				title: webuiL('pers.grp_mplayer', 'Mediaplayer'),
				collapsible: true,
				fields: [
					persMode('mplayer_mbrowser', webuiL('pers.mplayer_mbrowser', 'My recordings')),
					persMode('mplayer_fileplay_video', webuiL('pers.mplayer_video', 'Multiformat video playback')),
					persMode('mplayer_fileplay_audio', webuiL('pers.mplayer_audio', 'Multiformat audio playback'))
				]
			},
			{
				// Farbtasten: ACTIVE-Domaene (an/aus), keine Sichtbarkeit
				// (settings.cpp:38-41, personalize.cpp:485)
				title: webuiL('pers.usermenu', 'User menu'),
				collapsible: true,
				fields: [
					persBool('redbutton', webuiL('pers.btn_red', 'User menu red')),
					persBool('greenbutton', webuiL('pers.btn_green', 'User menu green')),
					persBool('yellowbutton', webuiL('pers.btn_yellow', 'User menu yellow')),
					persBool('bluebutton', webuiL('pers.btn_blue', 'User menu blue')),
					persBool('usermenu_show_cancel', webuiL('pers.show_cancel', 'Show Cancel button')),
					persBool('usermenu_plugin_type_games', webuiL('pers.plugin_games', 'Games')),
					persBool('usermenu_plugin_type_tools', webuiL('pers.plugin_tools', 'Tools')),
					persBool('usermenu_plugin_type_scripts', webuiL('pers.plugin_scripts', 'Scripts')),
					persBool('usermenu_plugin_type_lua', webuiL('pers.plugin_lua', 'Lua Plugins'))
				]
			},
			{
				// Neutrino wertet diese Keys zur Laufzeit aus (user_menue.cpp:
				// 323-365), bietet dafuer aber kein Menue an — die Web-UI schon.
				title: webuiL('pers.grp_featkey', 'Feature keys'),
				collapsible: true,
				fields: [
					{ type: 'info', label: webuiL('pers.featkey_label', 'Note'),
					  text: webuiL('pers.featkey_note', 'Neutrino has no menu for these keys; the Neutrino-Web-UI offers them in addition.') },
					persFeatKey('fav', webuiL('pers.feat_fav', 'add channel to favorites')),
					persFeatKey('timerlist', webuiL('pers.feat_timerlist', 'Timer list')),
					persFeatKey('vtxt', webuiL('pers.feat_vtxt', 'Videotext')),
					persFeatKey('rclock', webuiL('pers.feat_rclock', 'Lock remote control'))
				]
			}
		]
	},
	{
		id: 'buttons',
		v1section: 'keybindings',
		group: 'neutrino',
		icon: regIcon('<rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>'),
		label: webuiL('reg.keys', 'Keys'),
		sections: [
			{
				title: webuiL('reg.key_mapping', 'Key mapping (RC keycodes)'),
				fields: [
					{ key: 'bouquetlist_mode', label: webuiL('reg.bqlist_mode', 'Bouquet list mode'), type: 'select',
					  options: [{v:'0',l:'Bouquets'},{v:'1',l: webuiL('bqe.channels', 'Channels')}] },
					{ key: 'key_tvradio_mode', label: webuiL('reg.tv_radio_switch', 'TV/radio switching'), type: 'number', min: 0 },
					{ key: 'key_lastchannel', label: webuiL('reg.last_channel', 'Last channel'), type: 'number', min: 0 },
					{ key: 'key_zaphistory', label: webuiL('reg.zap_history', 'Zap history'), type: 'number', min: 0 },
					{ key: 'key_bouquet_up', label: webuiL('reg.bq_up', 'Bouquet up'), type: 'number', min: 0 },
					{ key: 'key_bouquet_down', label: webuiL('reg.bq_down', 'Bouquet down'), type: 'number', min: 0 },
					{ key: 'key_quickzap_up', label: webuiL('reg.quickzap_up', 'Quickzap up'), type: 'number', min: 0 },
					{ key: 'key_quickzap_down', label: webuiL('reg.quickzap_down', 'Quickzap down'), type: 'number', min: 0 },
					{ key: 'key_subchannel_up', label: webuiL('reg.subch_up', 'Subchannel up'), type: 'number', min: 0 },
					{ key: 'key_subchannel_down', label: webuiL('reg.subch_down', 'Subchannel down'), type: 'number', min: 0 }
				]
			}
		]
	},
	{
		id: 'nhttpd',
		group: 'web',
		icon: regIcon('<rect x="2" y="2" width="20" height="8" rx="2" ry="2"/><rect x="2" y="14" width="20" height="8" rx="2" ry="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/>'),
		label: 'Webserver',
		// read and written over /api/v1/system/webserver, by its member names
		config: 'nhttpd',
		sections: [
			{
				title: webuiL('reg.access', 'Access'),
				fields: [
					{ key: 'page_login', label: webuiL('reg.auth', 'Password protection'), type: 'bool' },
					{ key: 'user', label: webuiL('set.mnt_user', 'User'), type: 'text' },
					// sent only with a new password or user name, which the box changes only with it
					{ key: 'current', label: webuiL('set.pw_current', 'Current password'), type: 'password', autocomplete: 'current-password',
					  hint: webuiL('set.pw_current_hint', 'needed to change the password or the user name: the password you sign in with') },
					{ key: 'password', label: webuiL('set.mnt_pass', 'Password'), type: 'password' },
					{ key: 'no_login_prefix', label: webuiL('reg.noauth_client', 'Without login (IP prefix)'), type: 'text',
					  hint: webuiL('reg.noauth_client_hint', 'callers whose address begins with this see the pages without the login, and so does the box itself, unless a proxy is entered in nhttpd-access.conf (trusted_proxies); a change made in the web interface still asks for the sign-in') },
					// read only, kept in nhttpd-access.conf beside trusted_proxies, which no row shows
					{ key: 'allowed_hosts', label: webuiL('reg.allowed_hosts', 'Other names of the box'), type: 'info',
					  hint: webuiL('reg.allowed_hosts_hint', 'besides its IP address and the names of the home network (without a dot, with .local or with the router\'s domain), the box answers only to these names, such as a DynDNS name or the name of a proxy; they are added to the line allowed_hosts in nhttpd-access.conf, names only and no comment on the line, and count within 10 seconds') },
					// kept in nhttpd-access.conf beside allowed_hosts, and in effect at once
					{ key: 'control_without_asking', label: webuiL('reg.control_without_asking', 'Run commands without asking'), type: 'bool',
					  hint: webuiL('reg.control_without_asking_hint', 'a browser that opens a command of the old interface (an address under /control/ and the like) from a bookmark, a link or another site runs it without the box asking first; a page on another site could then run such commands too. Off, the box asks each time') },
					{ key: 'port', label: 'Port', type: 'number', min: 1, max: 65535 }
				]
			},
			{
				title: webuiL('reg.content', 'Content'),
				fields: [
					{ key: 'override_directory', label: webuiL('reg.override_dir', 'Override directory'), type: 'text', picker: true, hint: webuiL('reg.override_dir_hint', 'files here override the default web root') },
					{ key: 'hosted_directory', label: webuiL('reg.hosted_dir', 'Hosted directory'), type: 'text', picker: true },
					{ key: 'threading', label: 'Threading', type: 'bool' },
					{ key: 'send_all_types', label: webuiL('reg.send_all_types', 'Send all file types'), type: 'bool' }
				]
			}
		]
	},
	{
		id: 'webui',
		group: 'web',
		icon: regIcon('<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>'),
		label: 'Neutrino-Web-UI',
		config: 'webui',
		sections: [
			{
				/* Only what this interface reads. The box name and colour, the
				   screenshot key, the framebuffer mode and the management address
				   were read by the old pages alone and stay in webui.conf for
				   whoever still asks for them. */
				title: webuiL('reg.ui', 'User interface'),
				fields: [
					{ key: 'start_page', label: webuiL('reg.start_page', 'Start page'), type: 'select', options: webuiStartPages() },
					{ key: 'screenshot_timeout', label: webuiL('reg.screenshot_timeout', 'Screenshot timeout (sec.)'), type: 'number', min: 0 }
				]
			}
		]
	}
];
