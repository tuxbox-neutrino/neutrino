// The script of the screenshot's page (extension screenshot).

let reloadInterval = null;
let shotUrl = null;		// object URL of the picture shown, freed when replaced
let shotBusy = false;		// one capture at a time: the box refuses a second one
let displayProbe = null;	// 'shown', 'none', 'again' after a failure, or 'signin' while a sign-in is missing
let displayTries = 0;		// probes that failed for a moment; three, and it is none

// Inject shared remote template; registering via the host keeps
// this instance replaceable by a late-loaded skin renderer
webuiExtTexts('screenshot').then(function() {
	webuiRenderRemote('remoteContainer', {
		onKey: 'rcsim',
		id: 'screenshotRemote'
	});

	// Mobile accordion (shared)
	webuiInitMobileAccordion({
		cardOrder: ['card-remote', 'card-screenshot'],
		storageKey: 'webui-screenshot-card',
		defaultCard: 'card-screenshot',
		breakpoint: 768,
		containerSelector: '.webui-screenshot-grid',
		mode: 'expanded'
	});
});

// Remote Control Simulation (key press + optional auto-screenshot)
async function rcsim(key) {
	if (!checkEnable()) return;
	try {
		await webuiSendKey(key);
		if (document.getElementById('checkReload').checked) {
			setTimeout(doSnapshot, 750);
		}
	} catch (e) {
		webuiToastError(webuiL('msg.error', 'Error'), e);
	}
}

// Screenshot functions
function checkEnable() {
	const osd = document.getElementById('checkOSD').checked;
	const video = document.getElementById('checkVideo').checked;

	if (!osd && !video) {
		webuiShowToast(webuiL('shot.select', 'Please select OSD or video!'), true);
		return false;
	}
	return true;
}

// The picture comes straight from GET /api/v1/osd/screenshot: nothing
// is written to the box, so there is nothing to clean up after
function showPicture(blob) {
	const img = document.getElementById('screenshot');
	if (shotUrl) URL.revokeObjectURL(shotUrl);
	shotUrl = URL.createObjectURL(blob);
	img.src = shotUrl;
	img.style.display = 'block';
	document.getElementById('placeholder').style.display = 'none';
}

// Which capture this is, as part of the address, so that two captures are
// two addresses (the fetch skips the browser's cache as well). The box
// reads the number as a whole number of its own width, four bytes on most
// boxes, so the milliseconds since 1970 do not fit there. These fit, and
// they repeat only every eleven and a half days.
function captureMark() {
	return Date.now() % 1000000000;
}

async function capture(path, query) {
	if (shotBusy) return false;	// the running one answers
	shotBusy = true;
	try {
		showPicture(await webuiV1Blob(path, Object.assign({at: captureMark()}, query || {})));
	} finally {
		shotBusy = false;
	}
	// the probe at load went unanswered, for want of a sign-in, which this
	// capture has just had, or for a moment
	if (displayProbe === 'signin' || displayProbe === 'again') probeDisplay();
	return true;
}

async function doSnapshot() {
	if (!checkEnable()) return;

	// Mobile: show screenshot card
	selectCard('card-screenshot');

	const placeholder = document.getElementById('placeholder');
	const loading = document.getElementById('loading');
	const img = document.getElementById('screenshot');
	const had = img.style.display === 'block';

	if (!had) placeholder.style.display = 'none';
	loading.style.display = had ? 'none' : 'flex';

	try {
		await capture('/osd/screenshot', {
			osd: document.getElementById('checkOSD').checked,
			video: document.getElementById('checkVideo').checked,
			format: 'png'
		});
	} catch (e) {
		// another capture still running keeps the picture there is
		if (e.status !== 409)
			webuiToastError(webuiL('msg.error', 'Error'), e);
		if (!had) placeholder.style.display = 'flex';
	}
	loading.style.display = 'none';
}

// The front display only where the box can read it: 501 says it cannot,
// and any other failure is asked about again with the next capture
async function probeDisplay() {
	try {
		await webuiV1Blob('/osd/display/screenshot', {at: captureMark()}, {quiet: true});
		document.getElementById('btnGlcd').style.display = '';
		displayProbe = 'shown';
	} catch (e) {
		displayProbe = e.status === 403 ? 'signin' : (e.status === 501 || e.status === 404 || ++displayTries >= 3) ? 'none' : 'again';
	}
}
webuiExtTexts('screenshot').then(probeDisplay);

async function doGlcdSnapshot(btn) {
	btn.disabled = true;
	try {
		await capture('/osd/display/screenshot');
	} catch (e) {
		webuiToastError(webuiL('shot.glcdfail', 'GLCD screenshot failed'), e);
	}
	btn.disabled = false;
}

function doClear() {
	const img = document.getElementById('screenshot');
	img.style.display = 'none';
	img.removeAttribute('src');
	if (shotUrl) URL.revokeObjectURL(shotUrl);
	shotUrl = null;
	document.getElementById('placeholder').style.display = 'flex';
}

// The repeat: every so many seconds, from one to 180 whatever is typed
// (an empty field counts as 15), and a new number counts at once
function shotSeconds() {
	var input = document.getElementById('inputReload');
	var n = parseInt(input.value, 10);
	return isNaN(n) ? 15 : Math.min(180, Math.max(1, n));
}

function shotRepeat() {
	clearInterval(reloadInterval);
	reloadInterval = null;
	if (document.getElementById('checkReload').checked)
		reloadInterval = setInterval(doSnapshot, shotSeconds() * 1000);
}

document.getElementById('checkReload').addEventListener('change', function() {
	document.getElementById('inputReload').disabled = !this.checked;
	shotRepeat();
});
document.getElementById('inputReload').addEventListener('change', function() {
	this.value = shotSeconds();
	shotRepeat();
});

// Initialize, once the texts are in
webuiExtTexts('screenshot').then(function() {
	document.getElementById('screenshot').alt = webuiL('shot.alt', 'What the box shows');
	doSnapshot();
});
