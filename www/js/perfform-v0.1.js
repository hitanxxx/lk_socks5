var perform_run_time ;
var app;
var reg_num = /^[0-9]+$/
var reg_str = /^[A-Za-z]+$/
var reg_ipstr = /^[A-Za-z\.]+$/

Vue.component('model', {
	props: ['list', 'isactive'],
	template: `<div class="overlay" v-show="isactive">
	<div class="con">
	<h2 class="title">新增/修改</h2>
		<div class="overlay_content">
			<table>

			<tr>
				<td>https</td>
				<td>
				<input type="checkbox" id="" v-model="modifylist.https">
				<label for="checkbox">{{ modifylist.https }}</label>
				</td>
			</tr>


			<tr>
				<td>用例地址</td>
				<td><input type="text" maxlength="30" v-model="modifylist.ip"></td>
			</tr>

			<tr>
				<td>用例端口</td>
				<td><input type="text" maxlength="5" v-model.number="modifylist.port"></td>
			</tr>

			<tr>
				<td>method</td>
				<td>
					<select name="" id="" v-model="modifylist.method">
					<option value="GET">get</option>
					</select>
				</td>
			</tr>

			<tr>
				<td>uri</td>
				<td><input type="text" maxlength="1024" v-model="modifylist.uri"></td>
			</tr>

			<tr>
				<td>host</td>
				<td><input type="text" maxlength="30" v-model="modifylist.host"></td>
			</tr>

			</table>
		<p>
		<input type="button" @click="modify" value="保存">
		<input type="button" @click="changeActive" value="取消">
		</p>
		</div>
	</div>
</div>`,
	computed: {
		modifylist() {
			return this.list;
		}
	},
	methods: {
		changeActive() {
			this.$emit('change');
		},
		modify() {
			this.$emit('modify', this.modifylist);
		}
	}
});


app = new Vue({
	el: '#app',
	data: {
		isActive: false,
		selected: -1,
		selectedlist: {},
		slist: [],
		concurrent: 0,
		time: 0,
		keepalive:false,
		list: []
	},
	created() {
		this.setSlist(this.list);
	},
	methods: {
		perform_start: function ( ) {
			var json;
			json = {
				concurrent: this.concurrent,
				time: this.time,
				keepalive: this.keepalive,
				pipeline : this.list
			};
			perform_run_time = this.time;
			console.log( json );
			axios.post('/perform_start', json )
			.then(function (response) {
				loop_start( perform_run_time );
			})
			.catch(function (error) {
				alert("Please check your network...");
			});
		},

		add: function () {
			this.selectedlist = {
				index: 0,
				ip: '127.0.0.1',
				port: 9999,
				https: false,
				method: 'GET',
				uri: '/',
				host: 'localhost',
			};
			this.isActive = true;
		},

		showOverlay(index) {
			this.selected = index;
			this.selectedlist = this.list[index];
			this.changeOverlay();
		},

		modify(arr) {

			if (this.selected > -1) {
				arr.index = this.selected+1;
				Vue.set(this.list, this.selected, arr);
			} else {
				arr.index = this.list.length + 1;
				this.list.push(arr);
			}
			this.setSlist(this.list);
			this.changeOverlay();
		},

		del(index) {
			this.list.splice(index, 1);
			this.setSlist(this.list);
		},

		changeOverlay( ) {
			this.isActive = !this.isActive;
		},

		setSlist(arr) {
			this.slist = JSON.parse(JSON.stringify(arr));
		}
	},
	watch: {

	}
})
