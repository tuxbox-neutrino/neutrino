/*	VLC abstraction by yjogol@online.de
	$Date: 2007/02/21 17:38:48 $
	$Revision: 1.1 $
*/
/*ie1=ActiveC, moz1=Mozilla<0.8.5.1, moz2>= 0.8.5.1*/
var CyVLC = function(_id, masterid, width, height) {
	this.initialize(_id, masterid, width, height);
};
CyVLC.prototype = {
	id : "vlc",
	vlc : "",
	plugin : "ie1",
	version_string : "",
	version_level1 : 0,
	version_level2 : 0,
	version_level3 : 0,
	version_level4 : 0,
	options : [],

	c_masterid : "vlc_view",
	c_width : 384,
	c_height: 288,
	c_loop : false,
	c_show_display : true,
	c_auto_play : false,

	initialize : function(_id, masterid, width, height) {
		this.id = _id;
		this.c_masterid = masterid;
		this.c_width = width;
		this.c_height = height;
		if(!is_ie) {
			this.version_string = this._get_version();
			this._generate_sub_versions();
			this._determine_plugin_generation();
		}
		this.insert_control();
		this.vlc = id(_id);
		this.set_resolution(this.c_width, this.c_height);
		if(is_ie) {
			this.version_string = this._get_version();
			this._generate_sub_versions();
			this._determine_plugin_generation();
		}
	},
	_get_version : function() {
		if(is_ie)
		{
			var vstr = this.vlc.VersionInfo;
			var words = vstr.split(" ");
			return words[0];
		}
		else
			if(navigator.plugins) {
				var plug = navigator.plugins['VLC multimedia plugin'];
				if(typeof plug == 'undefined')
					var plug = navigator.plugins['VLC Multimedia Plugin'];
				var ex = /^.*[vV]ersion [\"]*([^ \"]*)[\"]*.*$/;
				var ve = ex.exec(plug.description);
				if(ve[1])
					return ve[1];
				else
					return "0.0.0";
			}
			else
				return "0.0.0";
	},
	_generate_sub_versions : function() {
		if(this.version_string == "")
			return
		var ex = /([^\.]*)[\.]*([^\.]*)[\.]*([^\.-]*)[\.-]*([^\.]*).*$/;
		var ve = ex.exec(this.version_string);
		if(ve.length >1)	this.version_level1 = ve[1];
		if(ve.length >2)	this.version_level2 = ve[2];
		if(ve.length >3 && ve[3] != "")	this.version_level3 = ve[3];
		if(ve.length >4 && ve[4] != "")	this.version_level4 = ve[4];
	},
	_determine_plugin_generation : function() {
		if(is_ie)
			this.plugin = "ie1";
		else
			if(this.version_level1 <= "0" && this.version_level2 <= "8" && this.version_level3 <= "5")
				this.plugin = "moz1";
			else
				this.plugin = "moz2";
	},
	set_actual_mrl : function(mrl) {
		switch(this.plugin) {
			case "ie1":
				this.vlc.playlistClear();
				this.vlc.addTarget(mrl, this.options, 4+8, -666);
				break;
			case "moz2":
				this.vlc.playlist.clear();
				this.vlc.playlist.add(mrl, null, this.options);
				break;
			default:
				this.vlc.clear_playlist();
				this.vlc.add_item(mrl);
				break;
		}
	},
	play : function() {
		switch(this.plugin) {
			case "moz2": 	this.vlc.playlist.play();break;
			default:	this.vlc.play();break;
		}
	},
	stop : function() {
		switch(this.plugin) {
			case "moz2":	this.vlc.playlist.stop();break;
			default:	this.vlc.stop();break;
		}
	},
	pause : function() {
		switch(this.plugin) {
			case "moz2":
				if(this.vlc.playlist.isPlaying)
					this.vlc.playlist.togglePause();
				break;
			default: this.vlc.pause(); break;
		}
	},
	next : function() {
		switch(this.plugin) {
			case "moz1":	this.vlc.next();break;
			case "moz2":	this.vlc.playlist.next();break;
			default:	this.vlc.playlistNext();break;
		}
	},
	prev : function() {
		switch(this.plugin) {
			case "moz1":	this.vlc.playlist.previous();break;
			case "moz2":	this.vlc.playlist.prev();break;
			default:	this.vlc.playlistPrev();break;
		}
	},
	is_playing : function() {
		switch(this.plugin) {
			case "ie1":	return this.vlc.Playing;break;
			case "moz2":	return this.vlc.playlist.isPlaying;break;
			default:	return this.vlc.isplaying();break;
		}
	},
	toggle_fullscreen : function() {
		switch(this.plugin) {
			case "moz2":	this.vlc.video.toggleFullscreen();break;
			default:	this.vlc.fullscreen();break;
		}
	},
	set_volume : function(vol) {
		switch(this.plugin) {
			case "ie1":	this.vlc.volume = vol;break;
			case "moz2":	this.vlc.audio.volume = vol;break;
			default:	this.vlc.set_volume(vol);break;
		}
	},
	get_volume : function() {
		switch(this.plugin) {
			case "ie1":	return this.vlc.volume;break;
			case "moz2":	return this.vlc.audio.volume;break;
			default:	return this.vlc.get_volume();break;
		}
	},
	set_volume_delta : function(delta) {
		var new_vol = this.get_volume() + delta;
		new_vol = (new_vol >= 0) ? new_vol : 0;
		new_vol = (new_vol <= 200) ? new_vol : 200;
		this.set_volume(new_vol);
	},
	toggle_mute : function() {
		switch(this.plugin) {
			case "ie1":	this.vlc.toggleMute();reak;
			case "moz2":	this.vlc.audio.toggleMute();break;
			default:	this.vlc.mute();break;
		}
	},
	press_key : function(key) {
		switch(this.plugin) {
			case "ie1":
				var keyvalue = this.vlc.getVariable(key);
				this.vlc.setVariable("key-pressed", keyvalue);
				break;
			case "moz2":
				var keyvalue = this.vlc.get_int_variable(key);
				this.vlc.set_int_variable("key-pressed", keyvalue);
//				alert("not implemented for this version of vlc");
				break;
			default:
				var keyvalue = this.vlc.get_int_variable(key);
				this.vlc.set_int_variable("key-pressed", keyvalue);
				break;
		}
	},
	snapshot : function() {
		this.press_key("key-snapshot");
	},
	change_audio_channel : function() {
		this.press_key("key-audio-track");
	},
	direct_record : function() {
		this.press_key("key-record");
	},
	set_resolution : function (w,h) {
		this.vlc.width		= w;
		this.vlc.height		= h;
		this.vlc.style.width 	= w;
		this.vlc.style.height 	= h;
	},
	have_options : function() {
		switch(this.plugin) {
			case "ie1":
			case "moz2":	return true;break;
			default:	return false;break;
		}
	},
	insert_control : function()
	{
		var vlc_control_html = "";
		if(is_ie) {
			vlc_control_html =
				"<object classid=\"clsid:E23FE9C6-778E-49D4-B537-38FCDE4887D8\" " +
					"width=\""+this.c_width+"\" height=\""+this.c_height+"\" id=\""+this.id+"\" events=\"True\">" +
					"<param name='ShowDisplay' value='"+this.c_show_display+"' />" +
					"<param name='Loop' value='"+this.c_loop+"' />" +
					"<param name='AutoPlay' value='"+this.c_auto_play+"' />" +
					"<param name=\"Visible\" value=\"-1\"/>" +
					"The VideoLan Client ActiveX is not installed.<br/>"+
					"You need <a href='http://www.videolan.org' target='_blank'>VideoLan Client</a> V0.8.5 or higher.<br/>" +
					"Install with Option ActiveX." +
				"</object>";
		}
		else {
			vlc_control_html = "<embed type='application/x-vlc-plugin'";
			if(this.plugin == "moz2")
				vlc_control_html += "version=\"VideoLAN.VLCPlugin.2\"";
			vlc_control_html +=
				"id='"+this.id+"'"+
				"autoplay='"+this.c_auto_play+"' loop='"+this.c_loop+"' width='"+this.c_width+"' height='"+this.c_height+"'" +
				"target='' >" +
				"</embed>";
		}
		obj_update(this.c_masterid,vlc_control_html);
		this.vlc = id(this.id);
	}
};
