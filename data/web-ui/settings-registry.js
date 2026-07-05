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
		id: 'wol',
		label: 'Wake-on-LAN',
		custom: true
	},
	{
		id: 'xmltv',
		label: 'WebTV-Quellen',
		custom: true
	},
	{
		id: 'zapitupload',
		label: 'Zapit-Upload',
		custom: true
	},
	{
		id: 'camds',
		label: 'CAM-Daemons',
		custom: true
	},
	{
		id: 'backup',
		label: 'Sichern',
		custom: true
	},
	{
		id: 'personalize',
		label: 'Personalisierung',
		sections: [
			{
				title: 'PIN',
				fields: [
					{ key: 'personalize_pincode', label: 'PIN-Code', type: 'password' },
					{ key: 'personalize_pinstatus', label: 'PIN aktiv', type: 'bool01' }
				]
			},
			{
				title: 'Menuepunkte (Sichtbarkeit)',
				fields: [
					{ key: 'personalize_settings', label: 'Einstellungen', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_service', label: 'Service', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_audio', label: 'Audio', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_video', label: 'Video', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_network', label: 'Netzwerk', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_recording', label: 'Aufnahme', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_mediaplayer', label: 'Mediaplayer', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_games', label: 'Spiele', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_scants', label: 'Sendersuche', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_misc', label: 'Verschiedenes', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_sleeptimer', label: 'Sleeptimer', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_reboot', label: 'Reboot', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_restart', label: 'Neustart GUI', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_shutdown', label: 'Ausschalten', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_bluebutton', label: 'Blaue Taste', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] },
					{ key: 'personalize_redbutton', label: 'Rote Taste', type: 'select',
					  options: [{v:'0',l:'Versteckt'},{v:'1',l:'Sichtbar'},{v:'2',l:'PIN-geschuetzt'}] }
				]
			}
		]
	},
	{
		id: 'buttons',
		label: 'Tasten',
		sections: [
			{
				title: 'Tastenbelegung (RC-Keycodes)',
				fields: [
					{ key: 'bouquetlist_mode', label: 'Bouquetlisten-Modus', type: 'select',
					  options: [{v:'0',l:'Bouquets'},{v:'1',l:'Kanaele'}] },
					{ key: 'key_tvradio_mode', label: 'TV/Radio-Umschaltung', type: 'number', min: 0 },
					{ key: 'key_lastchannel', label: 'Letzter Kanal', type: 'number', min: 0 },
					{ key: 'key_zaphistory', label: 'Zap-Historie', type: 'number', min: 0 },
					{ key: 'key_bouquet_up', label: 'Bouquet hoch', type: 'number', min: 0 },
					{ key: 'key_bouquet_down', label: 'Bouquet runter', type: 'number', min: 0 },
					{ key: 'key_quickzap_up', label: 'Quickzap hoch', type: 'number', min: 0 },
					{ key: 'key_quickzap_down', label: 'Quickzap runter', type: 'number', min: 0 },
					{ key: 'key_subchannel_up', label: 'Subkanal hoch', type: 'number', min: 0 },
					{ key: 'key_subchannel_down', label: 'Subkanal runter', type: 'number', min: 0 }
				]
			}
		]
	},
	{
		id: 'infoicons',
		label: 'Info-Icons',
		sections: [
			{
				title: 'Status-Icons',
				fields: [
					{ key: 'mode_icons', label: 'Icons aktiv', type: 'bool01' },
					{ key: 'mode_icons_background', label: 'Hintergrund', type: 'bool01' },
					{ key: 'mode_icons_flag0', label: 'Icon-Datei 0', type: 'text' },
					{ key: 'mode_icons_flag1', label: 'Icon-Datei 1', type: 'text' },
					{ key: 'mode_icons_flag2', label: 'Icon-Datei 2', type: 'text' },
					{ key: 'mode_icons_flag3', label: 'Icon-Datei 3', type: 'text' },
					{ key: 'mode_icons_flag4', label: 'Icon-Datei 4', type: 'text' },
					{ key: 'mode_icons_flag5', label: 'Icon-Datei 5', type: 'text' },
					{ key: 'mode_icons_flag6', label: 'Icon-Datei 6', type: 'text' },
					{ key: 'mode_icons_flag7', label: 'Icon-Datei 7', type: 'text' }
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
