// The script of the file manager's page (extension files).

// The file manager works through /api/v1/storage: it reaches the
// directories the box names as its roots (where it records, where its
// movie browser looks, its plugin folders and the usual media and
// temporary directories) and nothing else on the box. The top level
// lists those roots. Rows are built with webuiEl, so a file name is
// always text, whatever characters it carries.

// State: '' is the top level, the list of roots
let currentPath = '';
let roots = [];		// the roots that are there, as GET /storage/roots names them

// SVG icon templates (page source, not data)
const ICONS = {
	folder: '<svg class="webui-file-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z"/></svg>',
	file: '<svg class="webui-file-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/></svg>',
	link: '<svg class="webui-file-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10 13a5 5 0 007.54.54l3-3a5 5 0 00-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 00-7.54-.54l-3 3a5 5 0 007.07 7.07l1.71-1.71"/></svg>',
	parent: '<svg class="webui-file-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="15,18 9,12 15,6"/></svg>',
	download: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4"/><polyline points="7,10 12,15 17,10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>',
	rename: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M17 3a2.83 2.83 0 114 4L7.5 20.5 2 22l1.5-5.5L17 3z"/></svg>',
	delete: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3,6 5,6 21,6"/><path d="M19 6v14a2 2 0 01-2 2H7a2 2 0 01-2-2V6m3 0V4a2 2 0 012-2h4a2 2 0 012 2v2"/></svg>',
	ok: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:12px;height:12px"><polyline points="20,6 9,17 4,12"/></svg>',
	cancel: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:12px;height:12px"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>'
};

// Format file size
function formatFileSize(bytes) {
	if (bytes === 0) return '0 B';
	const units = ['B', 'KB', 'MB', 'GB', 'TB'];
	const k = 1024;
	const i = Math.floor(Math.log(bytes) / Math.log(k));
	return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + units[i];
}

// Format timestamp to readable date
function formatFileDate(timestamp) {
	if (!timestamp) return '';
	const d = new Date(timestamp * 1000);
	const pad = n => n < 10 ? '0' + n : n;
	return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()) + ' ' + pad(d.getHours()) + ':' + pad(d.getMinutes());
}

// Normalize path (remove double slashes, trailing slash)
function normPath(p) {
	p = p.replace(/\/+/g, '/');
	if (p.length > 1 && p.endsWith('/')) p = p.slice(0, -1);
	return p || '/';
}

function joinPath(dir, name) {
	return normPath(dir + '/' + name);
}

// The root a path lies in: the longest one it is or lies below
function rootOf(path) {
	let best = '';
	roots.forEach(r => {
		if ((path === r || path.startsWith(r + '/')) && r.length > best.length) best = r;
	});
	return best;
}

function storageQuery(path) {
	return '?path=' + encodeURIComponent(path);
}

// A yes or no that shows its text as text (file names among it)
function askFirst(title, text, ok) {
	return new Promise(resolve => {
		webuiSheet({
			title: title,
			body: webuiEl('p', {}, text),
			actions: [
				{label: webuiL('msg.cancel', 'Cancel'), value: false},
				{label: ok, value: true, danger: true}
			],
			onClose: result => resolve(result === true)
		});
	});
}

// ===== Quick Access Sidebar: the roots =====

// why the roots could not be read, for the top level to say instead of
// showing nothing ('' once they were read)
let rootsWhy = '';

async function loadQuickDirs() {
	try {
		const r = await webuiV1('GET', '/storage/roots');
		roots = ((r && r.items) || []).filter(x => x.present).map(x => normPath(x.id));
		rootsWhy = '';
	} catch (e) {
		roots = [];
		rootsWhy = webuiErrText(e);
		webuiToastError(webuiL('msg.load_fail', 'Loading failed'), e);
	}
	renderQuickDirs();
}

function renderQuickDirs() {
	const list = document.getElementById('quickList');
	const items = [{name: webuiL('fm.roots', 'Permitted folders'), path: ''}].concat(roots.map(r => ({name: r, path: r})));
	list.replaceChildren(...items.map(d => webuiEl('li', {
		className: 'webui-nav-item webui-quick-item' + (d.path === currentPath ? ' active' : ''),
		on: {click: () => navigateTo(d.path)}
	}, webuiMarkup('<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z"/></svg>'),
		webuiEl('span', {className: 'webui-nav-name webui-quick-name'}, d.name))));
}

// ===== Breadcrumb: the roots, the root, then what lies below it =====

function renderBreadcrumb() {
	const el = document.getElementById('breadcrumb');
	const crumbs = [{name: webuiL('fm.roots', 'Permitted folders'), path: ''}];
	const root = rootOf(currentPath);
	if (root) {
		crumbs.push({name: root, path: root});
		let acc = root;
		currentPath.substring(root.length).split('/').filter(Boolean).forEach(part => {
			acc += '/' + part;
			crumbs.push({name: part, path: acc});
		});
	}
	const parts = [];
	crumbs.forEach((c, i) => {
		if (i > 0) parts.push(webuiEl('span', {className: 'webui-breadcrumb-sep'}, '/'));
		if (i === crumbs.length - 1)
			parts.push(webuiEl('span', {className: 'webui-breadcrumb-current'}, c.name));
		else
			parts.push(webuiEl('a', {attrs: {href: '#'}, on: {click: ev => { ev.preventDefault(); navigateTo(c.path); }}}, c.name));
	});
	el.replaceChildren(...parts);
}

// ===== File Listing =====

function fileRow(entry) {
	const isDir = entry.kind === 'dir';
	const isLink = entry.kind === 'link';
	const fullpath = joinPath(currentPath, entry.id);
	const actions = webuiEl('div', {className: 'webui-file-actions', on: {click: ev => ev.stopPropagation()}});
	if (!isDir)
		actions.appendChild(webuiEl('a', {className: 'webui-btn', title: webuiL('fm.download', 'Download'),
			attrs: {href: '/api/v1/storage/file' + storageQuery(fullpath), download: entry.id}}, webuiMarkup(ICONS.download)));
	actions.appendChild(webuiEl('button', {type: 'button', className: 'webui-btn', title: webuiL('fm.rename', 'Rename'),
		on: {click: () => startRename(fullpath, entry.id)}}, webuiMarkup(ICONS.rename)));
	actions.appendChild(webuiEl('button', {type: 'button', className: 'webui-btn', title: webuiL('fm.delete', 'Delete'),
		style: {color: 'var(--webui-danger)'}, on: {click: () => deleteItem(fullpath, entry.id, isDir)}}, webuiMarkup(ICONS.delete)));

	const size = isDir || !entry.attributes_read ? '' : formatFileSize(entry.size || 0);
	const date = entry.attributes_read ? formatFileDate(entry.mtime || 0) : '';
	return webuiEl('li', {className: 'webui-file-row' + (isDir ? ' is-dir' : ''), data: {path: fullpath, name: entry.id},
		on: isDir ? {click: () => navigateTo(fullpath)} : {}},
		webuiMarkup(isDir ? ICONS.folder : (isLink ? ICONS.link : ICONS.file)),
		webuiEl('div', {className: 'webui-file-info'},
			webuiEl('span', {className: 'webui-file-name'}, entry.id),
			webuiEl('span', {className: 'webui-file-size'}, size),
			webuiEl('span', {className: 'webui-file-date'}, date)),
		actions);
}

function rootRow(root) {
	return webuiEl('li', {className: 'webui-file-row is-dir', data: {path: root}, on: {click: () => navigateTo(root)}},
		webuiMarkup(ICONS.folder),
		webuiEl('div', {className: 'webui-file-info'}, webuiEl('span', {className: 'webui-file-name'}, root)));
}

function note(text, color) {
	return webuiEl('li', {style: {padding: '1rem', 'text-align': 'center', color: color || 'var(--pico-muted-color)'}}, text);
}

let listToken = 0;	// a slower answer for a directory left already is dropped

async function loadDir(path) {
	currentPath = path ? normPath(path) : '';
	const mine = ++listToken;
	renderBreadcrumb();
	renderQuickDirs();
	updatePreviews();

	const list = document.getElementById('fileList');
	const title = document.getElementById('fileListTitle');

	// The top level: the roots, and a word on why there is nothing else
	if (!currentPath) {
		title.textContent = webuiL('fm.roots', 'Permitted folders');
		document.getElementById('diskInfo').textContent = '';
		list.replaceChildren(...roots.map(rootRow), rootsWhy
			? note(webuiL('msg.load_fail', 'Loading failed') + ': ' + rootsWhy, 'var(--webui-danger)')
			: note(webuiL('fm.roots_note', 'Only these folders and what lies below them can be reached from here.')));
		return;
	}

	title.textContent = currentPath.split('/').filter(Boolean).pop() || currentPath;
	list.replaceChildren(webuiEl('li', {attrs: {'aria-busy': 'true'}, style: {padding: '1rem', 'text-align': 'center'}}, webuiL('msg.loading', 'Loading...')));

	try {
		const data = await webuiV1('GET', '/storage/files', {query: {path: currentPath}});
		if (mine !== listToken) return;
		const files = (data && data.items) || [];

		// Sort: directories first, then alphabetical
		files.sort((a, b) => {
			const aDir = a.kind === 'dir';
			const bDir = b.kind === 'dir';
			if (aDir && !bDir) return -1;
			if (!aDir && bDir) return 1;
			return (a.id || '').localeCompare(b.id || '');
		});

		// Up: from a root to the list of roots, else one directory up
		const parentPath = rootOf(currentPath) === currentPath ? '' :
			(currentPath.substring(0, currentPath.lastIndexOf('/')) || '/');
		const rows = [webuiEl('li', {className: 'webui-file-row parent-dir is-dir', on: {click: () => navigateTo(parentPath)}},
			webuiMarkup(ICONS.parent),
			webuiEl('div', {className: 'webui-file-info'}, webuiEl('span', {className: 'webui-file-name'}, '..')))];
		files.forEach(f => rows.push(fileRow(f)));
		if (!files.length) rows.push(note(webuiL('fm.empty', 'Directory is empty')));
		list.replaceChildren(...rows);

		loadDiskInfo(mine);
	} catch (e) {
		if (mine !== listToken) return;
		list.replaceChildren(note(webuiL('msg.load_fail', 'Loading failed') + ': ' + webuiErrText(e), 'var(--webui-danger)'));
	}
}

function navigateTo(path) {
	loadDir(path);
	// Mobile: switch to file list card
	if (window.innerWidth <= 800) {
		selectCard('card-files');
	}
}

function refreshDir() {
	return loadDir(currentPath);
}

// ===== Disk Info: the mount the directory lies on =====

async function loadDiskInfo(mine) {
	const el = document.getElementById('diskInfo');
	try {
		const data = await webuiV1('GET', '/storage/mounts');
		if (mine !== listToken) return;
		let best = null;
		((data && data.items) || []).forEach(m => {
			const at = m.id === '/' ? '' : m.id;
			if ((currentPath === m.id || currentPath.startsWith(at + '/')) && (!best || m.id.length > best.id.length))
				best = m;
		});
		if (best && best.total > 0) {
			const pct = Math.round((best.total - best.free) * 100 / best.total) + '%';
			el.textContent = formatFileSize(best.free) + ' ' + webuiL('fm.free', 'free') + ' / ' + formatFileSize(best.total) + ' (' + pct + ')';
		} else {
			el.textContent = '';
		}
	} catch (e) {
		el.textContent = '';
	}
}

// ===== File Actions =====

// Delete: a file, a link or an empty directory; the box keeps a
// directory that still holds something and says so
async function deleteItem(fullpath, name, isDir) {
	const ok = await askFirst(webuiL('fm.delete', 'Delete'),
		(isDir ? webuiL('fm.folder', 'Folder') : webuiL('fm.file', 'File')) + ' "' + name + '" ' + webuiL('fm.delete_confirm', 'really delete?'),
		webuiL('fm.delete', 'Delete'));
	if (!ok) return;
	try {
		await webuiV1('DELETE', '/storage/path', {query: {path: fullpath}});
		webuiShowToast(webuiL('fm.deleted', 'Deleted'));
		refreshDir();
	} catch (e) {
		webuiToastError(webuiL('fm.delete_fail', 'Delete failed'), e);
	}
}

// Rename in place: the name field takes the row's name
function startRename(fullpath, name) {
	const row = Array.prototype.find.call(document.querySelectorAll('.webui-file-row'), r => r.dataset.path === fullpath);
	if (!row) return;
	const nameEl = row.querySelector('.webui-file-name');
	const input = webuiEl('input', {type: 'text', value: name});
	const go = () => doRename(fullpath, name, input.value.trim());
	nameEl.classList.add('editing');
	nameEl.replaceChildren(input,
		webuiEl('button', {type: 'button', className: 'webui-btn primary', style: {padding: '0.15rem 0.3rem'},
			attrs: {'aria-label': webuiL('fm.rename', 'Rename')}, on: {click: go}}, webuiMarkup(ICONS.ok)),
		webuiEl('button', {type: 'button', className: 'webui-btn', style: {padding: '0.15rem 0.3rem'},
			attrs: {'aria-label': webuiL('msg.cancel', 'Cancel')}, on: {click: () => refreshDir()}}, webuiMarkup(ICONS.cancel)));
	input.focus();
	// Select filename without extension
	const dotIdx = name.lastIndexOf('.');
	if (dotIdx > 0) input.setSelectionRange(0, dotIdx);
	else input.select();
	input.addEventListener('keydown', e => {
		if (e.key === 'Enter') go();
		if (e.key === 'Escape') refreshDir();
	});
}

async function doRename(fullpath, origName, newName) {
	if (!newName || newName === origName) {
		refreshDir();
		return;
	}
	if (newName.indexOf('/') >= 0) {
		webuiShowToast(webuiL('fm.rename_fail', 'Rename failed') + ': ' + webuiL('fm.bad_name', 'a name must not contain /'), true);
		return;
	}
	try {
		const dir = fullpath.substring(0, fullpath.lastIndexOf('/')) || '/';
		await webuiV1('POST', '/storage/rename', {body: {from: fullpath, to: joinPath(dir, newName)}});
		webuiShowToast(webuiL('fm.renamed', 'Renamed'));
		refreshDir();
	} catch (e) {
		webuiToastError(webuiL('fm.rename_fail', 'Rename failed'), e);
	}
}

// ===== Create Folder =====

function showNewFolderInput() {
	if (!currentPath) return;	// the top level holds the roots only
	const row = document.getElementById('newFolderRow');
	row.classList.add('active');
	const input = document.getElementById('newFolderName');
	input.value = '';
	input.focus();
}

function hideNewFolderInput() {
	document.getElementById('newFolderRow').classList.remove('active');
}

function handleNewFolderKey(e) {
	if (e.key === 'Enter') createFolder();
	if (e.key === 'Escape') hideNewFolderInput();
}

async function createFolder() {
	const input = document.getElementById('newFolderName');
	const name = input.value.trim();
	if (!name || !currentPath) return;
	if (name.indexOf('/') >= 0) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + webuiL('fm.bad_name', 'a name must not contain /'), true);
		return;
	}
	try {
		await webuiV1('POST', '/storage/directory', {body: {path: joinPath(currentPath, name)}});
		hideNewFolderInput();
		webuiShowToast(webuiL('fm.folder_created', 'Folder created'));
		refreshDir();
	} catch (e) {
		webuiToastError(webuiL('msg.error', 'Error'), e);
	}
}

// ===== Upload =====

function toggleUpload() {
	if (!currentPath) return;
	const area = document.getElementById('uploadArea');
	area.classList.toggle('active');
}

// The length the piece file has reached, for a piece whose answer was lost
/* In pieces below the server's body ceiling, to a hidden name of its own
   beside the target, which takes the real name at the end: what was there
   under that name stays whole until the last piece is in, and a name in use
   is only replaced after asking. An upload that fails takes its pieces away
   again. */
async function uploadFile(file) {
	if (!file || !currentPath) return;
	const dir = currentPath;
	const target = joinPath(dir, file.name);
	const part = joinPath(dir, '.' + file.name + '.' + Math.random().toString(36).slice(2, 8) + '.upload');

	const progress = document.getElementById('uploadProgress');
	const bar = document.getElementById('uploadBar');
	const status = document.getElementById('uploadStatus');
	progress.classList.add('active');
	status.textContent = webuiL('fm.uploading', 'Uploading') + ': ' + file.name;
	bar.value = 0;

	// the pieces take themselves away when they fail; after that the
	// piece file is this upload's to take away
	let uploaded = false;
	try {
		await webuiV1UploadPieces(file, part, (sent, total) => {
			bar.value = total ? Math.round(sent * 100 / total) : 100;
		});
		uploaded = true;

		try {
			await webuiV1('POST', '/storage/rename', {body: {from: part, to: target}});
		} catch (e) {
			if (e.code !== 'name-taken') throw e;
			const replace = await askFirst(webuiL('fm.upload', 'Upload'), '"' + file.name + '" ' + webuiL('fm.replace_ask', 'already exists here. Replace it?'), webuiL('fm.replace', 'Replace'));
			if (!replace) throw new Error(webuiL('fm.not_replaced', 'the existing file was kept'));
			await webuiV1('POST', '/storage/rename', {body: {from: part, to: target, overwrite: true}});
		}
		uploaded = false;

		status.textContent = webuiL('fm.done', 'Done') + ': ' + file.name;
		webuiShowToast(webuiL('fm.uploaded', 'Uploaded') + ': ' + file.name);
		setTimeout(() => {
			progress.classList.remove('active');
			document.getElementById('uploadArea').classList.remove('active');
		}, 2000);
		if (currentPath === dir) refreshDir();
	} catch (e) {
		status.textContent = webuiL('msg.error', 'Error') + ': ' + webuiErrText(e);
		webuiToastError(webuiL('fm.upload_fail', 'Upload error'), e);
		if (uploaded)
			webuiV1('DELETE', '/storage/path', {query: {path: part}}).catch(() => {});
	}

	// Reset file input
	document.getElementById('fileInput').value = '';
}

// Drag & Drop
const dropzone = document.getElementById('dropzone');
dropzone.addEventListener('dragover', function(e) {
	e.preventDefault();
	this.classList.add('dragover');
});
dropzone.addEventListener('dragleave', function() {
	this.classList.remove('dragover');
});
dropzone.addEventListener('drop', function(e) {
	e.preventDefault();
	this.classList.remove('dragover');
	if (e.dataTransfer.files.length > 0) {
		uploadFile(e.dataTransfer.files[0]);
	}
});

// ===== Preview Updates =====

function updatePreviews() {
	const sidebarPreview = document.getElementById('sidebarPreview');
	const filesPreview = document.getElementById('filesPreview');
	const where = currentPath || webuiL('fm.roots', 'Permitted folders');
	if (sidebarPreview) sidebarPreview.textContent = rootOf(currentPath) || where;
	if (filesPreview) filesPreview.textContent = where;
}

// ===== Pull-to-Refresh =====

function isAtScrollTop() {
	const expandedCard = document.querySelector('.webui-card:not(.collapsed)');
	if (expandedCard) {
		const cardBody = expandedCard.querySelector('.webui-card-body');
		if (cardBody && cardBody.scrollTop > 5) return false;
	}
	const main = document.querySelector('.webui-files-grid');
	if (main && main.scrollTop > 5) return false;
	return true;
}

// ===== Initialize =====

// The roots first, so the top level and the breadcrumb know them; an
// address with #<path> inside them opens there
webuiExtTexts('files').then(async function() {
	await loadQuickDirs();
	let asked = '';
	try { asked = decodeURIComponent(location.hash.replace(/^#/, '')); } catch (e) { /* not an address */ }
	loadDir(asked && rootOf(normPath(asked)) ? asked : '');
});

webuiExtTexts('files').then(function() {
	webuiInitMobileAccordion({
		cardOrder: ['card-sidebar', 'card-files'],
		storageKey: 'webui-files-card',
		defaultCard: 'card-files',
		breakpoint: 800,
		indicatorId: 'cardIndicator',
		containerSelector: '.webui-files-grid',
		mode: 'collapsed'
	});
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		if (!isAtScrollTop()) return Promise.resolve();
		return loadDir(currentPath);
	});
}

if (typeof window.webuiIsAtScrollTop === 'undefined') {
	window.webuiIsAtScrollTop = isAtScrollTop;
}
