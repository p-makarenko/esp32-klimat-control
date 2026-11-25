function f_check_ip(ip)     
{     
    var reg =  /^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/     
    var ret = reg.test(ip);
	if(!ret){
		alert("IPµØÖ·¸ñÊ½´íÎó£¡");
	}
	return ret;
}

function remove_all_ssid(){
	var obj=document.getElementById('slist');
	obj.options.length=0;
}


function getSsidListInfo(){
	$.mobile.defaultPageTransition = 'none';
	$.ajax({
		type: "get",
		url: "system/ssidlst.php",
		cache:false,
        success: function (data) {
        	remove_all_ssid();
        	var obj=document.getElementById('slist');
        	var result = eval(data);
        	for (var i = 0; i < result.length; i++) {
        		//result[i]±íÊ¾»ñµÃµÚi¸öjson¶ÔÏó¼´JSONObject
        		//result[i]Í¨¹ý.×Ö¶ÎÃû³Æ¼´¿É»ñµÃÖ¸¶¨×Ö¶ÎµÄÖµ 
        		obj.options[i] = new Option(result[i].ssid,i);
        		obj.options[i].selected = false;
    		}
    		if(result.length > 0){
    			$("#slist").val(0).selectmenu('refresh', true);
    		};
    		alert("ËÑË÷Íê³É");
		}  
	});
}

function getWifiInfo(){
	$.mobile.defaultPageTransition = 'none';
	$.ajax({
		type: "get",
		url: "system/status.php",
		cache:false,
        success: function (data) {
			var user = eval('(' + data + ')');
			$('#ver').html(user.ver);
			$('#kmode').val(user.mode);
			$('#mode').html($('#kmode  option:selected').text());	
			$('#smode').html($('#kmode  option:selected').text());
			
			$('#assid').html(user.assid);
			$('#aip').html(user.aip);
			$('#wip').html(user.aip);
			$('#sip').val(user.aip);
			
			$('#amac').html(user.amac);
			$('#wmac').html(user.amac);
			
			if(0 == user.stats){
				$('#stats').html("Disconnected");
				$("#_ip").hide();
				$("#_sig").hide();
				$("#_ssid").hide();
			}else{
				$('#stats').html("Connected");
				$("#_ip").show();
				$("#_sig").show();
				$("#_ssid").show();				
				$('#ip').html(user.ip);
				$('#signal').html(user.signal+" dbm");				
				$('#ssid').html(user.ssid);
			}
			
			
			$('#mac').html(user.mac);
			
			$('#wssid').html(user.assid);
			$('#iwssid').val(user.assid);
			
			$('#wchn').html(""+user.wchn+"");
			$('#wchns').val(user.wchn);
			

			$('#iwep').val(user.iwep);
			$('#wep').html(user.iwep);
			$('#wmask').html(user.amask);
			$('#smask').val(user.amask);
			$('#wdhcps').val(user.adhcp);
			$('#sssid').html(user.ssid);
			$('#ssmac').html(user.mac);
			$('#sswep').html(user.swep);
			if(user.sdhcp > 0){
				$('#ssdhcp').html("OPEN");
			}else{
				$('#ssdhcp').html("CLOSE");
			}
			$('#ssip').html(user.ip);
			$('#ssmask').html(user.smask);
			$('#ssgw').html(user.sgw);
			$('#ssdns').html(user.sdns);
			$('#siwssid').val(user.ssid);
			$('#siwep').val(user.swep);
			$('#kdhcps').val(user.sdhcp);
			$('#ksip').val(user.ip);
			$('#kmask').val(user.smask);
			$('#kgw').val(user.sgw);
			$('#kbaud').val(user.baud);
			$('#sbaud').html($('#kbaud  option:selected').text());
			$('#ktype').val(user.type);
			$('#stype').html($('#ktype  option:selected').text());				
			
			$('#sserver').html(user.sserver);
			$('#kserver').val(user.sserver);
			$('#sport').html(""+user.port+"");
			$('#kport').val(""+user.port+"");
			$('#stimeout').html(""+user.timeout+"");
			$('#ktimeout').val(""+user.timeout+"");
			$('#suser').html(user.wuser);
			$('#kuser').val(user.wuser);
			$('#skey').html(user.wkey);
			$('#kkey').val(user.wkey);
		}  
	});
}




function ButtonCheckInit(){
	$("#btwo_1").click(function(){
		var string = $('#iwssid').val();
		if(0==string.length||string.length>32){
			string = $('#wssid').html();
			$('#iwssid').val(string);
			alert("SSID³¤¶È´íÎó£¡");
			return;
		}
		$('#wssid').html(string);
	});
	$("#btwo_2").click(function(){
		var string = $('#wchns').val();
		$('#wchn').html(string);
	});
	$("#btwo_3").click(function(){
		var string = $('#iwep').val();
		if(string.length>64){
			string = $('#wep').html();
			$('#iwep').val(string);
			alert("ÃÜÂë³¤¶È´íÎó£¡");
			return;
		}
		$('#wep').html(string);
		
	});
	$("#btwo_4").click(function(){
		var string = $('#sip').val();
		if(!f_check_ip(string)){
			string = $('#wip').html();
			$('#sip').val(string);
			return;
		}
		$('#wip').html(string);
	});

	$("#btwo_5").click(function(){
		var string = $('#smask').val();
		if(!f_check_ip(string)){
			string = $('#wmask').html();
			$('#smask').val(string);
			return;
		}
		$('#wmask').html(string);
	});
	
	$("#bthr_1").click(function(){
		var string = $('#siwssid').val();
		if(string.length>32){
			string = $('#sssid').html();
			$('#siwssid').val(string);
			alert("SSID³¤¶È´íÎó£¡");
			return;
		}
		$('#sssid').html(string);
	});	
	$("#bthr_2").click(function(){
		var string = $('#siwep').val();
		if(string.length>64){
			string = $('#sswep').html();
			$('#siwep').val(string);
			alert("ÃÜÂë³¤¶È´íÎó£¡");
			return;
		}
		$('#sswep').html(string);	
	});
	$("#bthr_3").click(function(){
		var string = $('#kdhcps  option:selected').text();
		$('#ssdhcp').html(string);
	});
	$("#bthr_4").click(function(){
		var string = $('#ksip').val();
		if(!f_check_ip(string)){
			string = $('#ssip').html();
			$('#ksip').val(string);
			return;
		}
		$('#ssip').html(string);
	});
	$("#bthr_5").click(function(){
		var string = $('#kmask').val();
		if(!f_check_ip(string)){
			string = $('#ssmask').html();
			$('#kmask').val(string);
			return;
		}
		$('#ssmask').html(string);
	});
	$("#bthr_6").click(function(){
		var string = $('#kgw').val();
		if(!f_check_ip(string)){
			string = $('#ssgw').html();
			$('#kgw').val(string);
			return;
		}
		$('#ssgw').html(string);
	});
	$("#bfour_1").click(function(){
		var string = $('#kmode  option:selected').text();
		$('#smode').html(string);
	});
	$("#bfour_2").click(function(){
		var string = $('#ktype  option:selected').text();
		$('#stype').html(string);
	});
	$("#bfour_3").click(function(){
		var string = $('#kserver').val();
		if(string.length>64 || string.length < 5){
			string = $('#sserver').html();
			$('#kserver').val(string);
			alert("DNS»òÕßIP³¤¶È´íÎó£¡");
			return;
		}
		$('#sserver').html(string);	
	});
	$("#bfour_4").click(function(){
		var string = $('#kport').val();

		$('#sport').html(string);	
	});
	$("#bfour_5").click(function(){
		var string = $('#ktimeout').val();

		$('#stimeout').html(string);	
	});
	$("#bfour_6").click(function(){
		var string = $('#kbaud  option:selected').text();
		$('#sbaud').html(string);	
	});
	$("#bfour_9").click(function(){
		var string = $('#kuser').val();
		if(string.length>32){
			string = $('#suser').html();
			$('#kuser').val(string);
			alert("ÕËºÅ³¤¶È´íÎó£¡");
			return;			
		}
		$('#suser').html(string);	
	});
	$("#bfour_10").click(function(){
		var string = $('#kkey').val();
		if(string.length>32){
			string = $('#skey').html();
			$('#kkey').val(string);
			alert("ÃÜÂë³¤¶È´íÎó£¡");
			return;			
		}
		$('#skey').html(string);	
	});
	
	$(".ksaveh").click(function(){
		var jsObj = {};  
		jsObj.apname = $('#iwssid').val();  
		jsObj.apchn = $('#wchns').val();  
		jsObj.apkey = $('#iwep').val();
		jsObj.apip = $('#sip').val();
		jsObj.apmask = $('#smask').val();
		jsObj.staname = $('#sssid').html();
		jsObj.stakey = $('#siwep').val();
		jsObj.stadhcp = $('#kdhcps').val();
		jsObj.staip = $('#ksip').val();
		jsObj.stamask = $('#kmask').val();
		jsObj.stagw = $('#kgw').val();
		jsObj.type = $('#ktype').val();
		jsObj.server = $('#kserver').val();
		jsObj.port = $('#kport').val();
		jsObj.timeout = $('#ktimeout').val();
		jsObj.baud = $('#kbaud  option:selected').text();
		jsObj.Nuser = $('#kuser').val();
		jsObj.Nkey = $('#kkey').val();
		jsObj.mode = $('#kmode').val();
		jsObj.active = 3;  
		var str = JSON.stringify(jsObj);
		$.post("system/set.php",str,function(data,status){
			alert("ÉèÖÃ³É¹¦!");
			
		});
	});
	$(".krefreshh").click(function(){
		var jsObj = {};
		jsObj.active = 5; 
		var str = JSON.stringify(jsObj);
		$.post("system/set.php",str,function(data,status){
			alert("ÖØÖÃ³É¹¦!");
		});
	});	
}



