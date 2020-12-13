
var menus = {
	"文件": [
		{
			item: "新建",
			shortcut: "Ctrl+N",
			action: file_new,
			description: "建立新文件.",
		},
		{
			item: "打开",
			shortcut: "Ctrl+O",
			action: file_open,
			description: "打开一个文件.",
		},
		{
			item: "保存",
			shortcut: "Ctrl+S",
			action: file_save,
			description: "保存文件.",
		},
		{
			item: "另存为",
			shortcut: "Ctrl+Shift+S",
			// in mspaint, no shortcut is listed, but it supports F12; it doesn't support Ctrl+Shift+S
			action: file_save_as,
			description: "另存为.",
		},
		$MenuBar.DIVIDER,
		{
			item: "网络图片",
			// shortcut: "Ctrl+L",
			action: file_load_from_url,
			description: "打开网络图片.",
		},
		{
			item: "上传文件",
			action: upload_to_imgur,
			description: "上传文件",
		},
		$MenuBar.DIVIDER,
		{
			item: "内存管理",
			action: manage_storage,
			description: "管理所有打开的图像.",
		},
		$MenuBar.DIVIDER,
		{
			item: "打印预览",
			action: function(){
				print();
			},
			description: "打印活动文档并设置打印选项.",
			//description: "Displays full pages.",
		},
		{
			item: "页面设置",
			action: function(){
				print();
			},
			description: "页面设置.",
			//description: "Changes the page layout.",
		},
		{
			item: "打印",
			shortcut: "Ctrl+P",
			action: function(){
				print();
			},
			description: "打印.",
		},
		$MenuBar.DIVIDER,
		{
			item: "设为壁纸",
			action: set_as_wallpaper_tiled,
			description: "设为壁纸.",
		},
		{
			item: "设为壁纸(居中)", // in mspaint it's Wa&llpaper
			action: set_as_wallpaper_centered,
			description: "设为壁纸(居中).",
		},
		$MenuBar.DIVIDER,
		{
			item: "最近文件",
			enabled: false, // @TODO for chrome app / desktop app
			description: "",
		},
		$MenuBar.DIVIDER,
		{
			item: "退出",
			shortcut: "Alt+F4",
			action: function(){
				close();
			},
			description: "退出软件.",
		}
	],
	"编辑": [
		{
			item: "撤销",
			shortcut: "Ctrl+Z",
			enabled: function(){
				return undos.length >= 1;
			},
			action: undo,
			description: "撤销.",
		},
		{
			item: "重做",
			shortcut: "F4",
			enabled: function(){
				return redos.length >= 1;
			},
			action: redo,
			description: "重做先前未完成的动作.",
		},
		$MenuBar.DIVIDER,
		{
			item: "剪切",
			shortcut: "Ctrl+X",
			enabled: function(){
				// @TODO disable if no selection (image or text)
				return (typeof chrome !== "undefined") && chrome.permissions;
			},
			action: function(){
				document.execCommand("cut");
			},
			description: "剪切.",
		},
		{
			item: "复制",
			shortcut: "Ctrl+C",
			enabled: function(){
				// @TODO disable if no selection (image or text)
				return (typeof chrome !== "undefined") && chrome.permissions;
			},
			action: function(){
				document.execCommand("copy");
			},
			description: "复制.",
		},
		{
			item: "粘贴",
			shortcut: "Ctrl+V",
			enabled: function(){
				return (typeof chrome !== "undefined") && chrome.permissions;
			},
			action: function(){
				document.execCommand("paste");
			},
			description: "粘贴.",
		},
		{
			item: "清除",
			shortcut: "Del",
			enabled: function(){ return !!selection; },
			action: delete_selection,
			description: "清除选中区域.",
		},
		{
			item: "全选",
			shortcut: "Ctrl+A",
			action: select_all,
			description: "全选.",
		},
		$MenuBar.DIVIDER,
		{
			item: "复制到...",
			enabled: function(){ return !!selection; },
			action: save_selection_to_file,
			description: "复制到...",
		},
		{
			item: "粘贴来自...",
			action: paste_from_file_select_dialog,
			description: "粘贴来自...",
		}
	],
	"视图": [
		{
			item: "工具",
			shortcut: "Ctrl+T",
			checkbox: {
				toggle: function(){
					$toolbox.toggle();
				},
				check: function(){
					return $toolbox.is(":visible");
				},
			},
			description: "工具.",
		},
		{
			item: "颜色",
			shortcut: "Ctrl+L",
			checkbox: {
				toggle: function(){
					$colorbox.toggle();
				},
				check: function(){
					return $colorbox.is(":visible");
				},
			},
			description: "颜色.",
		},
		{
			item: "状态栏",
			checkbox: {
				toggle: function(){
					$status_area.toggle();
				},
				check: function(){
					return $status_area.is(":visible");
				},
			},
			description: "显示/隐藏状态栏.",
		},
		{
			item: "文字工具栏",
			enabled: false, // @TODO
			checkbox: {},
			description: "显示/隐藏文字工具栏.",
		},
		$MenuBar.DIVIDER,
		{
			item: "额外菜单",
			checkbox: {
				toggle: function(){
					$extras_menu_button.toggle();
					try{
						localStorage["jspaint extras menu visible"] = this.check();
					}catch(e){}
				},
				check: function(){
					return $extras_menu_button.is(":visible");
				}
			},
			description: "显示/隐藏额外菜单.",
		},
		$MenuBar.DIVIDER,
		{
			item: "缩放",
			submenu: [
				{
					item: "正常",
					shorcut: "Ctrl+PgUp",
					description: "100%.",
					enabled: function(){
						return magnification !== 1;
					},
					action: function(){
						set_magnification(1);
					},
				},
				{
					item: "放大",
					shorcut: "Ctrl+PgDn",
					description: "400%.",
					enabled: function(){
						return magnification !== 4;
					},
					action: function(){
						set_magnification(4);
					},
				},
				{
					item: "缩放",
					enabled: false, // @TODO
					description: "缩放图片.",
				},
				$MenuBar.DIVIDER,
				{
					item: "网格",
					shorcut: "Ctrl+G",
					enabled: false, // @TODO
					checkbox: {},
					description: "显示/隐藏网格.",
				},
				{
					item: "缩略图",
					enabled: false, // @TODO
					checkbox: {},
					description: "显示缩略图.",
				}
			]
		},
		{
			item: "查看图像",
			shortcut: "Ctrl+F",
			action: view_bitmap,
			description: "查看图像.",
		}
	],
	"图像": [
		{
			item: "旋转",
			shortcut: "Ctrl+R",
			action: image_flip_and_rotate,
			description: "翻转/旋转.",
		},
		{
			item: "拉伸",
			shortcut: "Ctrl+W",
			action: image_stretch_and_skew,
			description: "拉伸/扭曲.",
		},
		{
			item: "反色",
			shortcut: "Ctrl+I",
			action: image_invert,
			description: "反色.",
		},
		{
			item: "属性...",
			shortcut: "Ctrl+E",
			action: image_attributes,
			description: "属性.",
		},
		{
			item: "清空",
			shortcut: "Ctrl+Shift+N",
			//shortcut: "Ctrl+Shft+N", [sic]
			action: clear,
			description: "清空.",
		},
		{
			item: "透明",
			checkbox: {
				toggle: function(){
					transparent_opaque = {
						"opaque": "transparent",
						"transparent": "opaque",
					}[transparent_opaque];

					$G.trigger("option-changed");
				},
				check: function(){
					return transparent_opaque === "opaque";
				},
			},
			description: "使当前选择不透明或透明.",
		}
	],
	"颜色": [
		{
			item: "编辑...",
			action: function(){
				$colorbox.edit_last_color();
			},
			description: "新建颜色.",
		},
		{
			item: "获取...",
			action: function(){
				get_FileList_from_file_select_dialog(function(files){
					var file = files[0];
					Palette.load(file, function(err, new_palette){
						if(err){
							show_error_message("This file is not in a format that paint recognizes, or no colors were found.");
						}else{
							palette = new_palette;
							$colorbox.rebuild_palette();
						}
					});
				});
			},
			description: "使用先前保存的颜色调色板.",
		},
		{
			item: "保存...",
			action: function(){
				var blob = new Blob([JSON.stringify(palette)], {type: "application/json"});
				saveAs(blob, "colors.json");
			},
			description: "保存颜色...",
		}
	],
	"帮助": [
		{
			item: "使用帮助",
			action: show_help,
			description: "显示当前任务或命令的帮助。.",
		},
		$MenuBar.DIVIDER,
		{
			item: "关于",
			action: function(){
				var $msgbox = new $Window();
				$msgbox.title("关于本程序");
				$msgbox.$content.html(
					"<h1><img src='images/icons/32.png'/>小木木绘图<hr/></h1>" +
					"<p>浏览器上的绘图软件</p>" +
					"<p>欢迎访问:<a href='http://www.licings.com'>www.licings.com</a></p>"
				).css({padding: "15px"});
				$msgbox.center();
			},
			description: "使用帮助",
			//description: "Displays program information, version number, and copyright.",
		}
	],
	"更多...": [
		{
			item: "创建gif",
			// shortcut: "Ctrl+Shift+G",
			action: render_history_as_gif,
			description: "从文档历史中创建gif.",
		},
		// {
		// 	item: "Render History as &APNG",
		// 	// shortcut: "Ctrl+Shift+A",
		// 	action: render_history_as_apng,
		// 	description: "Creates an animation from the document history.",
		// },
		// {
		// 	item: "&Additional Tools",
		// 	action: function(){
		// 		// ;)
		// 	},
		// 	description: "Enables extra editing tools.",
		// },
		// {
		// 	item: "&Preferences",
		// 	action: function(){
		// 		// :)
		// 	},
		// 	description: "Configures JS Paint.",
		// }
		{
			item: "多用户",
			submenu: [
				{
					item: "共享session",
					action: function(){
						var name = prompt("输入将在URL中用于共享session名称：");
						if(typeof name == "string"){
							name = name.trim();
							if(name == ""){
								show_error_message("session不能为空");
							}else if(name.match(/[.\/\[\]#$]/)){
								show_error_message("session不能包含./[]#$");
							}else{
								location.hash = "session:" + name;
							}
						}
					},
					description: "从当前文档开始一个新的多用户session",
				},
				{
					item: "新建空白session",
					action: function(){
						show_error_message("不支持...");
					},
					enabled: false,
					description: "从空文档开始一个新的多用户session",
				},
			]
		},
		{
			item: "主题",
			submenu: [
				{
					item: "经典",
					action: function(){
						set_theme("classic.css");
					},
					enabled: function(){
						return get_theme() != "classic.css"
					},
					description: "Windows 98...",
				},
				{
					item: "现代",
					action: function(){
						set_theme("modern.css");
					},
					enabled: function(){
						return get_theme() != "modern.css"
					},
					description: "现代主题.",
				},
			]
		},
	],
};

var go_outside_frame = false;
if(frameElement){
	try{
		if(parent.$MenuBar){
			$MenuBar = parent.$MenuBar;
			go_outside_frame = true;
		}
	}catch(e){}
}
var $menu_bar = $MenuBar(menus);
if(go_outside_frame){
	$menu_bar.insertBefore(frameElement);
}else{
	$menu_bar.prependTo($V);
}

$menu_bar.on("info", function(e, info){
	$status_text.text(info);
});
$menu_bar.on("default-info", function(e){
	$status_text.default();
});

var $extras_menu_button = $menu_bar.get(0).ownerDocument.defaultView.$(".extras-menu-button");
try{
	// TODO: refactor shared key string
	if(localStorage["jspaint extras menu visible"] != "true"){
		$extras_menu_button.hide();
	}
}catch(e){}
