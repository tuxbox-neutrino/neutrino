/*	yWeb by yjogol
	internal organisation of yweb
	$Date$
	$Revision$
*/

/* define namespace */
if(typeof(Y) == "undefined")
	Y = {};
/* Class Y.yweb */
Y.yweb = new Class.create();
Object.extend(Y.yweb.prototype, {
	ver_file_prop : new Hash(),
	yweb_version:  $H({major:'0', minor:'0', patch:'0', pre:'0'}),
	yweblib_version:  $H({major:'1', minor:'0', patch:'0', pre:'0'}),
	prototype_version:  $H({major:'0', minor:'0', patch:'0', pre:'0'}),
	baselib_version:  $H({major:'1', minor:'0', patch:'0', pre:'0'}),

	initialize: function(){
		this.ver_file_get();
		split_version(this.ver_file_prop.get('version'),this.yweb_version);
		split_version(Prototype.Version,this.prototype_version);
		if(typeof(baselib_version)!="undefined")
			split_version(baselib_version,this.baselib_version);
	},
	ver_file_get: function(){
		var v_file=loadSyncURL("/Y_Version.txt");
		var lines=v_file.split("\n");
		lines.each(function(e){
			var x=e.split("=");
			if(x.length==2 && (x[0]!=""))
				this.ver_file_prop.set(x[0],x[1]);
		},this);
	},
	_require: function(min_version_str,check_version,is_msg,msg_text){
		var v=$H();
		split_version(min_version_str,v);
		if (!version_le(v, check_version)) {
			if (typeof(is_msg) != "undefined") alert(msg_text + min_version_str + " required");
			return false;
		}
		else 
			return true;
	},
	require_prototype:function(min_version_str,is_msg){
		return this._require(min_version_str,this.prototype_version,is_msg,"prototype-library version: ");
	},
	require_baselib:function(min_version_str,is_msg){
		return this._require(min_version_str,this.baselib_version,is_msg,"baselib-library version: ");
	},
	require_yweblib:function(min_version_str,is_msg){
		return this._require(min_version_str,this.yweblib_version,is_msg,"yweb-library version: ");
	},
	require:function(min_version_str,is_msg){
		return this._require(min_version_str,this.yweb_version,is_msg,"yweb version: ");
	}
});


/* main instance */
if (window == top.top_main.prim_menu) {
	var yweb = new Y.yweb();
	yweb.require_prototype("1.6");
}
else 
	if(top.top_main.prim_menu && top.top_main.prim_menu.yweb)
		var yweb = top.top_main.prim_menu.yweb;
	else { // should not happen!
		var yweb = new Y.yweb();
		yweb.require_prototype("1.6");
	}
	
/* n/m= type, menuitem, desc, file, tag, version, url, yweb_version, info_url
 * x= type, menuitem, ymenu, file, tag, version, url, yweb_version, info_url
 * u=type,site,description,url
 */


/* Class Y.extension */
Y.extension = new Class.create();
Object.extend(Y.extension.prototype, {
	ext_version: 1, /* ver of Y.extention*/
	conf_version: 2,/* ver of local conf file */
	upd_version: 2, /* ver of upd file */
	file: "",
	installed_extensions: [],
	update_sites: [],
	upd_extensions: [],
	on_get_updates: null,	
	get_file: function(){
		this.file = loadSyncURL("/control/exec?Y_Tools&get_extension_list&" + Math.random());
	},
	read_items: function(){
		this.installed_extensions=[];
		this.update_sites=[];
		this.conf_version=2;
		this.get_file();
		if(this.file!=""){
			var list = this.file.split("\n");
			list.each(function(line){
				var p=str_to_hash(line);
				switch(p.get('type')){
					case "m": case "n": case "p": case "x": case "s": case "o":
						this.installed_extensions.push(p);
						break;
					case "u":
						this.update_sites.push(p);
						break;
					case "v":
						this.conf_version=p.get('version');
						break;
				}
			},this);
		}
	},
	get_updates: function(){
		this.upd_extensions=[];
		this.upd_version=2;
		this.update_sites.each(function(e){
			var update_file = loadSyncURL("/control/exec?Y_Tools&url_get&"+e.get('url')+"&ext_upt.txt&" + Math.random());
			if(this.on_get_updates) this.on_get_updates(e.get('url'), (update_file.search(/wget: cannot/)!=-1) );
			var list = update_file.split("\n");
			list.each(function(line){
				var p=str_to_hash(line);
				switch(p.get('type')){
					case "m": case "n": case "p": case "x": case "s": case "o":
						p.set('site', e.get('site'));
						this.upd_extensions.push(p);
						break;
					case "v":
						this.upd_version=p.get('version');
						break;
				}
			},this);
		},this);
		this.upd_extensions = this.upd_extensions.sortBy(function(e){return e.get('tag');});
	},
	build_extension_file: function(){
		var ext="version:"+this.conf_version+"\n";
		for (i = 0; i < this.installed_extensions.length; i++)
			ext+=hash_to_str(this.installed_extensions[i])+"\n"; 
		for(i=0;i<this.update_sites.length;i++)
			ext+=hash_to_str(this.update_sites[i])+"\n"; 
		return ext;
	},
	add_extension_to_installed: function(props)	{
		var allready_installed = false;
		for(i=0;i<this.installed_extensions.length;i++){
			if(this.installed_extensions[i].get('tag')==props.get('tag')){
				allready_installed = true;
				this.installed_extensions[i]=props;
			}
		}
		if(!allready_installed)
			this.installed_extensions.push(props);
	},
	install: function(item){
		var res = loadSyncURL("/control/exec?Y_Tools&ext_installer&"+item.get('url'));
		if(res.search(/error/)==-1)
			this.add_extension_to_installed(item);
		return res;
	},
	get_item_by_tag:function(tag){
		return this.installed_extensions.find(function(e){
			return e.get('tag')==tag;		
		},this);
	},
	uninstall: function(tag){
		var res = loadSyncURL("/control/exec?Y_Tools&ext_uninstaller&"+tag);
		if(res.search(/error/)==-1)
			this.installed_extensions = this.installed_extensions.without(this.get_item_by_tag(tag));
		return res;
	},
	select_menu: function(menu_name){
		return this.installed_extensions.findAll(function(e){
			return e.get('type')=="x" && e.get('ymenu')==menu_name;		
		},this);
	},
	select_type: function(_type){
		return this.installed_extensions.findAll(function(e){
			return e.get('type')==_type;		
		},this);
	}
});

function add_yExtensions(_ymenu, _id) {
	var menu=ext.select_menu(_ymenu);
	menu.each(function(e){
		var el=new Element('li').update(
			new Element('a', {
				'class': (_ymenu == 'main') ? 'y_menu_prim_ext' : 'y_menu_sec_ext',
				'target': (_ymenu == 'main') ? 'base' : 'work',
				'title': e.get('desc'),
				'href': e.get('file')
			}).update(e.get('menuitem'))
		);
		$(_id).insert({'bottom':el});
	});
}

/* singleton pattern*/
if (window == top.top_main.prim_menu) {
	var ext = new Y.extension();
	ext.read_items();
}
else 
	if(top.top_main.prim_menu && top.top_main.prim_menu.ext)
		var ext = top.top_main.prim_menu.ext;
	else { // should not happen!
		var ext = new Y.extension();
		ext.read_items();
	}

