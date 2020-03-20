var myChart = echarts.init(document.getElementById('dashboard'));
var x = [];
var y = [];
var series_arr = [];
var i = 0;
var j = 0;
var timer_a;
var timer_b;
var perform_json;
var running = 0;
option = {
	grid: {
        left: '3%',
        right: '4%',
        bottom: '3%',
        containLabel: true
    },
	tooltip : {
        trigger: 'axis',
        axisPointer: {
            type: 'cross',
            label: {
                backgroundColor: '#6a7985'
            }
        }
    },
	toolbox: {
        feature: {
            saveAsImage: {}
        }
    },
	xAxis: {
		type: 'category',
		boundaryGap: false,
		data: x
	},
	yAxis: {
		boundaryGap: [0, '50%'],
		type: 'value'
	},
	series: []
};
myChart.setOption(option);
function draw_alloc ( ) {
	var request = {};
	axios.get('/perform_info', request )
	.then( function (response) {
		perform_json = response;
		for( i = 0; i < perform_json.data.length; i ++ ) {
	
			y[i] = new Array();
			y[i].push( perform_json.data.result[i].success );
			
			series_arr.push ( {
				name: 'example' + perform_json.data.result[i].index,
				type: 'line',
				symbol: 'none',
				areaStyle: {
					normal: {}
				},
				data: y[i]
			} );
		}
		x.push ( j++ );
	})
	.catch(function (error) {
		alert("Please check your network...");
	});
	myChart.setOption ( {
		xAxis:  { data: x },
		series: series_arr
	} );
	running = 1;
}
function draw_update( ) {
	var request = {};
	axios.get('/perform_info', request )
	.then( function (response) {
		perform_json = response;
		for( i = 0; i < perform_json.data.length; i ++ ) {
			y[i].push( perform_json.data.result[i].success );
		}
		x.push ( j++ );
	})
	.catch(function (error) {
		alert("Please check your network...");
	});
	myChart.setOption ( {
		xAxis:  { data: x },
		series: series_arr
	} );
}
function draw_free( ) {
	x = [];
	y = [];
	series_arr = [];
	i = 0;
	j = 0;
	myChart.setOption(option);
}

function loop_stop ( ) {
	clearInterval( timer_a );
	clearInterval( timer_b );
	running = 0;
}
function loop_start( x ) {
	if( running == 1 ) {
		alert( "running, please wait a moment ..." );
		return;
	}
	draw_free( );
	draw_alloc( );
	timer_a = setInterval( draw_update, 1*1000 );
	timer_b = setInterval( loop_stop , x*1000+1*1000 );
}

