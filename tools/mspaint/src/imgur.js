var $imgur_window;

function upload_to_imgur(){
	if($imgur_window){
		$imgur_window.close();
	}
	$imgur_window = $FormWindow().title("上传图片到服务器").addClass("dialogue-window");
	$imgur_window.$main.html(
		"<label>URL: </label>" +
		"<label id='imgur-description'>点击上传开始上传</label>" +
		"<a id='imgur-url' href='' target='_blank'></a>"
	);

	var $imgur_url = $imgur_window.$main.find("#imgur-url");
	var $imgur_description = $imgur_window.$main.find("#imgur-description");

	$imgur_window.$Button("上传", function(){
		// base64 encoding to send to imgur api
		var base64 = canvas.toDataURL().split(",")[1];
		var payload = {
			image: base64,
		};

		// send ajax call to the imgur image upload api
		$.ajax({
			type: "POST",
			url: "http://www.lcings.com/data/image",
			headers: {
				"Authorization":"Client-ID 203da2f300125a1",
			},
			dataType: 'json',
			data: payload,
			beforeSend: function(){
				$imgur_description.text("上传中...");
			},
			success: function(data){
				var url = data.data.link;
				$imgur_description.text("");
				$imgur_url.text(url);
				$imgur_url.attr('href', url);
			},
			error: function(error){
				$imgur_description.text("上传出错:(");
			},
		})
	});
	$imgur_window.$Button("取消", function(){
		$imgur_window.close();
	});
	$imgur_window.width(300);
	$imgur_window.center();
}
