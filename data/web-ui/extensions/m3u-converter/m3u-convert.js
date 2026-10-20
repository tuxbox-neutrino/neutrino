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

/* Saved into the directory the box keeps its own web TV lists in, which is
   where the file routes may write and where the settings' own uploads go
   (GET /api/v1/channels/sources names it), under the name given. */
async function m3uSaveToBox(btn) {
	var out = document.getElementById('m3uOutput').value;
	if (!out) { webuiShowToast(webuiL('m3u.convert_first', 'Convert first'), true); return; }
	var name = (document.getElementById('m3uName').value || 'webtv_usr.xml').replace(/[^A-Za-z0-9._-]/g, '_');
	btn.disabled = true;
	try {
		var src = await webuiV1('GET', '/channels/sources');
		await m3uStore({
			out: out,
			path: String(src.webtv_dir || '').replace(/\/+$/, '') + '/' + name,
			add: document.getElementById('m3uAddSource').checked
		}, false);
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	} finally {
		btn.disabled = false;
	}
}

/* One save as it was asked for: a question about replacing a file answers
   for this file and these bytes, whatever the fields say by then. A file
   that is one of the WebTV sources has the box read its lists again, also
   when it only replaced itself; where asked, one that is not yet becomes
   one, with the same reading. */
async function m3uStore(job, overwrite) {
	try {
		await webuiV1('PUT', '/storage/file', {query: {path: job.path, overwrite: overwrite ? 'true' : null},
			raw: new Blob([job.out], {type: 'text/xml'})});
	} catch (e) {
		if (e.code !== 'name-taken' || overwrite) throw e;
		webuiShowConfirm(webuiL('m3u.replace_title', 'Replace the file?'),
			webuiLH('m3u.replace_text', 'There is a file of this name on the box already:') + ' ' + escapeHtml(job.path),
			webuiL('m3u.replace', 'Replace'),
			function() {
				return m3uStore(job, true).catch(function(e2) {
					webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e2.message, true);
				});
			});
		return;
	}
	var said = webuiL('m3u.saved', 'Saved') + ': ' + job.path;
	// the list as it is now, not as it was before a question: one changed
	// in the settings meanwhile is not written back over
	var now = await webuiV1('GET', '/channels/sources');
	var sources = (now && now.webtv) || [];
	var listed = sources.indexOf(job.path) >= 0;
	if (listed || job.add) {
		await webuiV1('PUT', '/channels/sources/webtv', {body: listed ? sources : sources.concat([job.path]),
			query: {reload: true}});
		said += ' – ' + (listed ? webuiL('m3u.source_known', 'already one of the WebTV sources, the channel lists are read again') :
			webuiL('m3u.source_added', 'added to the WebTV sources, the channel lists are read again'));
	}
	webuiShowToast(said);
}

webuiExtTexts('m3u-converter').then(function() {
	// prefill only while untouched (English default from the markup)
	var ti = document.getElementById('m3uTitle');
	if (ti.value === 'my streams') ti.value = webuiL('m3u.title_default', 'my streams');
});
