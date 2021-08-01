const char CSS_page[] PROGMEM = R"=====(
.button { width:105px; height:26px; font-size:0.8em; }
)=====";

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<link rel='stylesheet' type='text/css' href='style.css'>
Status: <span id='state'></span><br>
<br>Sum A = <span id='sumA'></span>
<br>Sum B = <span id='sumB'></span>
<br>Timer = <span id='timer'></span>
<form action="api/start">
    <p>P1 = <input type='text' name='p1'/> <span id='n1'></span><span id='r1'/></p>
    <p>P2 = <input type='text' name='p2'/> <span id='n2'></span><span id='r2'/></p>
    <p>P3 = <input type='text' name='p3'/> <span id='n3'></span><span id='r3'/></p>    
    <p>P4 = <input type='text' name='p4'/> <span id='n4'></span><span id='r4'/></p>
    <p>P5 = <input type='text' name='p5'/> <span id='n5'></span><span id='r5'/></p>    
    <p>P6 = <input type='text' name='p6'/> <span id='n6'></span><span id='r6'/></p>
    <p>P7 = <input type='text' name='p7'/> <span id='n7'></span><span id='r7'/></p>    
    <p>P8 = <input type='text' name='p8'/> <span id='n8'></span><span id='r8'/></p>
    <p id='actions' hidden='true'>
        <input type='submit' value='Start'/>
        <input type='button' onclick='location.href = "scales";' value='Scales'/>
        <input type='button' onclick='location.href = "calibration";' value='Calibration'/>
    </p>
</form>
ver : <span id='version'>unknown</span>
<script>
function loadMeta() {
    fetch('/api/meta').then(r => r.json()).then(r => {
        document.getElementById('version').textContent = r.version;
        for (let i = 1; i <= 8; i++) {
            document.getElementById('n' + i).textContent = r.names[i - 1];
        }
    })
    .catch(e => setTimeout(loadMeta, 5000));
}

function takeFromUrl() {
    let completed = false;
    let params = new URLSearchParams(location.search);
    for (let i = 1; i <= 8; i++) {
        let plan = params.get("p" + i);
        if (plan) {
            document.getElementsByName('p' + i)[0].value = parseFloat(plan).toFixed(2);
            completed = true;
        }
    }
    return completed;
}

function loadStatus() {
    fetch('/api/status').then(r => r.json()).then(r => {
        document.getElementById('state').textContent = r.state;
        let isReady = r.state == "Ready";
        fromUrl &= isReady; 
        if (!fromUrl) {
            document.getElementById('sumA').textContent = r.sumA.toFixed(2);
            document.getElementById('sumB').textContent = r.sumB.toFixed(2);
            document.getElementById('timer').textContent = r.timer;
            for (let i = 1; i <= 8; i++) {
                let plannedVol = r.planned[i - 1];
                document.getElementsByName('p' + i)[0].value = plannedVol.toFixed(2);
                let vol = r.pumped[i - 1];
                let e = document.getElementById('r' + i);
                e.textContent = vol ? '=' + vol.toFixed(2) + ' ' + (vol / plannedVol * 100 - 100).toFixed(2) + '%' : '';
                e.style.fontWeight = r.pumpWorking == i ? 'bold' : 'normal'; 
            }
        }
        if (isReady) {
            document.getElementById('actions').hidden = false;
        } else {
            document.getElementById('actions').hidden = true;                        
            setTimeout(loadStatus, 5000);
        }
    })
    .catch(e => setTimeout(loadStatus, 5000));
}

loadMeta();
fromUrl = takeFromUrl();
loadStatus();
</script>   
)=====";

const char SCALES_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<link rel='stylesheet' type='text/css' href='style.css'/>
Status: <span id="state"></span><br>
<br>Value = <span id="value"></span>g
<br>Raw = <span id="rawValue"></span>
<p>
    <input type='button' onclick="fetch('/api/tare');" value='Set to ZERO'/>
    <input type='button' onclick="location.href = '/';" value='Home'/>
</p>
ver : <span id="version">unknown</span>
<script>
function loadMeta() {
    fetch("/api/meta").then(r => r.json()).then(r => {
        document.getElementById("version").textContent = r.version;
    })
    .catch(e => setTimeout(loadMeta, 1000));
}

function loadValue() {
    fetch("/api/scales").then(r => r.json()).then(r => {
        document.getElementById("state").textContent = r.state;
        document.getElementById("value").textContent = r.value.toFixed(2);
        document.getElementById("rawValue").textContent = r.rawValue;
        setTimeout(loadValue, 1000);
    })
    .catch(e => setTimeout(loadValue, 1000));
}

loadMeta();
loadValue();
</script>
)=====";

const char CALIBRATION_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<link rel='stylesheet' type='text/css' href='style.css'>
Current scale_calibration_A=<span id="curA"></span>, scale_calibration_B=<span id="curB"></span>
<ol>
    <li>Unload scales. And press <input type='button' onclick="fetch('/api/tare');" value='Set to ZERO'/></li>
    <li>Take any object of known weight. Enter its weight here <input type='text' id='knownValue'/></li>
    <li>Place the weight on point A and press <input type='button' onclick="calc('A');" value="Ok"/> <b id='A'></b></li>
    <li>Place the weight on point B and press <input type='button' onclick="calc('B');"value="Ok"/> <b id='B'></b></li>
    <li>Open <b>mixer.ino</b> and set values for the constants</li>
</ol>
Test:
<ol>
    <li>Enter scale <input type='text' id='testScale'/> and press <input type='button' onclick="test();" value='Test'/></li>
    <li>Measured weight is <span id='testVal'>_</span></li>
</ol>
<p><input type='button' onclick="location.href = '/';" value='Home'/></p>
ver : <span id='version'>unknown</span>    
<script>
function loadMeta() {
    fetch("/api/meta").then(r => r.json()).then(r => {
        document.getElementById("version").textContent = r.version;
        document.getElementById("curA").textContent = r.scalePointA;        
        document.getElementById("curB").textContent = r.scalePointB;        
    })
    .catch(e => setTimeout(loadMeta, 1000));
}

function calc(point) {
    fetch("/api/scales").then(r => r.json()).then(r => {
        let knownValue = document.getElementById('knownValue').value;
        let result = (r.rawValue - r.rawZero) / knownValue;
        document.getElementById(point).textContent = ' scale_calibration_' + point + ' = ' + result;
    })
    .catch(e => alert(e));
}

function test(point) {
    fetch("/api/scales").then(r => r.json()).then(r => {
        let scale = document.getElementById('testScale').value;
        document.getElementById("testVal").textContent = (r.rawValue - r.rawZero) / scale;
    })
    .catch(e => alert(e));
}

loadMeta();
</script>
)=====";

const char OK_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<meta http-equiv="refresh" content="1;url=/">
Ok
)=====";


void cssPage(){
  server.send(200, "text/css", CSS_page);
}
void scalesPage(){
  server.send(200, "text/html", SCALES_page);
}
void calibrationPage(){
  server.send(200, "text/html", CALIBRATION_page);
}
void mainPage(){
  server.send(200, "text/html", MAIN_page);
}
void okPage(){
  server.send(200, "text/html", OK_page);
}
