var currAp = "";
var blockScan = 0;

function createInputForAp(ap) {
  if (ap.ssid=="" && ap.rssi==0) return;

  var i = $.mk("input");
  i.type = "radio";
  i.name = "ssid";
  i.value=ap.ssid;
  i.id   = "opt-" + ap.ssid;
  if (currAp == ap.ssid) i.checked = "1";

  var bars    = $.mk("div");
  var rssiVal = -Math.floor(ap.rssi/51)*32;
  bars.className = "lock-icon";
  bars.style.backgroundPosition = "0px "+rssiVal+"px";

  var rssi = $.mk("div");
  rssi.innerHTML = "" + ap.rssi +"dB";

  var e = $.mk("div");
  var ev  = "-64"; //assume wpa/wpa2
  if (ap.enc == "0") ev = "0"; //open
  if (ap.enc == "1") ev = "-32"; //wep
  e.className = "lock-icon";
  e.style.backgroundPosition = "-32px "+ev+"px";

  var label = $.mk("div");
  label.innerHTML = ap.ssid;

var d1 = $.mkhtml('<label for=\"opt-'+ap.ssid+'"></label>').childNodes[0];
var b = [i,e,bars,rssi,label]
for (a in b) {d1.appendChild(b[a]); }

  var d2 = $.mk("div");
  d2.className = 'outer';
  d2.appendChild(d1);
  return d2;
}

function getSelectedEssid() {
  var e = document.forms.wifiform.elements;
  for (var i=0; i<e.length; i++) {
    if (e[i].type == "radio" && e[i].checked) return e[i].value;
  }
  return currAp;
}

var scanReqCnt = 0;

function scanResult() {
    if (wifirep == undefined) {
        clearTimeout(scanTimeout);
        return;
    } else if (wifirep == 0) {
        clearTimeout(scanTimeout);
        return;
    }

  if (scanReqCnt > 60) {
    return scanAPs();
  }

  scanReqCnt += 1;
  $.ajax("/wifi/scan", function(data) {
      data = JSON.parse(data);
      currAp = getSelectedEssid();
      if (data.ap_count > 1) {
        $("#aps").html("");
        var n = 0;
        for (var i=0; i<data.ap_count; i++) {
          var ap = data.ap[i];
          if (ap.ssid == "" && ap.rssi == 0) continue;
        var newap = createInputForAp(ap);
          $("#aps").append($(newap));
          n = n+1;
        }
        sn("Scan found " + n + " networks");
//        var cb = $("#connect-button");
//        cb.removeClass("pure-button-disabled");
        if (scanTimeout != null) clearTimeout(scanTimeout);
        scanTimeout = window.setTimeout(scanAPs, 20000);
      } else {
        window.setTimeout(scanResult, 1000);
      }
    }, function(s, st) {
      window.setTimeout(scanResult, 5000);
  });
}

function scanAPs(delay, repeat) {
    if (wifirep == undefined) {
        repeat = 0;
        clearTimeout(scanTimeout);
        return;
    } else {
        repeat = wifirep;
    }
    if (blockScan) {
        scanTimeout = window.setTimeout(scanAPs, 1000);
        return;
    }
    console.log("scanning now");

    scanTimeout = null;
    scanReqCnt = 0;
    window.setTimeout(function() {
        $.ajax("/wifi/scan").then( function(data) {
            sn("Wifi scan started");
            window.setTimeout(scanResult, 1000);
//            if (repeat) fetchText(dly, repeat);
        }, function(s, st) {
            sn("Wifi scan may have started?");
            window.setTimeout(scanResult, dly);
        });
    }, delay);
}

function getStatus() {
   $.ajax("/wifi/status", function(data) {
       data = JSON.parse(data);
      if (data.status == "idle" || data.status == "connecting") {
        $("#aps").html("Connecting...");
        sn("Connecting...");
        window.setTimeout(getStatus, 1000);
      } else if (data.status == "got IP address") {
        var txt = "Connected! Got IP "+data.ip;
        sn(txt);
        showWifiInfo(data);
        blockScan = 0;

        if (data.modechange == "yes") {
            var txt2 = "esp-rfm69 will switch to STA-only mode in a few seconds";
            window.setTimeout(function() { sn(txt2); }, 4000);
        }

        $("#reconnect").removeAttr("hidden");
        $("#reconnect").html("Go to <a href=\"http://"+data.ip+"/\">"+data.ip+"</a>");
      } else {
        blockScan = 0;
        showWarning("Connection failed: " + data.status + ", " + data.reason);
        $("#aps").html("Check password.");
      }
    }, function(s, st) {
      //showWarning("Can't get status: " + st);
      window.setTimeout(getStatus, 2000);
    });
}

function dcWiFi(e) {
   e.preventDefault();
   if(!confirm(js_mksure_upgrade="Are you sure?"))
     { return false; }
  $.ajax("/wifi/dc", function (resp) { sn("Disconnected"); });
}

function changeWifiAp(e) {
  e.preventDefault();
  var passwd = $("#wifi-passwd").get().value;
  var ssid = getSelectedEssid();
  sn("Connecting to " + ssid);
  var url = "/wifi/connect";
  hideWarning();
  $("#reconnect").setAttribute("hidden", "");
  $("#wifi-passwd").value = "";
//  var cb = $("#connect-button");
//  var cn = cb.className;
//  cb.className += '';
  blockScan = 1;

  $.ajax(url, {ssid: ssid, pwd: passwd}, function(resp) {
      $("#spinner").removeAttribute('hidden'); // hack
      sn("Waiting for network change...");
      window.scrollTo(0, 0);
      window.setTimeout(getStatus, 2000);
    }, function(s, st) {
      showWarning("Error switching network: "+st);
      cb.className = cn;
      window.setTimeout(scanAPs, 1000);
    });
}
