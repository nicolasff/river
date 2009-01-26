<script type="text/javascript" src="jquery.js"></script>
<script type="text/javascript">

	//console.log("loading...");
	window.onload = function() {
	//	console.log("window loaded...");
		jsc = '';
		$.ajax({
			dataType: 'jsonp',
			url: "http://172.16.3.170:1234/test.js?callback=thamer",
			data: null,
			//async: true,
			jsonp: "plop"
		});
	}
//	console.log("still loading...");


	function lol(x) {
		alert(x);
//		console.log("got x=", x);
	}
/*
	var url = ;
	var request =  new XMLHttpRequest();
	request.open("POST", url, true);
	request.onreadystatechange = function() {
		if(request.status == 200) {
			if(request.responseText) {
				console.log(request.readyState, request.responseText);
			}
		}
	};
	request.send();
 */
</script>

