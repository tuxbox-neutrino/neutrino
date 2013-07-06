function do_submit()
{
	show_waitbox(true);
	document.f.submit();
}
/*sLog*/
var sLog_body;
var sLog_line_number;
function sLog_init(){
	sLog_line_number = 0;
	sLog_body=$("slog_list");
}
function sLog_clear(){
	sLog_body.update();
	sLog_line_number =  0;
}
function sLog_addRow(_body, state, action_text, state_text){
	sLog_line_number++;
	var mycurrent_row = y_add_row_to_table(_body, ((sLog_line_number % 2) ==0)?"a":"b" );
	var __img ="/images/info.png";
	switch (state)
	{
		case "green":	__img = "/images/accept.png"; break;
		case "yellow":	__img = "/images/alert.gif"; break;
		case "ok":	__img = "/images/info.png"; break;
		case "red":	__img = "/images/remove.png"; break;
	}
	y_add_html_cell_to_row(mycurrent_row, "icon", "<img src='"+__img+"'>");
	y_add_html_cell_to_row(mycurrent_row, "action_text", action_text);
	y_add_text_cell_to_row(mycurrent_row, "state_text", state_text);
}
/*update_list*/
var update_body;
/*<upd_site>,<type=n|m>,<link text>,<link helptext>,<url>,<unique tag>,<version>,<url of installer>*/
/*<type=n|m>,<link text>,<link helptext>,<url>,<unique tag>,<version>*/
/*<type=u>,<update site name>,<update site helptext>,<url of extention list>*/
function list_init(){
	update_body=$("update_list");
}
function list_clear(){
	update_body.update();
}
function update_list_addRow(_body, i, your_version, ext_item)
{
	var mycurrent_row = y_add_row_to_table(_body, ((i % 2) ==0)?"a":"b" );

	var check='<input type="checkbox" name="setupdate" id="ch'+i+'" ysize="'+ext_item.get('size')+'" onchange="changeitem(ch'+i+')">';
	var info="&nbsp;";
	var etype="";

	switch (ext.upd_version){
		case "1":
			yweb_version = "%";
			break;
		case "2":
			var yweb_version=ext_item.get('yweb_version');
			if (!yweb.require(yweb_version)) 
				check = "";
			if (typeof(ext_item.get('info_url')) != 'undefined')
				info="<a href=\""+ext_item.get('info_url')+"\" target=\"_blank\"><img src=\"/images/info.png\"/></a>";
			break;
	}
	switch(ext_item.get('type')){
		case "m": etype="<img src=\"/images/ext_mgr.png\" alt=\"m\" title=\"Extension: Management\"/>";break;
		case "n": etype="<img src=\"/images/ext_normal.png\" alt=\"n\" title=\"Extension: Normal\"/>";break;
		case "s": etype="<img src=\"/images/ext_script.png\" alt=\"s\" title=\"Script\"/>";break;
		case "p": etype="<img src=\"/images/ext_plugin.png\" alt=\"p\" title=\"Plugin\"/>";break;
		case "x": etype="<img src=\"/images/ext_menu.png\" alt=\"x\" title=\"Menu Extension\"/>";break;
		case "o": etype="<img src=\"/images/ext_ex.png\" alt=\"o\" title=\"One Time Run\"/>";break;
		
	}
	var upd_version = ext_item.get('version');
	if(version_str_less(your_version,upd_version))
		upd_version="<b>"+upd_version+"</b>";
	y_add_html_cell_to_row(mycurrent_row, "setupdate", check);
	y_add_text_cell_to_row(mycurrent_row, "site", ext_item.get('site'));
	y_add_html_cell_to_row(mycurrent_row, "type", etype);
	y_add_text_cell_to_row(mycurrent_row, "menu", ext_item.get('menuitem'));
	y_add_text_cell_to_row(mycurrent_row, "tag", ext_item.get('tag'));
	y_add_text_cell_to_row(mycurrent_row, "your_version", your_version);
	y_add_html_cell_to_row(mycurrent_row, "update_version", upd_version);
	y_add_html_cell_to_row(mycurrent_row, "yweb_version", yweb_version);
	y_add_html_cell_to_row(mycurrent_row, "size", ext_item.get('size'));
	y_add_html_cell_to_row(mycurrent_row, "info", info);
}
function changeitem(e){
	var avail=0;
	try{
		var a=$('avaiable').innerHTML;
		avail=parseInt(a,10);
		var siz=parseInt($(e).readAttribute('ysize'),10);
		if($(e).checked)
			avail-=siz;
		else
			avail+=siz;
		$('avaiable').update(avail);
	}
	catch(e){};
}
function build_list(){
	$('statusline').show();
	window.setTimeout("build_list2()",300);
}
function build_list2()
{
	show_free();
	sLog_init();
	list_init();
	list_clear();

	ext.read_items();
	sLog_addRow(sLog_body, "green", "installed Extensions: "+ext.installed_extensions.length, "ok");
	ext.on_get_updates=function(site,isError){
		if(isError)
			sLog_addRow(sLog_body, "red", "cannot get list from: "+site, "error");
		else
			sLog_addRow(sLog_body, "green", "get list from: "+site, "ok");
	};
	ext.get_updates();
	/*build_list*/
	i=0;
	ext.upd_extensions.each(function(e){
		var your_version="%";
		var have=ext.installed_extensions.find(function(ex){
			try { return ex.get('tag')==e.get('tag');}
			catch(e){return false;} 
			});
		if(have)
			your_version = have.get('version');
		update_list_addRow(update_body,++i,your_version,e);
	});
	$('statusline').hide();
}
function do_set_updates(){
	show_waitbox(true);
	window.setTimeout("do_set_updates2()",300);
}
function do_set_updates2(){
	var _rows = update_body.getElementsByTagName("tr");
	for(var i=0; i< _rows.length; i++){
		var rowNode = _rows.item(i);
		if(rowNode.firstChild.firstChild && rowNode.firstChild.firstChild.checked == true){
			var res=ext.install(ext.upd_extensions[i]);
			res=res.gsub("\n","<br/>");
			if(res.search(/error/)!=-1)
				sLog_addRow(sLog_body, "red", "update error: "+res, "error");
			else
				sLog_addRow(sLog_body, "green", "installed/updates: "+ext.upd_extensions[i].get('tag')+" "+ext.upd_extensions[i].get('version')+"<br>"+res, "ok");
		}
	}
	sLog_addRow(sLog_body, "green", "updating local extension list. Starting ...", "ok");
	var extfile=ext.build_extension_file();
	document.f.extentions.value=extfile;
	show_waitbox(false);
	alert("Update finished. Menue reload.");
	do_submit();
}
var avaiable=0;
function show_free(){
	var res=dbox_exec_tools("mtd_space");
	var Ausdruck = /([^ ]*)[ ]*([^ ]*)[ ]*([^ ]*)[ ]*([^ ]*)[ ]*([^ ]*)[ ]*([^ ]*).*$/;
	Ausdruck.exec(res);
	var mtd = RegExp.$1;
	var total = RegExp.$2;
	var used = RegExp.$3;
	avaiable = RegExp.$4;
	var percentage = RegExp.$5;
	var mtpt = RegExp.$6;
	if (total != "") {
		str = "Space in " + mtd + " (mounted on " + mtpt + ") Total: " + total + "kB; Used: " + used + "kB; Free: " + avaiable + "kB (" + percentage + ")";
		$('avaiable').update(avaiable);
	}
	else 
		str = "Can not determine free space.";
	$("free").update(str);
}
/*uninstall*/
function uninstall_list_addRow(_body, i, ext_item, has_uninstall)
{
	var mycurrent_row = y_add_row_to_table(_body, ((i % 2) ==0)?"a":"b" );

//	var check='<input type="checkbox" name="setupdate" id="ch'+i+'" ysize="'+ext_item.get('size')+'" onchange="changeitem(ch'+i+')">';
	var etype="";
	var info="&nbsp;";
	if (typeof(ext_item.get('info_url')) != 'undefined')
		info="<a href=\""+ext_item.get('info_url')+"\" target=\"_blank\"><img src=\"/images/info.png\"/></a>";
	var uninst="<a href=\'javascript:do_uninstall(\""+ext_item.get('tag')+"\")\' title=\"uninstall\"><img src=\"/images/cross.png\"/></a>";
	if(!has_uninstall) uninst="&nbsp;";
	switch(ext_item.get('type')){
		case "m": etype="<img src=\"/images/ext_mgr.png\" alt=\"m\" title=\"Extension: Management\"/>";break;
		case "n": etype="<img src=\"/images/ext_normal.png\" alt=\"n\" title=\"Extension: Normal\"/>";break;
		case "s": etype="<img src=\"/images/ext_script.png\" alt=\"s\" title=\"Script\"/>";break;
		case "p": etype="<img src=\"/images/ext_plugin.png\" alt=\"p\" title=\"Plugin\"/>";break;
		case "x": etype="<img src=\"/images/ext_menu.png\" alt=\"x\" title=\"Menu Extension\"/>";break;
		case "o": etype="<img src=\"/images/ext_ex.png\" alt=\"o\" title=\"One Time Run\"/>";break;
	}	
	y_add_html_cell_to_row(mycurrent_row, "type", etype);
	y_add_text_cell_to_row(mycurrent_row, "menu", ext_item.get('menuitem'));
	y_add_text_cell_to_row(mycurrent_row, "tag", ext_item.get('tag'));
	y_add_text_cell_to_row(mycurrent_row, "version", ext_item.get('version'));
	y_add_html_cell_to_row(mycurrent_row, "size", ext_item.get('size'));
	y_add_html_cell_to_row(mycurrent_row, "uninstall", uninst);
	y_add_html_cell_to_row(mycurrent_row, "info", info);
}
function do_uninstall(tag){
	if (confirm("Delete " + tag)) {
		show_waitbox(true);
		window.setTimeout("do_uninstall2(\"" + tag + "\")", 300);
	}
}
function do_uninstall2(tag){
	var res=ext.uninstall(tag);
	res=res.gsub("\n","<br/>");
	if(res.search(/error/)!=-1)
		sLog_addRow(sLog_body, "red", "uninstall error: "+res, "error");
	else
		sLog_addRow(sLog_body, "green", "uninstalled: "+tag+"<br/>"+res, "ok");
	sLog_addRow(sLog_body, "green", "updating local extension list. Starting ...", "ok");
	var extfile=ext.build_extension_file();
	document.f.extentions.value=extfile;
	show_waitbox(false);
	alert("Update finished. Menue reload.");
	do_submit();
}
function uninstall_build_list(){
	$('statusline').show();
	window.setTimeout("uninstall_build_list2()",300);
}
function uninstall_build_list2(){
	show_free();
	sLog_init();
	list_init();
	list_clear();
	sLog_addRow(sLog_body, "green", "installed Extensions: "+ext.installed_extensions.length, "ok");

	/*build_list*/
	i=0;
	ext.installed_extensions.sortBy(function(e){return e.get('tag');}).each(function(e){
		res=loadSyncURL("/y/cgi?execute=if-file-exists:/var/tuxbox/config/ext/"+e.get('tag')+"_uninstall.inc~1~0");
		uninstall_list_addRow(update_body,++i,e,res=="1");
	});
	$('statusline').hide();
}

