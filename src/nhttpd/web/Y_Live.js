/*	yWeb Live by yjogol
	$Date$
	$Revision$
*/
/*globals*/
var V2 = null;
var isUDP = false;
var IsTimeshift = true; //TEST
var IsLocked = false;
var IsMute = false;
var vlc_width = 384;
var vlc_height = 288;
var Window_delta_w = 0;
var Window_delta_h = 0;
var Mode = "tv";
var AudioChannel = 0;
var isSubs=false;
var g_intervall=null;
var current_channel = 0;
/*resize,init*/
function do_onload(){
	if(Mode != "tv" && Mode != "radio")
		Mode = "tv";
	set_controls("disable");
	if(isUDP && Mode == "tv")
		change_button_img('udp',"udp_switch_off");
	else if(Mode == "tv")
		change_button_img('udp',"udp_switch_on");
	window.setTimeout("do_init()",100);
}
function do_onresize(){
	if((vlc_width+Window_delta_w != window.innerWidth) || (vlc_height+Window_delta_h != window.innerHeight)){
		window.onresize=null;
		do_resize_vlc(yClientWidth()-Window_delta_w,yClientHeight()-Window_delta_h);
		window.onresize=do_onresize;
	}
}
function do_resize_vlc(width, height){
	vlc_width = width;
	vlc_height = height;
	V2.set_resolution(width, height);
}
function do_resize(){
	var dd = id('resolution');
	var sel = dd.selectedIndex;
	var w = -1;
	var h = -1;
	if(sel != -1){
		switch(sel.toString()) {
			case "0": w=384; h=288; break;
			case "1": w=768; h=576; break;
			case "2": w=960; h=720; break;
			case "3": w=1152; h=864; break;
		}
	}
	if(w != -1)
		window.resizeTo(w+Window_delta_w, h+Window_delta_h);
}
function do_init(){
	live_switchto(Mode);
//	vlc_width = 384;
//	vlc_height = 288;
	Window_delta_w = yClientWidth() - vlc_width;
	Window_delta_h = yClientHeight() - vlc_height;

	if(Mode == "tv")
		window.onresize=do_onresize;
	insert_vlc_control();
	build_bouquet_list(-1);
	set_controls("play");
//	do_play();
	change_channel_play();
//	g_intervall = window.setInterval('i_interval()', 20000);
}
function i_interval(){
	obj_update('bouquets_div', "<img src=\"/images/smallwait.gif\"/> "+Lgetting_bouquets);
	window.setTimeout("i_interval2()",100);
}
function i_interval2(){
	build_bouquet_list(-1);
}
function always_on_top() {	window.focus();
	window.setTimeout("always_on_top()",100);
}
function insert_vlc_control(){
	if(Mode == "radio") {
		var radio_html="<div style=\"width:"+vlc_width+"px;height:"+vlc_height+"px;text-align:center;\">"
				+"<br/><br/><br/><br/><br/><br/><div id=\"vlc_radio\"></div><br/><h1>Radio</h1></div>";
		obj_update('vlc_view',radio_html);
		V2 = new CyVLC('vlc', 'vlc_radio', 1, 1);
	} else
		V2 = new CyVLC('vlc', 'vlc_view', vlc_width, vlc_height);
}
function insert_message_control(msg){
	var wait_html="<div style=\"width:"+vlc_width+"px;height:"+vlc_height+"px;text-align:center;\">"
			+"<br/><br/><br/><br/><br/><br/><img src=\"/images/wait.gif\"><br/>"+msg+"</div>";
	obj_update('vlc_view',wait_html);
}
function change_button_img(id,img){
	var imgstr = "<img src=\"/images/"+img+".png\" width=\"16\" height=\"16\">";
	obj_update(id, imgstr);
}
/*controls*/
function set_controls(state){
	var go=false;
	var play=false;
	var opt=false;
	if(V2 && V2.have_options())
		opt=true;
	var plugin="";
	if(V2) plugin=V2.plugin;

	switch(state){
	case "disable":
		go=false;
		play=false;
		break;
	case "play":
		go=true;
		play=true;
		break;
	case "stop":
		go=true;
		play=false;
		break;
	}
	obj_enable('go', go);
	obj_enable('epg', go);
	obj_enable('PlayOrPause', go);
	obj_enable('stop', go);
	obj_enable('mute', play);
	obj_enable('volumeup', play);
	obj_enable('volumedown', play);
	obj_enable('livelock', go);
	if(LiveTyp!="popup"){
		show_obj('rec',opt);
		obj_enable('rec', play && opt);
		show_obj('transcode',opt);
		obj_enable('transcode', play && opt);
	}
	if(Mode == "tv"){
		show_obj('udp',haveUDP);
		obj_enable('udp', go);
		obj_enable('fullscreen', play);
		show_obj('snapshot',(plugin != "moz3"));
		obj_enable('snapshot', play && (plugin != "moz3"));
	}
}
/*vlc control*/
function do_play(){
	var options = new Array();
	if(Mode == "tv"){
		if(isDeinterlace){
			options.push(":vout-filter=deinterlace");
			options.push(":deinterlace-mode=bob");
		}
		if(isUDP && Mode == "tv"){
			options.push(":access-filter=timeshift");
		}
		else
			if(cachetime > 0)
				options.push(":http-caching="+cachetime);
		if(AudioChannel != 0)
			options.push(":audio-track="+AudioChannel);
	}
	do_play_state(0, options);
}
function start_udp_server(){
	var pids = loadSyncURL("/control/yweb?video_stream_pids=0&no_commas=true");
	var args = ClientAddr+" 31330 0 "+pids;
	var _cmd = "udp_stream start "+args;
	var __cmd = _cmd.replace(/ /g, "&");
	loadXMLDoc("/control/exec?Y_Live&"+__cmd, dummy);
}
function do_stop(){
	V2.stop();
	while(V2.is_playing())
		;
	change_button_img('PlayOrPause',"play");
	set_controls("stop");
}
function dummy()
{
}
// VCL does not work with prototype.js!
function do_play_bystring(_str){
	do_play_state(1,[_str]);
}
function do_play_state(_state, _options){
	change_button_img('PlayOrPause',"pause");
	_options.push(":input-repeat=1");
//	alert("options:"+_options);
	V2.options = _options;
	var mrl = "";
	if(Mode == "tv" && isUDP)
		mrl = "udp://@:31330";
	else
		if(current_channel)
			mrl = "http://" + window.location.host + ":31339/id=" + current_channel;
		else
			mrl = loadSyncURL("/control/build_live_url");
	  
	V2.set_actual_mrl(mrl);
	V2.play();
	V2.next();
	set_controls("play");
	if(isDeinterlace && V2.plugin=="moz2"){
		V2.vlc.video.deinterlace.enable("bob");
	}
	if(Mode == "tv" && isUDP)
		window.setTimeout("start_udp_server()",1000);
}
function do_play_or_pause(){
	if(V2.is_playing()) {
		change_button_img('PlayOrPause',"play");
		V2.pause();
		set_controls("stop");
	} else {
		change_button_img('PlayOrPause',"pause");
		V2.play();
		set_controls("play");
		if(isUDP)
			window.setTimeout("start_udp_server()",1000);
	}
}
/* bouquet & channel panel */
function change_bouquet(){
	var dd = id('bouquets');
	var bouquet = -1;
	var channel = -1;
	var sel = dd.selectedIndex;
	if(sel != -1){
		bouquet = dd[sel].value;
		channel = 0;
	}
	obj_update('channels_div', "<img src=\"/images/smallwait.gif\"/> "+Lgetting_channels);
	window.setTimeout("build_channel_list("+bouquet+", "+channel+")",2000);
}
function change_channel(){
	isSubs=false;
	var dd = id('channels');
	var channel = -1;
	var sel = dd.selectedIndex;
	if(sel != -1){
		channel = dd[sel].value;
		current_channel = channel;
	}
	do_stop();
	AudioChannel = 0;
	window.setTimeout("change_channel_zapto(\""+channel+"\")",100);
}
function change_sub_channel(){
	var dd = id('subs');
	var channel = -1;
	var sel = dd.selectedIndex;
	if(sel != -1){
		channel = dd[sel].value;
		current_channel = channel;
	}
	do_stop();
	AudioChannel = 0;
	window.setTimeout("change_channel_zapto(\""+channel+"\")",100);
}
function change_channel_zapto(channel){
	//dbox_zapto(channel);
	window.setTimeout("change_channel_play()",500);
}
function build_subchannels(){
	var subs = loadSyncURL("/control/zapto?getallsubchannels");
	if (subs != "") {
		var optlist="";
		var list = subs.split("\n");
		for(i=0;i<list.length;i++){
			var sc=split_one(list[i], " ");
			optlist+="<option value=\""+sc[0]+"\">"+sc[1]+"</option>\n";
		}
		optlist="<select id='subs' class='y_live_channels'>"+optlist+"</select>";
		id('subs_div').innerHTML = optlist;
		display_obj("subsRow", true);
		isSubs=true;
	}
	else
		display_obj("subsRow", false);
}
function change_channel_play(){
	insert_vlc_control();
	do_play();
	if (V2.have_options() && Mode == "tv") {
		build_audio_pid_list();
		if(!isSubs)
			build_subchannels();
	}
}
/*other buttons*/
function do_show_version(){
	alert("Version:"+V2.version_string+" Generation:"+V2.plugin+"\nlevel1:"+V2.version_level1+" 2:"+V2.version_level2
		+" 3:"+V2.version_level3+" 4:"+V2.version_level4);
}
function do_lock_toggle(){
	if( !IsLocked ) {
		change_button_img('livelock',"liveunlock");
		IsLocked = true;
		live_lock();
	} else {
		change_button_img('livelock',"livelock");
		IsLocked = false;
		live_unlock();
	}
}
function do_mute_toggle(){
	change_button_img('mute', (IsMute)?"volumemute":"volumeunmute");
	IsMute = !IsMute;
	V2.toggle_mute();
}
function do_udp_toggle(){
	change_button_img('udp', (isUDP)?"udp_switch_on":"udp_switch_off");
	isUDP = !isUDP;
	do_stop();
	do_play();
}
function view_streaminfo(){
	window.open("/fb/info.dbox2","streaminfo","width=400,height=400");
}
function doChangeAudioPid(){
	var dd = id('audiopid');
	AudioChannel = dd.selectedIndex;
	do_stop();
//	insert_message_control("... zapping ...");
	window.setTimeout("change_channel_play()",100);
}
function build_audio_pid_list(){
	var audio_pids_url = "/y/cgi?execute=func:get_audio_pids_as_dropdown";
	var audio_pid_list = loadSyncURL(audio_pids_url);
	audio_pid_list = "<select size=\"1\" class=\"y_live_audio_pids\" id=\"audiopid\" onChange=\"doChangeAudioPid()\">"
			+ audio_pid_list
			+ "</select>";
	obj_update('audio_pid_list', audio_pid_list);
}
