// The script of the LCD4Linux page (extension lcd4linux): the picture
// LCD4Linux draws of the front display, taken again every few seconds.

// seconds between two pictures: the interface's screenshot timeout where it
// names one of at least a second, else 15
var lcdEvery = 15;

function lcdShown(on) {
	document.getElementById('lcd-picture').hidden = !on;
	document.getElementById('lcd-off').hidden = on;
}

/* Asks the box whether LCD4Linux draws, and only then for its picture: a
   box without it answers the picture with a refusal, which an image would
   show as a broken one. A fresh address each time, so the browser does not
   answer from its cache. */
async function lcdShow() {
	var on = false;
	try {
		var d = await webuiV1('GET', '/osd/displays');
		on = ((d && d.items) || []).some(function(x) { return x.name === 'lcd4linux'; });
	} catch (e) { /* the next round asks again */ }
	if (on)
		document.getElementById('lcd-picture').src = '/api/v1/osd/displays/lcd4linux/screenshot?at=' + Date.now();
	else
		lcdShown(false);
}

document.getElementById('lcd-picture').addEventListener('load', function() { lcdShown(true); });
document.getElementById('lcd-picture').addEventListener('error', function() { lcdShown(false); });

(async function() {
	await webuiExtTexts('lcd4linux');
	try {
		var r = await webuiV1('GET', '/webui/settings');
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
