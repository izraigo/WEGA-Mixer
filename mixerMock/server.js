const http = require('http');
var fs = require('fs');
var path = require('path');

let clients = [];

const requestListener = function (req, res) {
  if (req.url.startsWith("/rest/start")) {sendFromFile("web/start.html", res); return;}
  switch (req.url) {
    case "/rest/meta":   sendMeta(res); return;
    case "/rest/start":  handleStart(res); return;
    case "/rest/tare":   sendFromFile("web/tare.html" , res); return;    
    case "/rest/events": subscriptionHandler(req , res); return;    
    case "/":            sendFromFile("web/index.html", res); return;
    case "/calibration": sendFromFile("web/calibration.html", res); return;
    case "/style.css":   sendFromFile("web/style.css" , res); return;
  }
 }

function subscriptionHandler(req, res) {
  const clientId = Date.now();
  const newClient = {
    id: clientId,
    response:res
  };
  clients.push(newClient);

  const headers = {
    'Content-Type': 'text/event-stream',
    'Connection': 'keep-alive',
    'Cache-Control': 'no-cache'
  };
  res.writeHead(200, headers);

  res.write(`event:state\ndata: ${JSON.stringify({"state": "Ready"})}\n\n`);
  res.write(`event:state\ndata: ${JSON.stringify({"state": "Working"})}\n\n`);
  let report = {
    "timer": 1840,
    "sumA": 550.000,
    "sumB": 556.300,
    "pumpWorking": 6,
    "goal": [100.000,100.000,100.000,100.000,100.000,100.000,100.000,100.000],
    "result": [100.000,100.006,100.000,100.002,100.000,100.001,0.0,0.0]
    };
    


  res.write(`event:report\ndata: ${JSON.stringify(report)}\n\n`);
  res.write(`event:state\ndata: ${JSON.stringify({"state": "Ready"})}\n\n`);
  res.write(`event:scales\ndata: ${JSON.stringify({"value": 14.246})}\n\n`);

  req.on('close', () => {
    console.log(`${clientId} Connection closed`);
    clients = clients.filter(client => client.id !== clientId);
  });

}

function sendFromFile(filePath, res) {
  fs.readFile(filePath, function(error, content) {
    var mimeTypes = {
      '.html': 'text/html',
      '.js': 'text/javascript',
      '.css': 'text/css',
      '.json': 'application/json',
      '.png': 'image/png',
      '.jpg': 'image/jpg',
      '.gif': 'image/gif',
      '.svg': 'image/svg+xml',
      '.wav': 'audio/wav',
      '.mp4': 'video/mp4',
      '.woff': 'application/font-woff',
      '.ttf': 'application/font-ttf',
      '.eot': 'application/vnd.ms-fontobject',
      '.otf': 'application/font-otf',
      '.wasm': 'application/wasm'
    };
    var extname = String(path.extname(filePath)).toLowerCase();
    var contentType = mimeTypes[extname] || 'application/octet-stream';
    res.writeHead(200, { 'Content-Type': contentType });
    res.end(content, 'utf-8');
  });
}
const metaJson=JSON.stringify({
  "version": "2.0.1 igor",
  "scalePointA": 1988.222,
  "scalePointB": 2020.444,
  "names": ["1:Ca", "2:NK", "3:NH", "4:Mg", "5:KP", "6:KS", "7:MK", "8:B"]
  });

function sendMeta(res) {
  res.setHeader("Content-Type", "application/json");
  res.writeHead(200);
  res.end(metaJson);
}

const server = http.createServer(requestListener);
server.listen(8080);

function sendEventsToAll(name, newFact) {
  clients.forEach(client => client.response.write(`event:${name}\ndata: ${JSON.stringify(newFact)}\n\n`))
}

function handleStart(res) {
  sendFromFile("web/start.html", res);
  sendEventsToAll("state", {"state":"Igor"});
  start();
  start();
  start();
  start();
  start();
  start();
  start();
}

function start() {
  sendEventsToAll("state", {"state":"Working"});
//  yield true;

  let report = {
    "timer": 1840,
    "sumA": 550.000,
    "sumB": 556.300,
    "pumpWorking": 0,
    "goal": [100.000,100.000,100.000,100.000,100.000,100.000,100.000,100.000],
    "result": [0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0]
    };

  for(i = 1; i <= 8; i++) {
    pumpWorking = i;
    report.result[i] = report.goal[i] + Math.random() - 0.5;
    sendEventsToAll("report", report);
//    yield true;
  }

  sendEventsToAll("state", {"state":"Ready"});
//  yield false;
}