// The script of the m3u converter's page (extension m3u-converter).

// .webui-file companion: show a count instead of names for multi-select
function m3uFiles(sel) {
	var files = sel.files;
	if (files.length > 1) {
		sel.closest('.webui-file').querySelector('.webui-file-name').textContent =
			files.length + ' ' + webuiL('m3u.files', 'files');
	} else {
		webuiFileName(sel);
	}
	var inp = document.getElementById('m3uInput');
	var pending = files.length;
	for (var i = 0; i < files.length; i++) {
		var r = new FileReader();
		r.onload = function(ev) {
			inp.value += (inp.value ? '\n' : '') + ev.target.result;
			if (--pending === 0) webuiShowToast(files.length + ' ' + webuiL('m3u.loaded', 'file(s) loaded'));
		};
		r.readAsText(files[i]);
	}
}

// port of the Y_Tools_m3uConvert logic: #EXTINF provides the name,
// the following line the URL; udp://@ becomes udp/, & is XML-escaped
function m3uConvert() {
	var lines = document.getElementById('m3uInput').value.split('\n');
	var title = document.getElementById('m3uTitle').value;
	var desc = document.getElementById('m3uDesc').value;
	var genre = document.getElementById('m3uGenre').value;
	var s = '<?xml version="1.0"?>\n<webtvs name="[' + title + '] ">\n';
	var name = '';
	var count = 0;
	for (var i = 0; i < lines.length; i++) {
		var v = lines[i].trim();
		if (v === '') continue;
		if (v.indexOf('#') !== -1) {
			if (v.toUpperCase().indexOf('#EXTINF') !== -1) {
				var ci = v.indexOf(',');
				if (ci !== -1) name = v.substr(ci + 1);
			}
			continue;
		}
		var url = (v.toUpperCase().indexOf('UDP:') !== -1) ? v.replace('udp://@', 'udp/') : v;
		name = name.replace(/#EXTINF:|[0-9],|-[1-9],|,/g, '').replace(/^\s+/g, '');
		if (name === '') name = webuiL('m3u.channel', 'Channel') + ' ' + i;
		s += '    <webtv title="' + name + '" url="' + url +
			'" epgid="" genre="' + genre + '" description="' + desc + '" />\n';
		name = '';
		count++;
	}
	s += '</webtvs>';
	s = s.split('&').join('&amp;');
	document.getElementById('m3uOutput').value = s;
	webuiShowToast(count + ' ' + webuiL('m3u.converted', 'streams converted'));
}

function m3uDownload() {
	var out = document.getElementById('m3uOutput').value;
	if (!out) { webuiShowToast(webuiL('m3u.convert_first', 'Convert first'), true); return; }
	var name = document.getElementById('m3uName').value || 'webtv_usr.xml';
	var a = document.createElement('a');
	a.href = URL.createObjectURL(new Blob([out], {type: 'text/xml'}));
	a.download = name;
	a.click();
	URL.revokeObjectURL(a.href);
}

async function m3uSaveToBox(btn) {
	var out = document.getElementById('m3uOutput').value;
	if (!out) { webuiShowToast(webuiL('m3u.convert_first', 'Convert first'), true); return; }
	var name = document.getElementById('m3uName').value || 'webtv_usr.xml';
	btn.disabled = true;
	try {
		var paths = await webuiApi('paths');
		var target = (paths.configdir || '/var/tuxbox/config');
		var fd = new FormData();
		fd.append('file', new Blob([out], {type: 'text/xml'}), name);
		var up = await fetch('/api/v4/file?action=upload&path=' + encodeURIComponent(target), {method: 'POST', body: fd});
		if (!up.ok) throw new Error(webuiL('m3u.upfail', 'Upload failed'));
		webuiShowToast(webuiL('m3u.saved', 'Saved') + ': ' + target + '/' + name + ' — ' + webuiL('m3u.add_source', 'add it as a source!'));
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

webuiExtTexts('m3u-converter').then(function() {
	// prefill only while untouched (English default from the markup)
	var ti = document.getElementById('m3uTitle');
	if (ti.value === 'my streams') ti.value = webuiL('m3u.title_default', 'my streams');
});
