const net = require('net')

const client = net.Socket();

const port = 8999;

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

client.connect(port, "0.0.0.0", ()=>{
	console.log("connected");
	let n = 0;
	setInterval(()=>{
		client.write('Hello '+n+' ');
		//console.log('sent');
		n++;
	}, 1000);
});
