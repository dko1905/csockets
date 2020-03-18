const net = require('net')

const client = net.Socket();

const port = 8999;

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

client.connect(port, "0.0.0.0", ()=>{
	console.log("connected");
});
client.on('data', (chunk)=>{
	console.log(chunk.toString("utf8"));	
});
