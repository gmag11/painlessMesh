var ws, socketStr, tempIndex, lastPing, timeOut;

timeOut = 5000; //milliseconds

socketStr = window.location.host;
tempIndex = socketStr.indexOf(":");
socketStr = "ws://" + socketStr + ":2222/";

var control = new Object();

var outPackage = 0;

function decimel2HexStr(dec) {
    'use strict';
    var ret = Math.round(dec * 255).toString(16);

    while (ret.length < 2) { ret = "0" + ret; }
    
    return ret;
}

function hslHue2Rgb(h) { // assumes full satruation and lightness
    'use strict';
    var r, g, b, x, ret;
    
    r = g = b = 0;
    h =  h * 360;
    x = (h % 60) / 60;
    
    if (h <= 60) { r = 1; g = x; }
    if (60 < h && h <= 120) { r = 1 - x; g = 1; }
    if (120 < h && h <= 180) { g = 1; b = x; }
    if (180 < h && h <= 240) { g = 1 - x; b = 1; }
    if (240 < h && h <= 300) { r = x; b = 1; }
    if (300 < h && h < 360) { r = 1; b = 1 - x; }
    if (h === 360) { r = 1; }
    
    ret = "#" + decimel2HexStr(r) + decimel2HexStr(g) + decimel2HexStr(b);
    return ret;
}


function onOpenFunction(event) {
    'use strict';
    document.getElementById("hero").innerHTML = "WebSocket Open";
    outPackage = "wsOpened";
}

function onErrorFunction(event) {
    'use strict';
    var meshMsg = document.getElementById("meshMsg");
    meshMsg.innerHTML = "onErrorFunction<br>" + meshMsg.innerHTML;
}



function sliderChange(sliderNum) {
    'use strict';
    var newValue, thumb, width;
    
    newValue = document.getElementById("slider" + sliderNum).value;
    document.getElementById("footer").innerHTML = newValue.toString();
    
    control[sliderNum] = newValue;
        
    thumb = document.getElementById("sliderThumb" + sliderNum);
    width = document.getElementById("sliderContainer" + sliderNum).offsetWidth;
    width -= thumb.offsetWidth;
        
    thumb.style.left = width * newValue + 2;  // 2px is for boarder width
    thumb.style.backgroundColor = hslHue2Rgb(newValue);
    
    newValue = JSON.stringify(control);
    document.getElementById("header").innerHTML = newValue.toString();
    
    outPackage = newValue;
}


function populateSliders(inControl) {
    'use strict';
    var sliderDiv, width, i, inputDiv, top, track, container, thumb, newSlider, sliderContainer, sliderTrack, sliderThumb, sliderRange;
    
    document.getElementById("hero").innerHTML = ""; // remove connectButton
    
    sliderDiv = document.getElementById("sliderDiv");
    sliderDiv.innerHTML = "";
    
    top = Object.keys(inControl).length;
    for (i = 0; i < top; i += 1) {  // dynamically place sliders
        inputDiv =  "<div class='sliderContainer' id='sliderContainer" + i + "'>";
        inputDiv += "<img class='sliderTrack' src='hsl.png' id='sliderTrack" + i + "'>";
        inputDiv += "<div class='sliderThumb' id='sliderThumb" + i + "'></div>";
        inputDiv += "<input class='sliderRange' type='range' id='slider" + i + "' ";
        inputDiv += "oninput='sliderChange(" + i + ");' min='0' max='1' step='0.001' ";
        inputDiv += "value='" + inControl[i.toString()] + "'></div>";
             
        sliderDiv.innerHTML += inputDiv;
        
        
/*        sliderContainer = document.createElement("div");
        sliderContainer.className = "sliderContainer";
        sliderContainer.id = "sliderContainer" + i;

        sliderTrack = document.createElement("img");
        sliderTrack.className = "sliderTrack";
        sliderTrack.id = "sliderTrack" + i;
        sliderTrack.src = "hsl.png";

        sliderThumb = document.createElement("div");
        sliderThumb.className = "sliderThumb";
        sliderThumb.id = "sliderThumb" + i;

        sliderRange = document.createElement("input");
        sliderRange.className = "sliderRange";
        sliderRange.id = "slider" + i;
        sliderRange.oninput = function () { sliderChange( i ); };
        sliderRange.min = 0;
        sliderRange.max = 1;
        sliderRange.step = 0.001;
        
        sliderContainer.appendChild( sliderTrack );
        sliderContainer.appendChild( sliderThumb );
        sliderContainer.appendChild( sliderRange );
        
        sliderDiv.appendChild( sliderContainer );
*/
    }
    
    track = document.getElementsByClassName("sliderTrack");
    container = document.getElementsByClassName("sliderContainer");
    thumb = document.getElementsByClassName("sliderThumb");
    
    for (i = 0; i < track.length; i += 1) {
        container[i].style.height = thumb[i].offsetHeight;
        track[i].width = container[i].offsetWidth - thumb[i].offsetWidth;
        
        track[i].style.left = (thumb[i].offsetWidth / 2);
        track[i].style.top = (thumb[i].offsetHeight - track[i].offsetHeight) / 2;

        track[i].style.top = (thumb[i].offsetHeight - track[i].offsetHeight) / 2;

        sliderChange(i);
    }
}

function onMessageFunction(event) {
    'use strict';
    var inControl, meshMsg;
    // add debug data
    meshMsg = document.getElementById("meshMsg");
    meshMsg.innerHTML = event.data + "<br>" + meshMsg.innerHTML;

    lastPing = Date.now();

    if ( event.data === "ping") {
        return;
    }
    
    inControl = JSON.parse(event.data);
    if (inControl.hasOwnProperty("0")) {
        // control msg
        populateSliders(inControl);
    }
}

function sendData() {
    'use strict';
    if (outPackage !== 0) {
        ws.send(outPackage);
        outPackage = 0;
    }
}

function keepAlive() {
    'use strict';
    if ( typeof ws !== 'undefined' ) {
        if ( ws.readyState === 1 && document.getElementsByClassName("sliderDiv").length > 0 ) {  //OPEN
            sliderChange(0);
        }
    }
}

function checkAlive() {
    if ( (lastPing + timeOut) < Date.now() ){
        if ( typeof ws !== 'undefined' ) {
            if ( ws.readyState === 1 ) {  //OPEN
                var meshMsg = document.getElementById("meshMsg");
                meshMsg.innerHTML = "Timeout<br>" + meshMsg.innerHTML;
                ws.close();
                addConnectToButton();
            }    
        }
    }
}

function startWebSocket() {
    //ws = new WebSocket(socketStr);
    //ws = new WebSocket('ws://192.168.107.1:2222/');
    //ws = new WebSocket('ws://192.168.10.1:2222/');
    ws = new WebSocket('ws://192.168.167.1:2222/');

    ws.onmessage = function (event) {onMessageFunction(event); };
    ws.onopen = function (event) {onOpenFunction(event); };
    ws.onclose = function () {addConnectToButton(); };
    ws.onerror = function (event) {onErrorFunction(event); };
 
    document.getElementById("buttonText").innerHTML = '<div class="loading bar"><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div>';    
    
    lastPing = Date.now();
}

function addConnectToButton() {
    var button, buttonText;
    
    document.getElementById("sliderDiv").innerHTML = ""; // remove any sliders that might be hanging out
    document.getElementById("hero").innerHTML = ""; // clean up.
    
    button = document.createElement("div");
    button.className = "connectButton";
    button.id = "connectButton";
    button.onclick = function () { startWebSocket() };    
    
    buttonText = document.createElement("div");
    buttonText.className = "buttonText";
    buttonText.id = "buttonText";
    buttonText.innerHTML = "Connect<br>to<br>Mesh";

    button.appendChild( buttonText );
    document.getElementById("hero").appendChild( button );
}

setInterval(sendData, 200);
setInterval(keepAlive, 5000);
setInterval(checkAlive, 600);

//setInterval( addConnectToButton, 100 )

window.onload = addConnectToButton;

