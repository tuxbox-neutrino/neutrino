/*
 * Settings Registry — Central field definitions for settings.yhtm
 * Each category has: id, label, sections with fields
 * Field types: select, bool, bool01, number, text, password,
 *   info   { label, text | key }                    — readonly display
 *   action { id, label, button, confirm?, success?, refresh?,
 *            api: {endpoint, params, raw?} }        — v4 call button
 * Optional: showIf: {key, value} for conditional visibility
 */
// M5b: Sidebar-Gruppen — Reihenfolge = Anzeige-Reihenfolge der Sidebar.
// Jede Registry-Kategorie traegt group:'<id>'; Extension-Kategorien
// (EXT_SETTINGS_CATS) defaulten in buildNavigation() auf 'ext'.
var SETTINGS_GROUPS = [
	{ id: 'av',   label: ywebL('set.grp_av', 'Picture & Sound') },
	{ id: 'disp', label: ywebL('set.grp_disp', 'Displays') },
	{ id: 'sys',  label: ywebL('set.grp_sys', 'System') },
	{ id: 'net',  label: ywebL('set.grp_net', 'Network & Sources') },
	{ id: 'ui',   label: ywebL('set.grp_ui', 'User interface') },
	{ id: 'ext',  label: ywebL('set.grp_ext', 'Extensions') }
];

// Personalisierung — die Wertedomaenen aus src/gui/personalize.h:185-231.
// Welche Domaene ein Eintrag hat, entscheidet in Neutrino das item_mode-
// Argument von personalize.addItem(): SHOW_AS_ITEM_OPTION -> MODE,
// SHOW_AS_ACCESS_OPTION -> ACCESS. Jeder Key deklariert sie hier genau einmal.
var PERS_MODE_OPTS = [                                  // PERSONALIZE_MODE
	{ v: '0', l: ywebL('pers.notvisible', 'Not Visible') },
	{ v: '1', l: ywebL('pers.visible', 'Visible') },
	{ v: '2', l: ywebL('pers.pin', 'PIN') }
];
var PERS_KEY_OPTS = [                                   // PERSONALIZE_FEAT_KEY
	{ v: '0', l: ywebL('pers.col_red', 'red') },
	{ v: '1', l: ywebL('pers.col_green', 'green') },
	{ v: '2', l: ywebL('pers.col_yellow', 'yellow') },
	{ v: '3', l: ywebL('pers.col_blue', 'blue') },
	{ v: '4', l: ywebL('pers.col_auto', 'auto') }
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
		 label: label + ' ' + ywebL('pers.pinstatus', 'Require PIN') };
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
		group: 'av',
		icon: regIcon('<polygon points="23 7 16 12 23 17 23 7"/><rect x="1" y="5" width="15" height="14" rx="2" ry="2"/>'),
		label: 'Video',
		sections: [
			{
				title: ywebL('reg.video_format', 'Video format'),
				fields: [
					{ key: 'video_Format', label: ywebL('reg.video_format', 'Video format'), type: 'select',
					  options: [{v:'0',l:'Auto'},{v:'1',l:'16:9'},{v:'2',l:'4:3 (LB)'},{v:'3',l:'4:3 (PS)'}] },
					{ key: 'video_43mode', label: ywebL('reg.mode43', '4:3 mode'), type: 'select',
					  options: [{v:'0',l:'Panscan'},{v:'1',l:'Panscan2'},{v:'2',l:'Letterbox'},{v:'3',l: ywebL('reg.fullscreen', 'Full screen')}] },
					{ key: 'vcr_AutoSwitch', label: ywebL('reg.vcr_switch', 'VCR auto switch'), type: 'bool' }
				]
			},
			{
				title: ywebL('reg.output', 'Output'),
				fields: [
					{ key: 'video_dbdr', label: 'Deblocking/Deringing', type: 'select',
					  options: [{v:'0',l: ywebL('reg.off', 'Off')},{v:'1',l:'Deblock'},{v:'2',l: ywebL('reg.both', 'Both')}] },
					{ key: 'zappingmode', label: ywebL('reg.zap_mode', 'Zapping mode'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.mute', 'Mute')},{v:'1',l: ywebL('reg.hold_picture', 'Hold picture')},{v:'2',l: ywebL('reg.mute_until_lock', 'Mute until lock')},{v:'3',l: ywebL('reg.hold_until_lock', 'Hold until lock')}] },
					{ key: 'hdmi_colorimetry', label: ywebL('reg.hdmi_colorspace', 'HDMI color space'), type: 'select',
					  options: [{v:'0',l:'Auto'},{v:'1',l:'BT.709'},{v:'2',l:'BT.2020'}] }
				]
			}
		]
	},
	{
		id: 'audio',
		group: 'av',
		icon: regIcon('<polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><path d="M15.54 8.46a5 5 0 0 1 0 7.07"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14"/>'),
		label: 'Audio',
		sections: [
			{
				title: ywebL('reg.analog_out', 'Analog output'),
				fields: [
					{ key: 'audio_AnalogMode', label: ywebL('reg.analog_mode', 'Analog mode'), type: 'select',
					  options: [{v:'0',l:'Stereo'},{v:'1',l: ywebL('reg.mono_left', 'Mono left')},{v:'2',l: ywebL('reg.mono_right', 'Mono right')}] },
					{ key: 'audio_DolbyDigital', label: 'Dolby Digital', type: 'bool01' }
				]
			},
			{
				title: ywebL('reg.digital_pass', 'Digital passthrough'),
				fields: [
					{ key: 'ac3_pass', label: 'AC3 Passthrough', type: 'bool01' },
					{ key: 'dts_pass', label: 'DTS Passthrough', type: 'bool01' },
					{ key: 'hdmi_dd', label: 'HDMI Dolby Digital', type: 'select',
					  options: [{v:'0',l: ywebL('reg.off', 'Off')},{v:'1',l:'Auto'},{v:'2',l: ywebL('reg.force', 'Force')}] },
					{ key: 'spdif_dd', label: 'SPDIF Dolby Digital', type: 'bool01' }
				]
			},
			{
				title: ywebL('reg.av_sync', 'A/V synchronization'),
				fields: [
					{ key: 'avsync', label: 'A/V-Sync', type: 'select',
					  options: [{v:'0',l: ywebL('reg.off', 'Off')},{v:'1',l: ywebL('reg.on', 'On')},{v:'2',l:'Audio Master'}] }
				]
			},
			{
				title: ywebL('reg.volume', 'Volume'),
				fields: [
					{ key: 'current_volume_step', label: ywebL('reg.volume_step', 'Volume step'), type: 'number', min: 1, max: 25 },
					{ key: 'start_volume', label: ywebL('reg.start_volume', 'Start volume'), type: 'number', min: -1, max: 100, hint: ywebL('reg.start_volume_hint', '-1 keeps the last volume') },
					{ key: 'audio_volume_percent_ac3', label: ywebL('reg.ac3_vol', 'AC3 volume (%)'), type: 'number', min: 0, max: 100 },
					{ key: 'audio_volume_percent_pcm', label: ywebL('reg.pcm_vol', 'PCM volume (%)'), type: 'number', min: 0, max: 100 },
					{ key: 'audiochannel_up_down_enable', label: ywebL('reg.audio_ch_selectable', 'Audio channel selectable'), type: 'bool' },
					{ key: 'audio_left_right_selectable', label: ywebL('reg.lr_selectable', 'Left/right selectable'), type: 'bool' }
				]
			},
			{
				title: ywebL('reg.srs', 'SRS TruVolume'),
				fields: [
					{ key: 'srs_enable', label: ywebL('reg.srs_enable', 'SRS enabled'), type: 'bool01' },
					{ key: 'srs_algo', label: ywebL('reg.srs_algo', 'SRS mode'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.srs_light', 'light')},{v:'1',l: ywebL('reg.srs_normal', 'normal')},{v:'2',l: ywebL('reg.srs_heavy', 'heavy')}] },
					{ key: 'srs_ref_volume', label: ywebL('reg.srs_ref_volume', 'Reference volume'), type: 'number', min: 0, max: 100 },
					{ key: 'srs_nmgr_enable', label: ywebL('reg.srs_nmgr', 'Volume manager'), type: 'bool01' }
				]
			}
		]
	},
	{
		id: 'lcd',
		group: 'disp',
		icon: regIcon('<rect x="5" y="2" width="14" height="20" rx="2" ry="2"/><line x1="12" y1="18" x2="12.01" y2="18"/>'),
		label: 'LCD',
		sections: [
			{
				title: 'LCD Display',
				fields: [
					{ key: 'lcd_power', label: ywebL('reg.lcd_power', 'LCD on/off'), type: 'bool01' },
					{ key: 'lcd_inverse', label: ywebL('reg.invert', 'Invert'), type: 'bool01' },
					{ key: 'lcd_brightness', label: ywebL('reg.brightness', 'Brightness'), type: 'number', min: 0, max: 255 },
					{ key: 'lcd_standbybrightness', label: ywebL('reg.standby_brightness', 'Standby brightness'), type: 'number', min: 0, max: 255 },
					{ key: 'lcd_contrast', label: ywebL('reg.contrast', 'Contrast'), type: 'number', min: 0, max: 63 },
					{ key: 'lcd_dim_time', label: ywebL('reg.dim_timeout', 'Dim timeout (sec.)'), type: 'number', min: 0 },
					{ key: 'lcd_dim_brightness', label: ywebL('reg.dim_brightness', 'Dim brightness'), type: 'number', min: 0, max: 255 },
					{ key: 'lcd_epgmode', label: ywebL('reg.epg_display', 'EPG display'), type: 'select',
					  options: [{v:'0',l:'Standard'},{v:'1',l: ywebL('reg.epg_prog', 'Programme')},{v:'2',l: ywebL('reg.epg_ch_prog', 'Channel/programme')},
					            {v:'3',l: ywebL('reg.epg_ch_sep_prog', 'Channel/sep./programme')},{v:'7',l: ywebL('reg.epg_chs_prog', 'Channel (short)/programme')},
					            {v:'11',l: ywebL('reg.epg_chs_sep_prog', 'Channel (short)/sep./programme')},{v:'15',l: ywebL('reg.all', 'Everything')}] },
					{ key: 'lcd_show_volume', label: ywebL('reg.status_line', 'Status line'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.progress', 'Progress')},{v:'1',l: ywebL('reg.volume', 'Volume')},{v:'2',l: ywebL('reg.both', 'Both')},{v:'3',l:'+Audio'}] }
				]
			}
		]
	},
	{
		id: 'glcd',
		group: 'disp',
		icon: regIcon('<rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>'),
		label: 'GLCD',
		sections: [
			{
				title: 'Graphic LCD',
				fields: [
					{ key: 'glcd_enable', label: ywebL('reg.glcd_power', 'GLCD on/off'), type: 'bool01' },
					{ key: 'glcd_inverse', label: ywebL('reg.invert', 'Invert'), type: 'bool01' },
					{ key: 'glcd_brightness', label: ywebL('reg.brightness', 'Brightness'), type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_standby', label: ywebL('reg.standby_brightness', 'Standby brightness'), type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_dim', label: ywebL('reg.dim_brightness', 'Dim brightness'), type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_dim_time', label: ywebL('reg.dim_timeout', 'Dim timeout (sec.)'), type: 'number', min: 0, max: 60 }
				]
			}
		]
	},
	{
		id: 'cec',
		group: 'av',
		icon: regIcon('<rect x="2" y="7" width="20" height="15" rx="2" ry="2"/><polyline points="17 2 12 7 7 2"/>'),
		label: 'HDMI/CEC',
		sections: [
			{
				title: 'HDMI-CEC',
				fields: [
					{ key: 'hdmi_cec_mode', label: ywebL('reg.cec_mode', 'CEC mode'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.off', 'Off')},{v:'1',l:'Tuner'},{v:'2',l:'Recorder'}] },
					{ key: 'hdmi_cec_view_on', label: ywebL('reg.tv_on', 'Turn TV on'), type: 'bool01' },
					{ key: 'hdmi_cec_standby', label: 'TV Standby', type: 'bool01' },
					{ key: 'hdmi_cec_volume', label: ywebL('reg.volume', 'Volume'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.off', 'Off')},{v:'1',l:'Audiosystem'},{v:'2',l:'TV'}] }
				]
			}
		]
	},
	{
		id: 'system',
		group: 'sys',
		icon: regIcon('<rect x="4" y="4" width="16" height="16" rx="2" ry="2"/><rect x="9" y="9" width="6" height="6"/><line x1="9" y1="1" x2="9" y2="4"/><line x1="15" y1="1" x2="15" y2="4"/><line x1="9" y1="20" x2="9" y2="23"/><line x1="15" y1="20" x2="15" y2="23"/><line x1="20" y1="9" x2="23" y2="9"/><line x1="20" y1="14" x2="23" y2="14"/><line x1="1" y1="9" x2="4" y2="9"/><line x1="1" y1="14" x2="4" y2="14"/>'),
		label: ywebL('reg.system', 'System'),
		sections: [
			{
				title: ywebL('reg.cpu_fan', 'CPU and fan (box dependent)'),
				fields: [
					{ key: 'cpufreq', label: ywebL('reg.cpufreq', 'CPU frequency (MHz)'), type: 'number', min: 0,
					  hint: ywebL('reg.cpufreq_hint', '0 keeps the default; box dependent') },
					{ key: 'standby_cpufreq', label: ywebL('reg.standby_cpufreq', 'Standby CPU frequency (MHz)'), type: 'number', min: 0 },
					{ key: 'fan_speed', label: ywebL('reg.fan_speed', 'Fan speed'), type: 'number', min: 0, max: 15 }
				]
			}
		]
	},
	{
		id: 'mounts',
		group: 'net',
		icon: regIcon('<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/><line x1="10" y1="16" x2="10.01" y2="16"/>'),
		label: ywebL('reg.mounts', 'Network mounts'),
		custom: true
	},
	{
		id: 'plugins',
		group: 'ext',
		icon: regIcon('<path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/><polyline points="3.27 6.96 12 12.01 20.73 6.96"/><line x1="12" y1="22.08" x2="12" y2="12"/>'),
		label: ywebL('reg.plugins', 'Neutrino plugins'),
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
		label: ywebL('reg.webtv_sources', 'WebTV sources'),
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
		label: ywebL('set.ext_title', 'Neutrino-Web-UI extensions'),
		custom: true
	},
	{
		id: 'backup',
		group: 'sys',
		icon: regIcon('<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>'),
		label: ywebL('reg.backup', 'Backup'),
		custom: true
	},
	{
		id: 'personalize',
		group: 'ui',
		icon: regIcon('<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>'),
		label: ywebL('reg.personalize', 'Personalization'),
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
				title: ywebL('pers.access', 'Access options'),
				collapsible: true, open: true,
				fields: [
					persBool('pinstatus', ywebL('pers.pin_in_use', 'PIN access for personalize setup')),
					{ key: 'personalize_pincode', label: ywebL('pers.pincode', 'Personalization PIN'),
					  type: 'password', pattern: '[0-9]{4}', maxlength: 4 }
				]
			},
			{
				title: ywebL('pers.grp_main', 'Main menu'),
				collapsible: true,
				fields: [
					persMode('tv_radio_mode', ywebL('pers.tv_radio_mode', 'TV-Radio switch')),
					persMode('tv_mode', ywebL('pers.tv_mode', 'TV-Mode'), 'tv_radio_mode'),
					persMode('radio_mode', ywebL('pers.radio_mode', 'Radio mode'), 'tv_radio_mode'),
					persMode('timer', ywebL('pers.timer', 'Timer list')),
					persMode('media', ywebL('pers.media', 'Media')),
					persMode('avinput', ywebL('pers.avinput', 'HDMI Input (Mode)')),
					persMode('avinput_pip', ywebL('pers.avinput_pip', 'HDMI Input (PiP)')),
					persMode('games', ywebL('pers.games', 'Games')),
					persMode('lua', ywebL('pers.lua', 'Lua Plugins')),
					persAccess('settings', ywebL('pers.settings', 'System settings')),
					persAccess('service', ywebL('pers.service', 'Installation (service)')),
					persMode('poweroff_menu', ywebL('pers.poweroff_menu', 'Power off')),
					persMode('sleeptimer', ywebL('pers.sleeptimer', 'Sleeptimer'), 'poweroff_menu'),
					persMode('reboot', ywebL('pers.reboot', 'Reboot'), 'poweroff_menu'),
					persMode('standby', ywebL('pers.standby', 'Standby'), 'poweroff_menu'),
					persMode('shutdown', ywebL('pers.shutdown', 'Shutdown (Deep-standby)'), 'poweroff_menu'),
					persMode('infomenu_main', ywebL('pers.infomenu', 'Information')),
					persMode('cisettings_main', ywebL('pers.cisettings', 'Conditional access'))
				]
			},
			{
				title: ywebL('pers.grp_settings', 'Settings menu'),
				collapsible: true,
				fields: [
					persAccess('settingsmager', ywebL('pers.settingsmager', 'Manage settings')),
					persMode('misc', ywebL('pers.misc', 'Extended settings')),
					persMode('video', ywebL('pers.video', 'Video')),
					persMode('audio', ywebL('pers.audio', 'Audio')),
					persMode('parentallock', ywebL('pers.parentallock', 'Parental lock')),
					persMode('network', ywebL('pers.network', 'Network')),
					persMode('recording', ywebL('pers.recording', 'Recording')),
					persMode('osdlang', ywebL('pers.osdlang', 'Language and timezone')),
					persMode('osd', ywebL('pers.osd', 'OSD')),
					persMode('vfd', ywebL('pers.vfd', 'Front-Panel')),
					persMode('drives', ywebL('pers.drives', 'Data storage')),
					persMode('cisettings_settings', ywebL('pers.cisettings', 'Conditional access')),
					persMode('keybindings', ywebL('pers.keybindings', 'Key setup')),
					persMode('mediaplayer', ywebL('pers.mediaplayer', 'Multimedia'))
				]
			},
			{
				title: ywebL('pers.grp_service', 'Service menu'),
				collapsible: true,
				fields: [
					persMode('tuner', ywebL('pers.tuner', 'Setup tuner')),
					persMode('scants', ywebL('pers.scants', 'Servicescan')),
					persMode('reload_channels', ywebL('pers.reload_channels', 'Reload channel lists')),
					persMode('bouquet_edit', ywebL('pers.bouquet_edit', 'Bouquet editor')),
					persMode('reset_channels', ywebL('pers.reset_channels', 'Delete channels')),
					persMode('restart', ywebL('pers.restart', 'Soft restart')),
					persMode('reload_plugins', ywebL('pers.reload_plugins', 'Reload plugins')),
					persMode('infomenu_service', ywebL('pers.infomenu', 'Information')),
					persMode('softupdate', ywebL('pers.softupdate', 'Software Update'))
				]
			},
			{
				title: ywebL('pers.grp_media', 'Multimedia'),
				collapsible: true,
				fields: [
					persMode('media_audio', ywebL('pers.media_audio', 'Audioplayer')),
					persMode('media_intetplay', ywebL('pers.media_intetplay', 'Internetradio')),
					persAccess('media_movieplayer', ywebL('pers.media_movieplayer', 'Mediaplayer')),
					persMode('media_pviewer', ywebL('pers.media_pviewer', 'Picture viewer')),
					persMode('media_upnp', ywebL('pers.media_upnp', 'UPnP browser'))
				]
			},
			{
				title: ywebL('pers.grp_mplayer', 'Mediaplayer'),
				collapsible: true,
				fields: [
					persMode('mplayer_mbrowser', ywebL('pers.mplayer_mbrowser', 'My recordings')),
					persMode('mplayer_fileplay_video', ywebL('pers.mplayer_video', 'Multiformat video playback')),
					persMode('mplayer_fileplay_audio', ywebL('pers.mplayer_audio', 'Multiformat audio playback'))
				]
			},
			{
				// Farbtasten: ACTIVE-Domaene (an/aus), keine Sichtbarkeit
				// (settings.cpp:38-41, personalize.cpp:485)
				title: ywebL('pers.usermenu', 'User menu'),
				collapsible: true,
				fields: [
					persBool('redbutton', ywebL('pers.btn_red', 'User menu red')),
					persBool('greenbutton', ywebL('pers.btn_green', 'User menu green')),
					persBool('yellowbutton', ywebL('pers.btn_yellow', 'User menu yellow')),
					persBool('bluebutton', ywebL('pers.btn_blue', 'User menu blue')),
					persBool('usermenu_show_cancel', ywebL('pers.show_cancel', 'Show Cancel button')),
					persBool('usermenu_plugin_type_games', ywebL('pers.plugin_games', 'Games')),
					persBool('usermenu_plugin_type_tools', ywebL('pers.plugin_tools', 'Tools')),
					persBool('usermenu_plugin_type_scripts', ywebL('pers.plugin_scripts', 'Scripts')),
					persBool('usermenu_plugin_type_lua', ywebL('pers.plugin_lua', 'Lua Plugins'))
				]
			},
			{
				// Neutrino wertet diese Keys zur Laufzeit aus (user_menue.cpp:
				// 323-365), bietet dafuer aber kein Menue an — yWeb schon.
				title: ywebL('pers.grp_featkey', 'Feature keys'),
				collapsible: true,
				fields: [
					{ type: 'info', label: ywebL('pers.featkey_label', 'Note'),
					  text: ywebL('pers.featkey_note', 'Neutrino has no menu for these keys; the Neutrino-Web-UI offers them in addition.') },
					persFeatKey('fav', ywebL('pers.feat_fav', 'add channel to favorites')),
					persFeatKey('timerlist', ywebL('pers.feat_timerlist', 'Timer list')),
					persFeatKey('vtxt', ywebL('pers.feat_vtxt', 'Videotext')),
					persFeatKey('rclock', ywebL('pers.feat_rclock', 'Lock remote control'))
				]
			}
		]
	},
	{
		id: 'buttons',
		group: 'ui',
		icon: regIcon('<rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>'),
		label: ywebL('reg.keys', 'Keys'),
		sections: [
			{
				title: ywebL('reg.key_mapping', 'Key mapping (RC keycodes)'),
				fields: [
					{ key: 'bouquetlist_mode', label: ywebL('reg.bqlist_mode', 'Bouquet list mode'), type: 'select',
					  options: [{v:'0',l:'Bouquets'},{v:'1',l: ywebL('bqe.channels', 'Channels')}] },
					{ key: 'key_tvradio_mode', label: ywebL('reg.tv_radio_switch', 'TV/radio switching'), type: 'number', min: 0 },
					{ key: 'key_lastchannel', label: ywebL('reg.last_channel', 'Last channel'), type: 'number', min: 0 },
					{ key: 'key_zaphistory', label: ywebL('reg.zap_history', 'Zap history'), type: 'number', min: 0 },
					{ key: 'key_bouquet_up', label: ywebL('reg.bq_up', 'Bouquet up'), type: 'number', min: 0 },
					{ key: 'key_bouquet_down', label: ywebL('reg.bq_down', 'Bouquet down'), type: 'number', min: 0 },
					{ key: 'key_quickzap_up', label: ywebL('reg.quickzap_up', 'Quickzap up'), type: 'number', min: 0 },
					{ key: 'key_quickzap_down', label: ywebL('reg.quickzap_down', 'Quickzap down'), type: 'number', min: 0 },
					{ key: 'key_subchannel_up', label: ywebL('reg.subch_up', 'Subchannel up'), type: 'number', min: 0 },
					{ key: 'key_subchannel_down', label: ywebL('reg.subch_down', 'Subchannel down'), type: 'number', min: 0 }
				]
			}
		]
	},
	{
		id: 'nhttpd',
		group: 'sys',
		icon: regIcon('<rect x="2" y="2" width="20" height="8" rx="2" ry="2"/><rect x="2" y="14" width="20" height="8" rx="2" ry="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/>'),
		label: 'Webserver',
		config: 'nhttpd',
		sections: [
			{
				title: ywebL('reg.access', 'Access'),
				fields: [
					{ key: 'mod_auth.authenticate', label: ywebL('reg.auth', 'Password protection'), type: 'bool' },
					{ key: 'mod_auth.username', label: ywebL('set.mnt_user', 'User'), type: 'text' },
					{ key: 'mod_auth.password', label: ywebL('set.mnt_pass', 'Password'), type: 'password' },
					{ key: 'mod_auth.no_auth_client', label: ywebL('reg.noauth_client', 'Without login (IP prefix)'), type: 'text',
					  hint: ywebL('reg.noauth_client_hint', 'matching addresses never see the login; local access (127.0.0.1) is always exempt') },
					{ key: 'WebsiteMain.port', label: 'Port', type: 'number', min: 1, max: 65535 }
				]
			},
			{
				title: ywebL('reg.content', 'Content'),
				fields: [
					{ key: 'WebsiteMain.override_directory', label: ywebL('reg.override_dir', 'Override directory'), type: 'text', picker: true, hint: ywebL('reg.override_dir_hint', 'files here override the default web root') },
					{ key: 'WebsiteMain.hosted_directory', label: ywebL('reg.hosted_dir', 'Hosted directory'), type: 'text', picker: true },
					{ key: 'webserver.threading', label: 'Threading', type: 'bool' },
					{ key: 'mod_sendfile.sendAll', label: ywebL('reg.send_all_types', 'Send all file types'), type: 'bool' }
				]
			}
		]
	},
	{
		id: 'yweb',
		group: 'sys',
		icon: regIcon('<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>'),
		label: 'Neutrino-Web-UI',
		config: 'yweb',
		sections: [
			{
				title: ywebL('reg.ui', 'User interface'),
				fields: [
					{ key: 'start_page', label: ywebL('reg.start_page', 'Start page'), type: 'text' },
					{ key: 'yweb_box_name', label: ywebL('reg.box_name', 'Box name'), type: 'text' },
					{ key: 'yweb_box_color', label: ywebL('reg.box_color', 'Box color'), type: 'text' }
				]
			},
			{
				title: ywebL('reg.screenshot_control', 'Screenshot and control'),
				fields: [
					{ key: 'screenshot_timeout', label: ywebL('reg.screenshot_timeout', 'Screenshot timeout (sec.)'), type: 'number', min: 0 },
					{ key: 'screenshot_rc', label: ywebL('reg.screenshot_rc', 'Screenshot via RC key'), type: 'text' },
					{ key: 'fb', label: ywebL('reg.fb_mode', 'Framebuffer mode'), type: 'text' },
					{ key: 'management_ip', label: 'Management-IP', type: 'text' }
				]
			}
		]
	}
];
