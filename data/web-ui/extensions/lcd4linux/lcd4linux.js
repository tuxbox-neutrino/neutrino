// The script of the LCD4Linux page (extension lcd4linux): the picture
// LCD4Linux draws of the front display, taken again every few seconds, on a
// page of the interface or full screen.

// seconds between two pictures: the interface's screenshot timeout where it
// names one of at least a second, else 15
var lcdEvery = 15;

// the picture, or in its place what keeps it away
function lcdShown(on, text) {
	document.getElementById('lcd-picture').hidden = !on;
	var off = document.getElementById('lcd-off');
	off.hidden = on;
	if (!on)
		off.textContent = text || webuiL('lcd.off', 'LCD4Linux is not drawing on this box.');
}

/* What the box answers, read without a sign-in dialog, which this screen
   has no place for; a refusal or no answer throws with its status */
async function lcdRead(path) {
	var r = await fetch(_webuiV1Url(path), {credentials: 'same-origin', cache: 'no-store'});
	if (!r.ok) {
		var e = new Error(path + ': HTTP ' + r.status);
		e.status = r.status;
		throw e;
	}
	return r.json();
}

/* Asks the box whether LCD4Linux draws, and only then for its picture: a
   box without it answers the picture with a refusal, which an image would
   show as a broken one. A fresh address each time, so the browser does not
   answer from its cache. A box that does not answer, or asks for a sign-in,
   says so: that is not a box without LCD4Linux. The next round asks again. */
async function lcdShow() {
	var on = false;
	try {
		var d = await lcdRead('/osd/displays');
		on = ((d && d.items) || []).some(function(x) { return x.name === 'lcd4linux'; });
	} catch (e) {
		lcdShown(false, e.status === 401 || e.status === 403 ? webuiL('err.sign_in', 'Only after signing in') :
			webuiL('lcd.no_answer', 'The box does not answer.'));
		return;
	}
	if (on)
		document.getElementById('lcd-picture').src = '/api/v1/osd/displays/lcd4linux/screenshot?at=' + Date.now();
	else
		lcdShown(false);
}

// The picture alone on the whole screen, and back; a browser without full
// screen keeps its page
function lcdFullscreen(on) {
	var stage = document.getElementById('lcd-stage');
	if (on && stage.requestFullscreen)
		stage.requestFullscreen().catch(function() { /* the page stays */ });
	else if (!on && document.fullscreenElement && document.exitFullscreen)
		document.exitFullscreen();
}

document.getElementById('lcd-picture').addEventListener('load', function() { lcdShown(true); });
document.getElementById('lcd-picture').addEventListener('error', function() {
	lcdShown(false, webuiL('lcd.no_picture', 'The picture of the display could not be read.'));
});

(async function() {
	await webuiExtTexts('lcd4linux');
	if (!document.getElementById('lcd-stage').requestFullscreen)
		document.getElementById('lcd-full').hidden = true;
	document.getElementById('lcd-picture').alt = webuiL('lcd.alt', 'The front display as LCD4Linux draws it');
	try {
		var r = await lcdRead('/webui/settings');
		((r && r.items) || []).forEach(function(it) {
			var n = parseInt(it.value, 10);
			if (it.key === 'screenshot_timeout' && n >= 1)
				lcdEvery = n;
		});
	} catch (e) { /* the default */ }
	lcdShow();
	// only while somebody can see it
	setInterval(function() {
		if (document.visibilityState === 'visible') lcdShow();
	}, lcdEvery * 1000);
})();
