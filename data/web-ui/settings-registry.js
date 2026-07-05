/*
 * Settings Registry — Central field definitions for settings.html
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
				title: 'Videoformat',
				fields: [
					{ key: 'video_Format', label: 'Videoformat', type: 'select',
					  options: [{v:'0',l:'Auto'},{v:'1',l:'16:9'},{v:'2',l:'4:3 (LB)'},{v:'3',l:'4:3 (PS)'}] },
					{ key: 'video_43mode', label: '4:3-Modus', type: 'select',
					  options: [{v:'0',l:'Panscan'},{v:'1',l:'Panscan2'},{v:'2',l:'Letterbox'},{v:'3',l:'Vollbild'}] },
					{ key: 'vcr_AutoSwitch', label: 'VCR Autoschalter', type: 'bool' }
				]
			},
			{
				title: 'Ausgabe',
				fields: [
					{ key: 'video_dbdr', label: 'Deblocking/Deringing', type: 'select',
					  options: [{v:'0',l:'Aus'},{v:'1',l:'Deblock'},{v:'2',l:'Beides'}] },
					{ key: 'zappingmode', label: 'Umschaltmodus', type: 'select',
					  options: [{v:'0',l:'Stumm'},{v:'1',l:'Bild halten'},{v:'2',l:'Stumm bis Lock'},{v:'3',l:'Halten bis Lock'}] },
					{ key: 'hdmi_colorimetry', label: 'HDMI-Farbraum', type: 'select',
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
				title: 'Analogausgang',
				fields: [
					{ key: 'audio_AnalogMode', label: 'Analog-Modus', type: 'select',
					  options: [{v:'0',l:'Stereo'},{v:'1',l:'Mono links'},{v:'2',l:'Mono rechts'}] },
					{ key: 'audio_DolbyDigital', label: 'Dolby Digital', type: 'bool01' }
				]
			},
			{
				title: 'Digital-Durchleitung',
				fields: [
					{ key: 'ac3_pass', label: 'AC3 Passthrough', type: 'bool01' },
					{ key: 'dts_pass', label: 'DTS Passthrough', type: 'bool01' },
					{ key: 'hdmi_dd', label: 'HDMI Dolby Digital', type: 'select',
					  options: [{v:'0',l:'Aus'},{v:'1',l:'Auto'},{v:'2',l:'Erzwingen'}] },
					{ key: 'spdif_dd', label: 'SPDIF Dolby Digital', type: 'bool01' }
				]
			},
			{
				title: 'A/V-Synchronisation',
				fields: [
					{ key: 'avsync', label: 'A/V-Sync', type: 'select',
					  options: [{v:'0',l:'Aus'},{v:'1',l:'Ein'},{v:'2',l:'Audio Master'}] }
				]
			},
			{
				title: 'Lautstaerke',
				fields: [
					{ key: 'current_volume_step', label: 'Lautstaerke-Stufe', type: 'number', min: 1, max: 25 },
					{ key: 'start_volume', label: 'Start-Lautstaerke', type: 'number', min: -1, max: 100 },
					{ key: 'audio_volume_percent_ac3', label: 'AC3-Lautstaerke (%)', type: 'number', min: 0, max: 100 },
					{ key: 'audio_volume_percent_pcm', label: 'PCM-Lautstaerke (%)', type: 'number', min: 0, max: 100 },
					{ key: 'audiochannel_up_down_enable', label: 'Tonkanal waehlbar', type: 'bool' },
					{ key: 'audio_left_right_selectable', label: 'Links/Rechts waehlbar', type: 'bool' }
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
					{ key: 'lcd_power', label: 'LCD ein/aus', type: 'bool01' },
					{ key: 'lcd_inverse', label: 'Invertieren', type: 'bool01' },
					{ key: 'lcd_brightness', label: 'Helligkeit', type: 'number', min: 0, max: 255 },
					{ key: 'lcd_standbybrightness', label: 'Standby-Helligkeit', type: 'number', min: 0, max: 255 },
					{ key: 'lcd_contrast', label: 'Kontrast', type: 'number', min: 0, max: 63 },
					{ key: 'lcd_dim_time', label: 'Dimm-Timeout (Sek.)', type: 'number', min: 0 },
					{ key: 'lcd_dim_brightness', label: 'Dimm-Helligkeit', type: 'number', min: 0, max: 255 },
					{ key: 'lcd_epgmode', label: 'EPG-Anzeige', type: 'select',
					  options: [{v:'0',l:'Standard'},{v:'1',l:'Sendung'},{v:'2',l:'Kanal/Sendung'},
					            {v:'3',l:'Kanal/Trennl./Sendung'},{v:'7',l:'Kanal(kurz)/Sendung'},
					            {v:'11',l:'Kanal(kurz)/Trennl./Sendung'},{v:'15',l:'Alles'}] },
					{ key: 'lcd_show_volume', label: 'Statuszeile', type: 'select',
					  options: [{v:'0',l:'Fortschritt'},{v:'1',l:'Lautstaerke'},{v:'2',l:'Beides'},{v:'3',l:'+Audio'}] }
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
					{ key: 'glcd_enable', label: 'GLCD ein/aus', type: 'bool01' },
					{ key: 'glcd_inverse', label: 'Invertieren', type: 'bool01' },
					{ key: 'glcd_brightness', label: 'Helligkeit', type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_standby', label: 'Standby-Helligkeit', type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_dim', label: 'Dimm-Helligkeit', type: 'number', min: 0, max: 10 },
					{ key: 'glcd_brightness_dim_time', label: 'Dimm-Timeout (Sek.)', type: 'number', min: 0, max: 60 }
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
					{ key: 'hdmi_cec_mode', label: 'CEC-Modus', type: 'select',
					  options: [{v:'0',l:'Aus'},{v:'1',l:'Tuner'},{v:'2',l:'Recorder'}] },
					{ key: 'hdmi_cec_view_on', label: 'TV einschalten', type: 'bool01' },
					{ key: 'hdmi_cec_standby', label: 'TV Standby', type: 'bool01' },
					{ key: 'hdmi_cec_volume', label: 'Lautstaerke', type: 'select',
					  options: [{v:'0',l:'Aus'},{v:'1',l:'Audiosystem'},{v:'2',l:'TV'}] }
				]
			}
		]
	},
	{
		id: 'mounts',
		label: 'Netzlaufwerke',
		custom: true
	},
	{
		id: 'plugins',
		label: 'Plugins',
		custom: true
	},
	{
		id: 'backup',
		label: 'Sichern',
		custom: true
	},
	{
		id: 'nhttpd',
		label: 'Webserver',
		config: 'nhttpd',
		sections: [
			{
				title: 'Zugriff',
				fields: [
					{ key: 'mod_auth.authenticate', label: 'Passwortschutz', type: 'bool' },
					{ key: 'mod_auth.username', label: 'Benutzer', type: 'text' },
					{ key: 'mod_auth.password', label: 'Passwort', type: 'password' },
					{ key: 'mod_auth.no_auth_client', label: 'Ohne Login (IP-Prefix)', type: 'text' },
					{ key: 'WebsiteMain.port', label: 'Port', type: 'number', min: 1, max: 65535 }
				]
			},
			{
				title: 'Inhalte',
				fields: [
					{ key: 'WebsiteMain.override_directory', label: 'Override-Verzeichnis', type: 'text' },
					{ key: 'WebsiteMain.hosted_directory', label: 'Hosted-Verzeichnis', type: 'text' },
					{ key: 'Tuxbox.DisplayLogos', label: 'Senderlogos anzeigen', type: 'bool' },
					{ key: 'Tuxbox.LogosURL', label: 'Logos-URL/-Pfad', type: 'text' },
					{ key: 'webserver.threading', label: 'Threading', type: 'bool' },
					{ key: 'mod_sendfile.sendAll', label: 'Alle Dateitypen senden', type: 'bool' }
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
				title: 'Oberflaeche',
				fields: [
					{ key: 'start_page', label: 'Startseite', type: 'text' },
					{ key: 'yweb_box_name', label: 'Box-Name', type: 'text' },
					{ key: 'yweb_box_color', label: 'Box-Farbe', type: 'text' }
				]
			},
			{
				title: 'Screenshot & Steuerung',
				fields: [
					{ key: 'screenshot_timeout', label: 'Screenshot-Timeout (Sek.)', type: 'number', min: 0 },
					{ key: 'screenshot_rc', label: 'Screenshot per RC-Taste', type: 'text' },
					{ key: 'fb', label: 'Framebuffer-Modus', type: 'text' },
					{ key: 'management_ip', label: 'Management-IP', type: 'text' }
				]
			}
		]
	}
];
