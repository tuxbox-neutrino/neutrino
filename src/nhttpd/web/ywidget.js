/* Y widget library
   by yjogol 2007
*/

// Y name space
if(typeof(Y) == "undefined")
	Y = {};

/* Y.Base */
Y.Base = {};
Y.Base.Overlay = {
	overlayname : 'yoverlay',
	overlay: null,
	isOverlay: false,
	init: function() {
		this.buildOverlay();
	},
	buildOverlay: function(){
		var bod	= document.getElementsByTagName('body')[0];
		this.overlay	= document.createElement('div');
		this.overlay.id	= 'yoverlay';
		this.hideOverlay();
		bod.appendChild(this.overlay);
	},
	showOverlay: function(){
		this.isOverlay = true;
		this.overlay.show();
	},
	hideOverlay: function(){
		this.overlay.hide();
		this.isOverlay = false;
	},
	showElement: function(e,x,y){
		if(typeof(x) != "undefined" && typeof(y) != "undefined")
			$(e).setStyle({left: x+"px", top: y+"px"});
		$(e).show();
	},
	showOverlayedElement: function(e,x,y){
		this.showOverlay();
		this.showElement(e,x,y);
	},
	hideElement: function(e){
		if(this.isOverlay)
			this.hideOverlay();
		$(e).hide();
	}
};
//Event.observe(window, 'load', function(e){Y.Base.Overlay.init();}, false);
/* END - Y.Base */

/* Y.Dialog */
Y.Dialog = Class.create();
Object.extend(Y.Dialog.prototype, {
	el: null,
	initialize: function(e){
		el=e;
	},
	show: function(x,y){
		Y.Base.Overlay.showOverlayedElement(el,x,y);
	},
	hide: function(){
		Y.Base.Overlay.hideElement(el);
	}
});

/* Y.ContextMenu
   Menu activation: Use Tag ycontextmenu=<id of ul od menu>
*/
Y.ContextMenu = Class.create();

/* Meta Object */
Object.extend(Y.ContextMenu, {
	isMenu : false,
	menu_list : new Array(),
	selElement: null,
	selTrigger: null,
	init: function() {
		this.buildMenus();
		if(this.menu_list.length >0)
			this.attachEvents();
	},
	buildMenus: function() {
		var menus = $$('[ycontextmenu]');
		menu_list=null;
		for(i = 0; i < menus.length; i++)
			this.menu_list.push(new Y.ContextMenu(menus[i],Element.readAttribute(menus[i],'ycontextmenu')) );
	},
	attachEvents: function() {
		Event.observe(window, 'click', this.eventCheckOutsideClick.bindAsEventListener(this), false);
	},
	eventCheckOutsideClick : function(e) {
		if(this.isMenu){
			this.menu_list.each(function(s,e){
				if(s.active){
					s.hide();
					if(!Position.within(s.menu,Event.pointerX(e),Event.pointerY(e)))
						Event.stop(e);
					throw $break;
				}
			});
		}
	}
});

/*ContextMenu items*/
Object.extend(Y.ContextMenu.prototype, {
	active: false,
	menu: null,
	trigger: null,
	width:0,
	height:0,
	initialize: function(element,contextmenu){
		if(typeof(element) == "undefined")
			return;
		Event.observe(element, "click", this.eventClick.bindAsEventListener(this), false);
		this.menu=$(contextmenu);
		this.trigger=$(element);
		var dimensions = this.menu.getDimensions();
		this.width = dimensions.width;
		this.height = dimensions.height;
	},
	eventClick: function(e) {
		this.setPosition(Event.pointerX(e), Event.pointerY(e));
		Y.ContextMenu.selElement = Event.element(e);
		Y.ContextMenu.selTrigger = this.trigger;
		this.show();
		Event.stop(e);
	},
	setPosition: function(x,y){/*mouse click coordinates*/
		var dist=20;
        var winHeight = window.innerHeight || document.body.clientHeight;
        var winWidth = window.innerWidth || document.body.clientWidth;
		if(x+this.width>= winWidth-dist)
			x=winWidth-dist-this.width;
		if(y+this.height>= winHeight-dist)
			y=winHeight-dist-this.height;
		this.menu.setStyle({left: x+"px", top: y+"px"});
	},
	show: function(){
		this.active = true;
		Y.Base.Overlay.showOverlayedElement(this.menu);
		Y.ContextMenu.isMenu = true;
	},
	hide: function(){
		this.active = false;
		Y.Base.Overlay.hideElement(this.menu);
		Y.ContextMenu.isMenu = false;
	}
});
/* init Context Menus*/
//Event.observe(window, 'load', function(e){Y.ContextMenu.init();}, false);
//Event.observe(window, 'unload', Event.unloadCache, false);

/* END - Context Menu */

