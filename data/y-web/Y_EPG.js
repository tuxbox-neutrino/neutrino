/*	yWeb EPG by yjogol
	$Date$
	$Revision$
*/
/*EPG+*/
var g_width_px=0; /*display width*/
//var g_cols_to_display=0; /*minutes to display*/
var g_number_of_cols=0; /*nr of cols*/
var g_width_all_items=0; /*width without bouquet*/
var c_width_px_per_min=3;	/* px per minute */
var c_min_per_col=15;/*minutes per col*/
var c_width_px_bouquet=100; /* width of bouquet*/
var c_slider_width=25;
var epg_data; /* all EPG Data in 2-dim Array*/
var epg_data_index=0;
var g_timer_eventids = new Array();
var g_selected=0;
//var epg_diag= new Y.Dialog('epg_info');

/* calc the dimension px and mins to display */
function epg_plus_calc_dimensions(){
	var show_dim=$('epg_plus').getDimensions();
	var usable_width_px = show_dim.width-c_slider_width; /*get display width*/
	var max_minutes_to_display = Math.round((usable_width_px-c_width_px_bouquet-c_width_px_per_min)/c_width_px_per_min); /* calc display minutes*/
	g_number_of_cols = Math.round(max_minutes_to_display/c_min_per_col);
	g_width_px = g_number_of_cols * c_width_px_per_min * c_min_per_col + c_width_px_bouquet + c_width_px_per_min;
	g_width_all_items=g_width_px-c_width_px_bouquet-c_width_px_per_min;
	$('epg_plus').style.cssText = "width:"+g_width_px;
}
function epg_zapto(){
	dbox_zapto($('d_channel_id').innerHTML);
}
function epg_set_timer_rec(){
	dbox_set_timer_rec($("d_channel_id").innerHTML, $("d_start").innerHTML, $("d_stop").innerHTML);
}
function epg_set_timer_zap(){
	dbox_set_timer_zap($("d_channel_id").innerHTML, $("d_start").innerHTML);
}
function build_epg_clear(){
	var ep = $("epg_plus");
	obj_clear_all_childs(ep);
}
/*set a layout box and content*/
function build_epg_setbox(_item, _starttime, _stoptime, _start, _stop){
	var d_start = Math.max(_start, _starttime);
	var d_stop = Math.min(_stop, _stoptime);
	var d_left = c_width_px_bouquet+c_width_px_per_min+ Math.round((d_start-_starttime) * c_width_px_per_min / 60);
	var d_width = Math.max(0,Math.round((d_stop-d_start) * c_width_px_per_min / 60)-3);
	d_width= Math.min(d_width,g_width_px-d_left);
	if(d_start<_stoptime)
		_item.style.cssText = "position:absolute; top:0px; left:"+d_left+"px; width:"+d_width+"px;";
}
/*show epg details*/
function show_epg_item(_index){
	g_selected=_index;
//epg_diag.show();
	$("d_desc").update(epg_data[_index][4]+" "+epg_data[_index][0]);
	$("d_info1").update(epg_data[_index][1]);
	$("d_info2").update(epg_data[_index][2]);
	$("d_start").update(epg_data[_index][3]);
	$("d_stop").update(epg_data[_index][5]);
	$("d_channel_id").update(epg_data[_index][6]);
	var logo =epg_data[_index][7];
	$('d_logo').update( (logo!="")?"<img class=\"channel_logos\" src=\""+logo+"\">":"" );
	var imdb_link = '<a target="_blank" class="exlink" href="http://german.imdb.com/find?s=all&q='+(epg_data[_index][0]).gsub(" ","+")+'">IMDb</a>';
	var klack_link = '<a target="_blank" class="exlink" href="http://www.klack.de/programmsuche.html?search=1&title='+(epg_data[_index][0]).gsub(" ","+")+'">klack.de</a>';
	var tvinfo_link = '<a target="_blank" class="exlink" href="http://www.tvinfo.de/exe.php3?quicksearch=1&volltext='+(epg_data[_index][0]).gsub(" ","+")+'&tpk=&showall=&genretipp=&target=list.inc">tvinfo.de</a>';
	$('d_lookup').update(imdb_link+" "+klack_link+" "+tvinfo_link);
	
	var off=$('epg_plus').cumulativeScrollOffset();
//	alert(off.inspect());
	$('epg_info').setStyle({
		'left':off.left+50+'px',
		'top':off.top+50+'px',
		'position': 'absolute'
//		'background-color': 'white'
	});
	show_obj("epg_info",true);
}
/* build one channel row*/
function build_epg_bouquet(__bdiv, __channel_id, _starttime, _stoptime, _logo)
{
	var xml = loadSyncURLxml("/control/epg?xml=true&channelid="+__channel_id+"}&details=true&stoptime="+_stoptime);
	if(xml){
		var prog_list = xml.getElementsByTagName('prog');
		for(var i=0;i<prog_list.length;i++){
			var prog = prog_list[i];

			var _stop = getXMLNodeItemValue(prog, "stop_sec");
			_stop = parseInt(_stop);
			if(_stop > _starttime){
				var _start_t	= getXMLNodeItemValue(prog, "start_t");
				var _start	= getXMLNodeItemValue(prog, "start_sec");
				_start = parseInt(_start);
				var _stop_t	= getXMLNodeItemValue(prog, "stop_t");
				var _desc	= epg_de_qout(getXMLNodeItemValue(prog, "description"));
				var _info1	= epg_de_qout(getXMLNodeItemValue(prog, "info1"));
				var _info2	= epg_de_qout(getXMLNodeItemValue(prog, "info2"));
				var __item = obj_createAt(__bdiv, "div", "ep_bouquet_item");

				var epg_obj= new Array(_desc, _info1, _info2, _start, _start_t, _stop.toString(), __channel_id, _logo);
				epg_data.push(epg_obj);
				__item.innerHTML = "<span onclick=\"show_epg_item('"+epg_data_index+"');\" title=\""+_start_t+" "+_desc+" (click for details)\">"+_desc+"</span>";
				build_epg_setbox(__item, _starttime, _stoptime, _start, _stop);
				epg_data_index++;
			}
		}
		/* look for   Timers*/
		var fou= g_timer_eventids.findAll(function(e){
			return e.get('channelid')==__channel_id; 
		});
		if(fou){
			fou.each(function(e){
				var stTime="0";
				var tclass="";
				if (e.get('eventType') == 3) {
					stTime=e.get('alarmTime');
					stTime = parseInt(stTime, 10) + 200;
					stTime = stTime.toString();
					tclass="ep_bouquet_zap";
				}
				else if (e.get('eventType') == 5) {/*record*/
					stTime=e.get('stopTime');
					tclass="ep_bouquet_rec";
				}
				var __item = obj_createAt(__bdiv, "div", tclass);
				build_epg_setbox(__item, _starttime, _stoptime, e.get('alarmTime'), stTime);
			});
		}
	}
}
/* build time row*/
function build_epg_time_bar(_tdiv, _starttime, _stoptime){
	var _start = _starttime;
	for(var i=0;i<g_number_of_cols;i++){
		var __item = obj_createAt(_tdiv, "div", "ep_time_bar_item");
		__item.innerHTML = format_time(new Date(_start*1000));
		var _stop = _start + (c_min_per_col * 60);
		build_epg_setbox(__item, _starttime, _stoptime, _start, _stop);
		_start = _stop;
	}
}
function get_timer(){
	g_timer_eventids = new Array();
	var timer = loadSyncURL("/control/timer?format=id");
	var lines=timer.split("\n");
	lines.each(function(e){
		var vals=e.split(" ");
		if(vals.length>=8 && (vals[1]==3||vals[5])){ /*record and zap*/
			var aTimer=$H({
				'eventID': vals[0],
				'eventType': vals[1],
				'eventRepeat': vals[2], 
				'repcount': vals[3],
				'announceTime': vals[4],
				'alarmTime': vals[5],
				'stopTime': vals[6],
				'channelid': vals[7]
			});
			g_timer_eventids.push(aTimer);
		}
	},this);
}

/* main */
var g_i = 0;
var g_bouquet_list;
var g_display_logos="";
function build_epg_plus(_bouquet, _starttime)
{
	build_epg_clear();
	epg_data = new Array();
	epg_data_index=0;
	var _bouquets_xml = loadSyncURLxml("/control/getbouquet?bouquet="+_bouquet+"&xml=true");
	if(_bouquets_xml){
		g_bouquet_list = _bouquets_xml.getElementsByTagName("channel");
		var ep = $("epg_plus");
		var _stoptime = _starttime + c_min_per_col * 60 * g_number_of_cols;
		var __tdiv = obj_createAt(ep, "div", "ep_time_bar");
		var __tname_div = obj_createAt(__tdiv, "div", "ep_time_bar_item");
		__tname_div.innerHTML = "Uhrzeit";
		__tname_div.style.cssText = "width:"+c_width_px_bouquet+"px;";
		build_epg_time_bar(__tdiv, _starttime, _stoptime);
		__tdiv.style.cssText = "width:"+g_width_px;
		var __ediv = obj_createAt(ep, "div", "epg_plus_container");
		__ediv.setAttribute("id", "epg_plus_container")
		g_i=0;
		window.setTimeout("build_epg_plus_loop("+_starttime+","+_stoptime+")",100);
	}
}
function build_epg_plus_loop(_starttime, _stoptime)
{
	if(g_i<g_bouquet_list.length){
		var _bouquet = g_bouquet_list[g_i];
		var __channel_name = getXMLNodeItemValue(_bouquet, "name");
		var __channel_id = getXMLNodeItemValue(_bouquet, "id");
		var __short_channel_id = getXMLNodeItemValue(_bouquet, "short_id");
		var __logo = getXMLNodeItemValue(_bouquet, "logo");
		var ep = $("epg_plus_container");
		var __bdiv = obj_createAt(ep, "div", "ep_bouquet");
		var __bname_div = obj_createAt(__bdiv, "div", "ep_bouquet_name");
		var ch_name_with_logo= (g_display_logos=="true")?"<img class=\"channel_logos\" src=\""+__logo+"\" title=\""+__channel_name+"\" alt=\""+__channel_name+"\" >":__channel_name;
		$(__bname_div).style.cssText = "width:"+c_width_px_bouquet+"px;";
		$(__bname_div).update("<a href=\"javascript:do_zap('"+__channel_id+"');\">"+ch_name_with_logo+"</a>");
		build_epg_bouquet(__bdiv, __channel_id, _starttime, _stoptime, __logo);
		window.setTimeout("build_epg_plus_loop("+_starttime+","+_stoptime+")",100);
		g_i++;
	}
	else{
		show_waitbox(false);
		obj_disable("btGet", false);
	}
}
/* main: build epg+ */
function build_epg_plus_main(){
	epg_plus_calc_dimensions();
	get_timer();
	show_obj("epg_info",false);
	show_waitbox(true);
	obj_disable("btGet", true);
	var sel=document.e.bouquets.selectedIndex;
	if(sel != -1)
		bou = document.e.bouquets[sel].value;
	else
		bou = 1;
	_secs=document.e.epg_time.value;
	_secs=parseInt(_secs);
	build_epg_plus(bou, _secs);
	/*document.getElementById("epg_plus").width = g_width_px;*/
}
/* change time offset and build epg+*/
function build_epg_plus_delta(_delta){
	if(document.e.epg_time.selectedIndex + _delta < document.e.epg_time.length && document.e.epg_time.selectedIndex + _delta >= 0)
		document.e.epg_time.selectedIndex += _delta;
	build_epg_plus_main();
}
/* time delta dropdown-list*/
function build_time_list(_delta){
	var now = new Date();
	now.setMinutes(0);
	now.setSeconds(0);
	now.setMilliseconds(0);
	now = new Date(now.getTime()+_delta*60*60*1000);
	var _secs = now/1000;
	var _hour = now.getHours();
	var et = document.getElementById("epg_time");
	for(i=0;i<24;i++){
		var _time = (_hour + i) % 24;
		if(_time < 10)
			_time = "0"+_time;
		_time += ":00";
		var _time_t = _secs + i * 3600;
		var __item = obj_createAt(et, "option", "ep_bouquet_item");
		__item.text = _time;
		__item.value = _time_t;
	}
}
/*init call*/
function epg_plus_init(_display_logos){
	g_display_logos = _display_logos;
	window.onresize=epg_plus_calc_dimensions;
	build_time_list(0);
}
/* ---*/
function do_zap(channelid){
	dbox_zapto(channelid);
}
