/*
 * Settings Registry — Central field definitions for settings.yhtm
 * Each category has: id, label, sections with fields
 * Field types: select, bool, bool01, number, text, password,
 *   info   { label, text | key }                    — readonly display
 *   action { id, label, button, confirm?, success?, refresh?,
 *            api: {endpoint, params, raw?} }        — v4 call button
 * Optional: showIf: {key, value} for conditional visibility
 */
var SETTINGS_REGISTRY = [
	{
		id: 'video',
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
		label: ywebL('reg.mounts', 'Network mounts'),
		custom: true
	},
	{
		id: 'plugins',
		label: 'Plugins',
		custom: true
	},
	{
		id: 'wol',
		label: 'Wake-on-LAN',
		custom: true
	},
	{
		id: 'xmltv',
		label: ywebL('reg.webtv_sources', 'WebTV sources'),
		custom: true
	},
	{
		id: 'zapitupload',
		label: 'Zapit-Upload',
		custom: true
	},
	{
		id: 'extensions',
		label: ywebL('set.ext_title', 'Extensions'),
		custom: true
	},
	{
		id: 'backup',
		label: ywebL('reg.backup', 'Backup'),
		custom: true
	},
	{
		id: 'personalize',
		label: ywebL('reg.personalize', 'Personalization'),
		sections: [
			{
				title: 'PIN',
				fields: [
					{ key: 'personalize_pincode', label: 'PIN-Code', type: 'password' },
					{ key: 'personalize_pinstatus', label: ywebL('reg.pin_active', 'PIN active'), type: 'bool01' }
				]
			},
			{
				title: ywebL('reg.menu_visibility', 'Menu items (visibility)'),
				fields: [
					{ key: 'personalize_settings', label: ywebL('nav.settings', 'Settings'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_service', label: 'Service', type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_audio', label: 'Audio', type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_video', label: 'Video', type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_network', label: ywebL('si.network', 'Network'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_recording', label: ywebL('bc.recording', 'Recording'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_mediaplayer', label: 'Mediaplayer', type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_games', label: ywebL('reg.games', 'Games'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_scants', label: ywebL('reg.channel_search', 'Channel search'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_misc', label: ywebL('reg.misc', 'Miscellaneous'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_sleeptimer', label: 'Sleeptimer', type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_reboot', label: 'Reboot', type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_restart', label: ywebL('bc.restart_btn', 'Restart GUI'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_shutdown', label: ywebL('bc.shutdown_btn', 'Power off'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_bluebutton', label: ywebL('reg.key_blue', 'Blue key'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] },
					{ key: 'personalize_redbutton', label: ywebL('reg.key_red', 'Red key'), type: 'select',
					  options: [{v:'0',l: ywebL('reg.hidden', 'Hidden')},{v:'1',l: ywebL('reg.visible', 'Visible')},{v:'2',l: ywebL('reg.pin_protected', 'PIN protected')}] }
				]
			}
		]
	},
	{
		id: 'buttons',
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
		label: 'Webserver',
		config: 'nhttpd',
		sections: [
			{
				title: ywebL('reg.access', 'Access'),
				fields: [
					{ key: 'mod_auth.authenticate', label: ywebL('reg.auth', 'Password protection'), type: 'bool' },
					{ key: 'mod_auth.username', label: ywebL('set.mnt_user', 'User'), type: 'text' },
					{ key: 'mod_auth.password', label: ywebL('set.mnt_pass', 'Password'), type: 'password' },
					{ key: 'mod_auth.no_auth_client', label: ywebL('reg.noauth_client', 'Without login (IP prefix)'), type: 'text' },
					{ key: 'WebsiteMain.port', label: 'Port', type: 'number', min: 1, max: 65535 }
				]
			},
			{
				title: ywebL('reg.content', 'Content'),
				fields: [
					{ key: 'WebsiteMain.override_directory', label: ywebL('reg.override_dir', 'Override directory'), type: 'text', picker: true, hint: ywebL('reg.override_dir_hint', 'files here override the default web root') },
					{ key: 'WebsiteMain.hosted_directory', label: ywebL('reg.hosted_dir', 'Hosted directory'), type: 'text', picker: true },
					{ key: 'Tuxbox.DisplayLogos', label: ywebL('reg.show_logos', 'Show channel logos'), type: 'bool' },
					{ key: 'Tuxbox.LogosURL', label: ywebL('reg.logos_url', 'Logos URL/path'), type: 'text' },
					{ key: 'webserver.threading', label: 'Threading', type: 'bool' },
					{ key: 'mod_sendfile.sendAll', label: ywebL('reg.send_all_types', 'Send all file types'), type: 'bool' }
				]
			}
		]
	},
	{
		id: 'yweb',
		label: 'yWeb',
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
