/*	yWeb Tools Library
*/

function Y_NI_Tools(_cmd, _tout){
	$("out").update("");
	show_waitbox(true);
	goUrl("/control/exec?Y_NI_Tools&" + _cmd);
	if (typeof(_tout) == "undefined")
		{
		show_waitbox(false);
		}
	else
		{
		window.setTimeout("document.location.reload()", _tout);
		}
}
