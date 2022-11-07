/*	yWeb v3lib
*/

//	next booth functions are taken from the original yweb
//	to make them available in all of our pages
function goConfirmUrl(_meld, _url){
	if (confirm(_meld)==true) goUrl(_url);
}

function goUrl(_url){
	var res = trim(loadSyncURL(_url));
	switch(res){
		case "1": res="on"; break;
		case "0": res="off"; break;
	}
	$("out").update(res);
}

function goPort(_port) {
	if (_port == "") return;
	var host = self.location.href;
	var pos1 = host.indexOf('//');
	var temp = host.substring(pos1+2,host.length);
	var pos2 = temp.indexOf('/');
	var host = host.substring(0,pos1+2+pos2);
	window.open(host + ":" + _port,"_blank");
}

function Y_v3_Tools(_cmd, _tout){
	$("out").update("");
	show_waitbox(true);
	goUrl("/control/exec?Y_v3_Tools&" + _cmd);
	if (typeof(_tout) == "undefined")
		{
		show_waitbox(false);
		}
	else
		{
		window.setTimeout("document.location.reload()", _tout);
		}
}

// taken from http://www.jjam.de/JavaScript/Datum_Uhrzeit/Wochentag.html
function getWeekDay(dd,mm,yyyy) {
	var month = "312831303130313130313031";
	var days = (yyyy-1)*365 + (dd-1);
	for(var i=0;i<mm-1;i++) days += month.substr(i*2,2)*1;
	if(yyyy>1582 || yyyy==1582 && (mm>10 || mm==10 && dd >4)) days -= 10;
	var leapyears = Math.floor(yyyy / 4);
	if(yyyy%4==0 && mm<3) leapyears--;
	if(yyyy>=1600) {
		leapyears -= Math.floor((yyyy-1600) / 100);
		leapyears += Math.floor((yyyy-1600) / 400);
		if(yyyy%100==0 && mm<3) {
			leapyears++;
			if(yyyy%400==0) leapyears--;
		}
	}
	days += leapyears;
	return "SatSonMonThuWedThuFri".substr(days%7*3,3);
}

function compareVersion(_vi, _vo)
{
	if (_vi == "" || _vo == "") return;

	if (_vi >= _vo)	jQuery(document).ready(function(){jQuery('.upd_no').show().css('color', '#c0c0c0')});
        else		jQuery(document).ready(function(){jQuery('.upd_yes').show().css('color', '#008000')});
}

function getVersion(_version)
{
	if (_version == "") return;
	var Vmajor = _version.substr(1, 1);
	var Vminor = _version.substr(2, 2);

	document.write(Vmajor + "." + Vminor);
}

function getBuild(_version)
{
	if (_version == "") return;

	var month=new Array(13);
		month[0]="";
		month[1]="Jan";
		month[2]="Feb";
		month[3]="Mar";
		month[4]="Apr";
		month[5]="May";
		month[6]="Jun";
		month[7]="Jul";
		month[8]="Aug";
		month[9]="Sep";
		month[10]="Oct";
		month[11]="Nov";
		month[12]="Dec";

	var Byear = _version.substr(4, 4);
	var Bmonth = _version.substr(8, 2).replace( /^(0+)/g, '' );
	var Bday = _version.substr(10, 2);
	var Bhour = _version.substr(12, 2);
	var Bmin = _version.substr(14, 2);

	document.write(getWeekDay(Bday,Bmonth,Byear) + " " + month[Bmonth] + " " + Bday + " " + Bhour + ":" + Bmin + " CEST " + Byear);
}

function get_update_txt()
{
        //show_waitbox(true);
	loadSyncURL("/control/exec?Y_v3_Tools&get_update_txt");
	window.document.location.href="/Y_About.yhtm?ani=false";
}
