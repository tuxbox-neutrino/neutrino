// The script of the info screen (extension info): what the box plays and
// what runs on it now, read from /api/v1 and drawn again on every event of
// the box that changes it, and every 15 seconds besides, as the page it
// replaces was.

// the channel shown, so its logo is asked for once and not every round
var infoChannel = null;
// counts the reads of the channel, so only the last one asked for is drawn
var infoAsked = 0;
// the same for the flags
var infoFlagsAsked = 0;
// what the event of the channel shown was, so the bar can move between reads
var infoEvent = null;

// a flag lit or not, in its colour and, for who cannot see that, in words
function infoFlag(id, on) {
	var el = document.getElementById(id);
	el.classList.toggle('on', !!on);
	el.querySelector('.vh').textContent = ': ' + (on ? webuiL('info.on', 'on') : webuiL('info.off', 'off'));
}

function infoClock() {
	var t = new Date();
	document.getElementById('info-clock').textContent =
		String(t.getHours()).padStart(2, '0') + ':' + String(t.getMinutes()).padStart(2, '0');
}

// the channel: its logo where the box has one, its name otherwise; without
// one, the text given (standby, or nothing playing)
function infoShowChannel(ch, text) {
	var logo = document.getElementById('info-logo');
	var name = document.getElementById('info-name');
	if (!ch) {
		infoChannel = null;
		logo.hidden = true;
		logo.removeAttribute('src');
		name.hidden = false;
		name.textContent = text;
		return;
	}
	name.textContent = ch.name || '';
	logo.alt = ch.name || '';
	if (ch.id !== infoChannel) {
		infoChannel = ch.id;
		logo.hidden = true;
		name.hidden = false;
		logo.src = '/api/v1/channels/' + encodeURIComponent(ch.id) + '/logo';
	}
}

/* The event: its title, how far it is as a bar, and the minutes done of all,
   counted as LCD4Linux counts them for the page this replaces. */
function infoShowEvent(ev) {
	infoEvent = ev;
	var percent = 0;
	var minutes = '';
	if (ev && ev.duration > 0 && ev.duration < 86400) {
		var now = Date.now() / 1000;
		var total = Math.floor(ev.duration / 60);
		var done = now < ev.start ? 0 : Math.floor((now - ev.start + 30) / 60);
		percent = Math.max(0, Math.min(100, Math.floor(100 * (now - ev.start) / ev.duration)));
		minutes = done + '/' + total;
	}
	document.getElementById('info-event').textContent = (ev && ev.title) || '';
	document.getElementById('info-progress').style.width = percent + '%';
	document.getElementById('info-bar').setAttribute('aria-valuenow', percent);
	document.getElementById('info-duration').textContent = minutes;
}

/* What the box answers, read without a sign-in dialog, which this screen
   has no place for. A refusal or no answer throws, with the status where
   there was one, so that it is not taken for a state of the box. */
async function infoRead(path, query) {
	var r = await fetch(_webuiV1Url(path, query), {credentials: 'same-origin', cache: 'no-store'});
	if (!r.ok) {
		var e = new Error(path + ': HTTP ' + r.status);
		e.status = r.status;
		throw e;
	}
	return r.json();
}

// what the screen says for a read that failed
function infoTrouble(e) {
	return e && (e.status === 401 || e.status === 403) ? webuiL('err.sign_in', 'Only after signing in') :
		webuiL('info.no_answer', 'The box does not answer');
}

// the box answered and has nothing of the kind (no channel playing, no
// programme known) as against a read that did not get through
function infoNothing(e) {
	return !!e && e.status >= 400 && e.status < 500 && e.status !== 401 && e.status !== 403;
}

/* The box in standby still names the channel it played last, so standby is
   asked first, and shown as the page this replaces showed it: the word, no
   event, an empty bar. A read that is no longer the last one asked for is
   dropped, so an older event never lands under a newer channel. */
async function infoChannelAndEvent() {
	var mine = ++infoAsked;
	var standby, ch = null, ev = null;
	try {
		standby = await infoRead('/system/standby');
		if (!standby.on)
			ch = await infoRead('/channels/current').catch(function(e) {
				if (infoNothing(e)) return null;
				throw e;
			});
		if (ch)
			ev = await infoRead('/epg/current', {channel: ch.id}).catch(function() { return null; });
	} catch (e) {
		if (mine !== infoAsked) return;
		infoShowChannel(null, infoTrouble(e));
		infoShowEvent(null);
		return;
	}
	if (mine !== infoAsked) return;
	infoShowChannel(ch, standby.on ? webuiL('info.standby', 'Standby') :
		webuiL('info.none', 'No channel is playing'));
	infoShowEvent(ev);
}

/* The flags at the top: ECM while a softcam answers for the channel (the
   rule LCD4Linux had for it, switched off there, so the page this replaces
   never lit it), and as LCD4Linux sets them CI+ while a module is sent the
   channel, TIMER while a zap or a recording is still to come, REC while the
   box records, TS while it keeps its shift of what it shows. */
async function infoFlags() {
	var mine = ++infoFlagsAsked;
	var got;
	try {
		got = await Promise.all([infoRead('/system/decryption'), infoRead('/timers'), infoRead('/recordings')]);
	} catch (e) {
		return;	// the flags as they were: no answer is no news
	}
	if (mine !== infoFlagsAsked) return;
	var dec = got[0] || {};
	var now = Date.now() / 1000;
	var timers = (got[1] && got[1].items) || [];
	var recs = (got[2] && got[2].items) || [];
	infoFlag('info-ecm', dec.softcam);
	infoFlag('info-cam', dec.ci_module);
	infoFlag('info-timer', timers.some(function(t) {
		return (t.kind === 'zapto' || t.kind === 'record') && t.start > now;
	}));
	infoFlag('info-rec', recs.some(function(r) { return !r.timeshift; }));
	infoFlag('info-ts', recs.some(function(r) { return r.timeshift; }));
}

function infoAll() {
	infoClock();
	return Promise.all([infoChannelAndEvent(), infoFlags()]);
}

/* The last 12 hexadecimal digits name a channel in both of the box's
   spellings; the short one comes without its leading zeros, so both are
   padded first (webuiLegacyChannelId). */
function infoSameChannel(a, b) {
	return !!a && !!b && webuiLegacyChannelId(a).slice(-12).toLowerCase() ===
		webuiLegacyChannelId(b).slice(-12).toLowerCase();
}

document.getElementById('info-logo').addEventListener('load', function() {
	this.hidden = false;
	document.getElementById('info-name').hidden = true;
});
document.getElementById('info-logo').addEventListener('error', function() {
	this.hidden = true;
	document.getElementById('info-name').hidden = false;
});

(async function() {
	await webuiExtTexts('info');
	await infoAll();
	webuiV1Events({
		zap: infoChannelAndEvent,
		standby: infoAll,
		'record-start': infoFlags,
		'record-stop': infoFlags,
		'timer-changed': infoFlags,
		'epg-updated': function(d) {
			if (d && infoSameChannel(d.channel_id, infoChannel)) infoChannelAndEvent();
		}
	});
	// the clock and the bar between two reads, the reads every 15 seconds,
	// both only while somebody can see the page
	setInterval(function() {
		if (document.visibilityState !== 'visible') return;
		infoClock();
		infoShowEvent(infoEvent);
	}, 5000);
	setInterval(function() {
		if (document.visibilityState === 'visible') infoAll();
	}, 15000);
})();
