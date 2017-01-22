/*	yWeb Flash by yjogol
	$Date$
	$Revision$
*/
function do_image_upload_dialog()
{
	var nr=obj_get_radio_value('R1');
	nr = (nr=="")?0:nr;
	document.f.mtd_text.value = document.f.elements[nr].title;
	document.f.mtd.value = nr;
	document.f.execute.value = "script:Y_Tools image_flash_free_tmp";
	document.f.tmpl.value = "Y_Tools_Flash_Upload.yhtm";
	document.f.submit();
}
function do_image_download()
{
	show_waitbox(true);
	$('f').disable();
	window.setTimeout("do_image_download_2()",100);
	/*setTimeout:refresh output*/
}
function do_image_download_2()
{
	var nr=obj_get_radio_value('R1');
	nr = (nr=="")?0:nr;
	var res = loadSyncURL("/control/exec?Y_Tools&image_backup&"+nr);
	$('download_link').href = res;
	$('f').enable();
	show_waitbox(false);
	$('download_box').show();
}
function goConfirmUrl(_meld, _url)
{
	if (confirm(_meld)==true)
		loadSyncURL(_url);
}
function image_delete()
{
	var res = loadSyncURL("/control/exec?Y_Tools&image_delete");
	$('download_box').hide();
}
/*flashing*/
var aktiv;
var flashing = false;
var flash_diag = null;
function show_progress(_msg,_type,_proz)
{
	var __msg = $("msg");
	__msg.value = _msg;
	var tmsg="";
	if(_type == 1){
		tmsg="erasing";
		$("erasing_left").width = _proz;
	}
	else if(_type == 2){
		tmsg="writing";
		$("erasing_left").width = "100%";
		$("writing_left").width = _proz;;
	}
	else if(_type == 3){
		tmsg="verifying";
		$("writing_left").width = "100%";
		$("verifying_left").width = _proz;;
	}
	loadSyncURL("/control/lcd?lock=1&clear=1&xpos=10&ypos=27&size=20&font=2&text=yWeb%20flashing%0A"+tmsg+"%20"+encodeURI(_proz)+"&update=1");
}
/* load fcp status from /tmp/e.txt*/
function progress_onComplete(_req)
{
	var last = _req.responseText.lastIndexOf("\r");
	var last_line = _req.responseText.slice(last+1, _req.responseText.length);
	var _type = 0;
	if(last_line.search(/Erasing/)!=-1)
		_type = 1;
	if(last_line.search(/Writing/)!=-1)
		_type = 2;
	if(last_line.search(/Verifying/)!=-1)
		_type = 3;
	var Ausdruck = /\((.*)\)/;
	var e=Ausdruck.exec(last_line);
	var p = RegExp.$1;
	show_progress(last_line, _type, p);
}
function progress_get()
{
	var myAjax = new Ajax.Request(
	"/tmp/e.txt",
	{
		method: 'post',
		onComplete: progress_onComplete
	});
}
function do_submit()
{
	var msg = "Flash Image?";
	if(document.f.demo.checked)
		msg = "DEMO: "+msg;
	if(confirm(msg)==true){
		show_waitbox(true);
		document.f.submit();
		$('f').disable();
	}
}

function do_image_upload_ready_error()
{
	$('f').enable();
	show_waitbox(false);
}
function do_image_flash_ready()
{
	window.clearInterval(aktiv);
	$('flash_diag').hide();
	loadSyncURL("/control/lcd?lock=0");
	alert("Image flashed. Press OK after reboot");
	top.location.href="/";
}
