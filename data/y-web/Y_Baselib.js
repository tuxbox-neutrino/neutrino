/*	yWeb Baselib by yjogol
	$Date$
	$Revision$
*/
var baselib_version="2.0.0";
var tmp = document.documentMode, e, isIE;
// Try to force this property to be a string.
try{document.documentMode = "";}
catch(e){ };
// If document.documentMode is a number, then it is a read-only property, and so
// we have IE 8+.
// Otherwise, if conditional compilation works, then we have IE < 11.
// Otherwise, we have a non-IE browser.
isIE = typeof document.documentMode == "number" || new Function("return/*@cc_on!@*/!1")( );
// Switch back the value to be unobtrusive for non-IE browsers.
try{document.documentMode = tmp;}
catch(e){ };

/*DHTML-Basics*/
function $yN(_obj_name)
{
	return $(document.getElementsByName(_obj_name));
}
function id(obj_id)
{
	return document.getElementById(obj_id);
}
function obj_update(obj_id, html)
{
	var obj = id(obj_id);
	if(obj)
		obj.innerHTML = html;
}
function yClientHeight()
{
	if(window.innerHeight)
		return window.innerHeight;
	else if (document.documentElement && document.documentElement.clientHeight)
		return document.documentElement.clientHeight;
	else if (document.body)
		return document.body.clientHeight;
}
function yClientWidth()
{
	if(window.innerWidth)
		return window.innerWidth;
	else if (document.documentElement && document.documentElement.clientWidth)
		return document.documentElement.clientWidth;
	else if (document.body)
		return document.body.clientWidth;
}
function obj_create(_typ, _class)
{
	var __obj 	= document.createElement(_typ);
	var __class 	= document.createAttribute("class");

	__class.nodeValue = _class;
	__obj.setAttributeNode(__class);
	return __obj;
}
function obj_createAt(_parent, _typ, _class)
{
	var __obj = obj_create(_typ, _class);
	_parent.appendChild(__obj);
	return __obj;
}
function obj_get_radio_value(_obj_name)
{
	var _obj = document.getElementsByName(_obj_name);
	if(_obj)
	{
		for(i=0;i<_obj.length;i++)
			if(_obj[i].checked)
				return _obj[i].value;
	}
	return "";
}
function obj_set_radio_value(_obj_name, _value)
{
	var _obj = document.getElementsByName(_obj_name);
	if(_obj)
	{
		for(i=0;i<_obj.length;i++)
			_obj[i].checked = (_obj[i].value == _value);
	}
}
function obj_clear_all_childs(_obj)
{
	while(_obj.childNodes.length > 0)
	{
		var aChild = _obj.firstChild;
		if(aChild)
			_obj.removeChild(aChild);
	}
}
/*DHTML-Table*/
function y_add_row_to_table(_table, _class)
{
	var __row=document.createElement("tr");
	var __class = document.createAttribute("class");
	__class.nodeValue = _class;
	__row.setAttributeNode(__class);
	_table.appendChild(__row);
	return __row;
}
function y_add_plain_cell_to_row(_row, _name)
{
	var __cell=document.createElement("td");
	__cell.setAttribute("name", _name);
	_row.appendChild(__cell);
	return __cell;
}
function y_add_text_cell_to_row(_row, _name, _value)
{
	var __cell=y_add_plain_cell_to_row(_row, _name);
	var __text=document.createTextNode(_value);
	__cell.appendChild(__text);
	return __cell;
}
function y_add_html_cell_to_row(_row, _name, _value)
{
	var __cell=y_add_plain_cell_to_row(_row, _name);
	__cell.innerHTML = _value;
	return __cell;
}
function y_add_li_to_ul(_ul, _class, _value){
	var __li=document.createElement("li");
	var __class = document.createAttribute("class");
	__class.nodeValue = _class;
	_ul.setAttributeNode(__class);
	_ul.appendChild(__li);
	__li.innerHTML=_value;
	return __li;
}
function getXMLNodeItemValue(node, itemname)
{

	var item = node.getElementsByTagName(itemname);
	if(item.length>0)
		if(item[0].firstChild)
			return item[0].firstChild.nodeValue;
	return "";
}
function setInnerHTML(_id, _html)
{
	var item = document.getElementById(_id);
	if(item)
		item.innerHTML = _html;
}

function obj_addAttributeNode(_obj, _attr, _value)
{
	var __attr = document.createAttribute(_attr);
	__attr.nodeValue = _value;
	_obj.setAttributeNode(__attr);
}
/*XMLHttpRequest AJAX*/
var g_req;
function loadXMLDoc(_url, _processReqChange)
{
	if (window.XMLHttpRequest)
	{
		g_req = new XMLHttpRequest();
		g_req.onreadystatechange = _processReqChange;
		if(g_req.overrideMimeType)
		{	g_req.overrideMimeType('text/xml');}
		g_req.open("GET", _url, true);
		g_req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
		g_req.send(null);
	}
	else if (window.ActiveXObject)
	{
		g_req = new ActiveXObject("Microsoft.XMLHTTP");
		if (g_req)
		{
			g_req.onreadystatechange = _processReqChange;
			g_req.open("GET", _url, true);
			g_req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
			g_req.send();
		}
	}
	else
		alert("No Browser-support for XMLHttpRequest");
}
function loadSyncURL2(_url)
{
	var myAjax = new Ajax.Request(
	_url,
	{
		method: 'post',
		asynchronous: false
	});
	return myAjax.responseText;
}
function loadSyncURL(_url)
{
	var _req;
	if (window.XMLHttpRequest)
	{
		_req = new XMLHttpRequest();
		_req.open("GET", _url, false);
		_req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
		_req.send(null);
	}
	else if (window.ActiveXObject)
	{
		_req = new ActiveXObject("Microsoft.XMLHTTP");
		if(_req)
		{
			_req.open("GET", _url, false);
			_req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
			_req.send();
		}
	}
	else
		alert("No Browser-support for XMLHttpRequest");
	if (_req.readyState == 4 && _req.status == 200)
		return _req.responseText;
	else
		return "";
}
function loadSyncURLxml(_url)
{
	var _req;
	if (window.XMLHttpRequest)
	{
		_req = new XMLHttpRequest();
		_req.open("GET", _url, false);
		_req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
		_req.send(null);
	}
	else if (window.ActiveXObject)
	{
		_req = new ActiveXObject("Microsoft.XMLHTTP");
		if(_req)
		{
			_req.open("GET", _url, false);
			_req.setRequestHeader("If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT");
			_req.send();
		}
	}
	else
		alert("No Browser-support for XMLHttpRequest");
	if (_req.readyState == 4 && _req.status == 200)
		return _req.responseXML;
	else
		return "";
}
/*visibility*/
function obj_disable(_obj_name, _disable)
{
	var __obj = document.getElementById(_obj_name);
	__obj.disabled = _disable;
}
function obj_enable(_obj_name, _disable)
{
	obj_disable(_obj_name, !_disable);
}
function show_obj(_obj_name, _show)
{
	var __obj = document.getElementById(_obj_name);
	__obj.style.visibility= (_show) ? "visible" : "hidden";
}
function display_obj(_obj_name, _display)
{
	var __obj = document.getElementById(_obj_name);
	__obj.style.display = (_display) ? "block" : "none";
}
function show_waitbox(_show)
{
	show_obj("wait", _show);
}

/*Strings*/
function l_trim(_str)
{
	return _str.replace(/\s*((\S+\s*)*)/, "$1");
}
function r_trim(_str)
{
	return _str.replace(/((\s*\S+)*)\s*/, "$1");
}
function trim(_str)
{
	return l_trim(r_trim(_str));
}
function split_one(_str, _delimiter)
{
	var __p = _str.indexOf(_delimiter);
	var __left = _str.substring(0, __p);
	var __right = _str.substring(__p+_delimiter.length);
	return new Array(__left, __right);
}
function split_left(_str, _delimiter)
{
	var res = split_one(_str, _delimiter);
	return res[0];
}
function split_right(_str, _delimiter)
{
	var res = split_one(_str, _delimiter);
	return res[1];
}
function de_qout(_str)
{
	_str = _str.replace(/\"/g,"&quot;");
	_str = _str.replace(/'/g,"&quot;");
	return _str;
}
function epg_de_qout(_str)
{
	_str = de_qout(_str);
	_str = _str.replace(/\x8A/g,"<br/>");
	return _str;
}
function split_version(vstring,v){
	var l=vstring.split(".");
	v.set('major', (l.length>0)?l[0]:"0");
	v.set('minor', (l.length>1)?l[1]:"0");
	v.set('patch', (l.length>2)?l[2]:"0");
	v.set('pre', (l.length>3)?l[3]:"0");
}
function version_less(l, r) /* l<= r?*/{
	return 	(l.get('major') < r.get('major'))||
		((l.get('major') == r.get('major')) && (l.get('minor') < r.get('minor'))) ||
		((l.get('major') == r.get('major')) && (l.get('minor') == r.get('minor')) && (l.get('patch') < r.get('patch'))) ||
		((l.get('major') == r.get('major')) && (l.get('minor') == r.get('minor')) && (l.get('patch') == r.get('patch')) && (l.get('pre') < r.get('pre')));
}
function version_le(l, r) /* l<= r?*/{
	return 	(l.get('major') < r.get('major'))||
		((l.get('major') == r.get('major')) && (l.get('minor') < r.get('minor'))) ||
		((l.get('major') == r.get('major')) && (l.get('minor') == r.get('minor')) && (l.get('patch') < r.get('patch'))) ||
		((l.get('major') == r.get('major')) && (l.get('minor') == r.get('minor')) && (l.get('patch') == r.get('patch')) && (l.get('pre') <= r.get('pre')));
}
function version_str_less(l, r) /* l<= r?*/{
	var lh=$H(); 
	split_version(l,lh);
	var rh=$H();
	split_version(r,rh);
	return version_less(lh,rh);
}
function str_to_hash(str){
	var h=new Hash();
	var items=str.split(",");
	items.each(function(e){
		pair=split_one(e,":");
		if(pair.length==2)
			h.set((pair[0]).gsub("\"","").gsub("'","").strip(),(pair[1]).strip().gsub("\"","").gsub("'",""));
	});
	return h;
}
function hash_to_str(h){
	var str="";
	h.each(function(e){
		if(str!="")str+=",";
		str+=e.key+":"+e.value;
	});
	return str;
}
/*etc*/
function format_time(_t)
{
	var hour = _t.getHours();
	var min = _t.getMinutes();
	if(hour < 10)
		hour = "0" + hour;
	if(min < 10)
		min = "0" + min;
	return hour + ":" + min;
}

function bt_get_value(_bt_name)
{
	var __button = document.getElementById(_bt_name);
	if(__button)
		return __button.firstChild.nodeValue;
	else
		return "";
}
function bt_set_value(_bt_name, _text)
{
	var __button = document.getElementById(_bt_name);
	__button.firstChild.nodeValue = _text;
}
/*dbox*/
/*expermental*/
function reload_neutrino_conf() {
	loadSyncURL("/control/reloadsetup");
}
function dbox_rcsim(_key){
	loadSyncURL("/control/rcem?" + _key);
}
function dbox_reload_neutrino(){
	var sc=dbox_exec_tools("restart_neutrino");
}
function dbox_exec_command(_cmd)
{
	alert("Function dbox_exec_command is deactivated for security reasons");
	var __cmd = _cmd.replace(/ /g, "&");
//	return loadSyncURL("/control/exec?Y_Tools&exec_cmd&"+__cmd);
}
function dbox_exec_tools(_cmd)
{
	var __cmd = _cmd.replace(/ /g, "&");
	return loadSyncURL("/control/exec?Y_Tools&"+__cmd);
}
function dbox_message(_msg)
{
	return loadSyncURL("/control/message?nmsg="+_msg);
}
function dbox_popup(_msg)
{
	return loadSyncURL("/control/message?popup="+_msg);
}
function dbox_set_timer_rec(_channel_id, _start, _stop)
{
	var _url = "/control/timer?action=new&type=5&alarm="+_start+"&stop="+_stop+"&announce="+_start+"&channel_id="+_channel_id+"&rs=1";
	return loadSyncURL(_url);
}
function dbox_set_timer_zap(_channel_id, _start)
{
	var _url = "/control/timer?action=new&type=3&alarm="+_start+"&channel_id="+_channel_id;
	return loadSyncURL(_url);
}
function dbox_zapto(_channel_id)
{
	var _url = "/control/zapto?"+_channel_id;
	return loadSyncURL(_url);
}
function dbox_spts_status()
{
	return (trim(loadSyncURL("/control/system?getAViAExtPlayBack")) == "1");
}
function dbox_spts_set(_on)
{
	return loadSyncURL("/control/system?setAViAExtPlayBack=" + (_on ? "spts" : "pes") );
}
function dbox_getmode()
{
	return trim( loadSyncURL("/control/getmode") );
}
function dbox_setmode(_mode)
{
	return loadSyncURL("/control/setmode?" + _mode);
}
/*live*/
function live_kill_streams()
{
	dbox_exec_command("killall streamts");
	dbox_exec_command("killall streampes");
}
function live_switchto(_mode)
{
	//live_kill_streams();
	var _actual_spts = dbox_spts_status();
	if(_mode == "tv" && !_actual_spts)
		dbox_spts_set(true);
	else if(_mode == "radio" && _actual_spts)
		dbox_spts_set(false);

	var _actual_mode = dbox_getmode();
	if(_actual_mode != _mode)
		dbox_setmode(_mode);
}
function live_lock()
{
	loadSyncURL("/control/lcd?lock=1&clear=1&rect=10,10,110,50,1,0&xpos=20&ypos=27&size=22&font=2&text=%20%20%20%20yWeb%0A%20%20LiveView&update=1");
	loadSyncURL("/control/rc?lock");
	loadSyncURL("/control/zapto?stopplayback");
}
function live_unlock()
{
	loadSyncURL("/control/lcd?lock=0");
	loadSyncURL("/control/rc?unlock");
	loadSyncURL("/control/zapto?startplayback");
}
function yhttpd_cache_clear(category)
{
	if(category == "")
		loadSyncURL("/y/cache-clear");
	else
		loadSyncURL("/y/cache-clear?category="+category);
}

function saveTextAsFile(content, filename, filetype)
{
	var textFileAsBlob = new Blob([content], { type: filetype });
	var downloadLink = document.createElement("a");
	downloadLink.download = filename;
	downloadLink.innerHTML = "Download File";
	if (window.webkitURL != null)
	{
		// Chrome allows the link to be clicked
		// without actually adding it to the DOM.
		downloadLink.href = window.webkitURL.createObjectURL(textFileAsBlob);
	}
	else
	{
		// Firefox requires the link to be added to the DOM
		// before it can be clicked.
		downloadLink.href = window.URL.createObjectURL(textFileAsBlob);
		downloadLink.onclick = function() { this.parentNode.removeChild(this); };
		downloadLink.style.display = "none";
		document.body.appendChild(downloadLink);
	}
	downloadLink.click();
}

function glcdscreenshot(_filename)
{
	return loadSyncURL("/control/glcdscreenshot?name="+_filename);
}
