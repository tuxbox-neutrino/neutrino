// The script of remote.html: the interface's remote control panel as a
// page of its own, drawn once the texts are in, with the toggle of its
// full-screen view above it. Its keys go over /api/v1 (webui-v1.js), as
// from the window the dashboard opens.
webuiAfterTexts(function() {
	document.getElementById('remoteBar').innerHTML = webuiRemoteFullToggle();
	webuiRenderRemote('remoteHost', {id: 'webui-remote-page-panel', onKey: 'webuiSendKey'});
});
