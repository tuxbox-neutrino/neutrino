// The script of remote.html: the interface's remote control panel as a
// page of its own, drawn once the texts are in. Its keys go over /api/v1
// (webui-v1.js), as from the window the dashboard opens.
webuiAfterTexts(function() {
	webuiRenderRemote('remoteHost', {id: 'webui-remote-page-panel', onKey: 'webuiSendKey'});
});
