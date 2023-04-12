var arr_length = 0;
var arr_index = 0;

var task_running = false;
var task_finish_play = false;

var button_play = document.getElementById("disk");
var audio = document.getElementById("audio");
var music_src_prefix = 'http://music.163.com/song/media/outer/url?id=';
var music_src_suffix = '.mp3';
var timer;
var arr_song_name 	= [];
var arr_song_id 	= [];
var arr_song_cover 	= [];

window.onload = function resource_init () {
	var req = {};
	req.ip = "music.163.com";
	req.port = 80;
	req.https = false;
	req.host = "music.163.com";
	req.uri = "/api/playlist/detail?id=2137714071";

	axios.post('/proxy', req )
	.then(function (response) {
		resource_cut( response );
		task_start ( false );
	})
	.catch(function (error) {
		alert("Get music list failed...");
	});
}
function resource_cut( response ) {
	var t;
	arr_length = response.data.result.tracks.length;
	for( var i =0; i < arr_length; i ++ ) {
		t =  response.data.result.tracks[i].name ;
		arr_song_name.push ( t )
		t =  response.data.result.tracks[i].id ;
		arr_song_id.push ( t );
		t =  response.data.result.tracks[i].album.blurPicUrl;
		arr_song_cover.push ( t );
	}
}
function task_state_change ( state ) {
	task_running = state;
}
function task_start ( play ) {
	arr_index = (arr_index) % arr_length;
	document.getElementById("name").innerHTML = "&nbsp&nbsp"
	 + ( arr_song_name[arr_index] ).toUpperCase() + "&nbsp&nbsp";
	document.getElementById("disk_inring").src = "/images/inring.png";
	audio.src = music_src_prefix + arr_song_id[arr_index] + music_src_suffix;
	task_finish_play = play;
	task_state_change( true );
}
audio.addEventListener( 'canplay', function task_finish () {
	timer = setInterval( function() {
		clearInterval(timer);
		document.getElementById("disk_inring").src = arr_song_cover[arr_index];
		// document.getElementById("music_core_dec").style.background =
		// "url(" + arr_song_cover[arr_index] + ") no-repeat 0 center";
		task_state_change( false );
		if( task_finish_play ) {
			pause2play();
			task_finish_play = !task_finish_play;
		}
		arr_index ++;
	}, 400 );
}, false );
audio.addEventListener( 'ended', function () {
	play2pause();
	task_start(true);
}, false );
audio.addEventListener( 'error', function () {
	play2pause();
	task_start(true);
}, false );
window.onbeforeunload = function(){ }
function play2pause(){
	document.getElementById("disk").className = "disk disk-rollstop";
	document.getElementById("needle").className = "needle needle-pause";
	audio.pause();
}
function pause2play(){
	if( task_running ) {
		alert("Please wait a moment...");
		return;
	}
	document.getElementById("disk").className = "disk disk-rollstart";
	document.getElementById("needle").className = "needle needle-play";
	audio.play();
}
function play_function( ) {
	if( !audio.paused ) {
		play2pause();
	} else {
		pause2play();
	}
}
button_play.onclick = play_function;
