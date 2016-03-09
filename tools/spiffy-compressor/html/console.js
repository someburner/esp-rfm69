function fetchText(delay, repeat) {
    var el = $.getId("console");
    if(el.textEnd==undefined) {
        el.textEnd=0;
        el.innerHTML=""
    }
    if (consrep == undefined) {
        repeat = 0;
    } else {
        repeat = consrep;
    }
   window.setTimeout(function() {
      var postData = {};
      postData['start'] = el.textEnd;
       $.ajax("/console", postData).then( function(resp) {
           var dly = updateText(resp);
           if (repeat) fetchText(dly, repeat);
       }, function(){retryLoad(repeat)} )
   }, delay);
}

function updateText(resp) {
   var el = $.getId("console");
   var delay = 3000;
   var j;
   try { j = JSON.parse(resp); }
   catch(err) {
      console.log("JSON parse error: " + err + ". In: " + resp);
//      err_cb(500, "JSON parse error: " + err);
      return;
   }
   if (j != null && j.len > 0) {
      console.log("updateText got", j.len, "chars at", j.start);
      // if (j.start > el.textEnd) {
      //    el.innerHTML = el.innerHTML.concat("\r\n<missing lines\r\n");
      // }
      el.innerHTML = el.innerHTML.concat(j.text);
      el.textEnd = j.start + j.len;
      delay = 500;
   }
   return delay;
}

function retryLoad(repeat) {
//   fetchText(1000, repeat);
}

//===== Radio info

function showRadioStatus(data) {
  Object.keys(data).forEach(function(v) {
    el = $.getId("radio-" + v);
    if (el != null) {
      if (el.nodeName === "INPUT") el.value = data[v];
      else el.innerHTML = data[v];
    }
  });
  $("#radio-spinner").hide();
}

function getRadioStatus() {
    console.log('getRadioStatus');
    $.ajax("/rfm69/status", {}).then(function(res){
        showRadioStatus(JSON.parse(res));
    }, function(res) {
      showWarning('Got an error, bro 1.');
    });
}

function showContent() {
    // hide old content
    if(current) current.style.display = 'none';

    current = getContentDiv(this);
    if (!current) return true;

    //current.innerHTML = "This is where the xml variable content should go";
    current.style.display = 'block';

    return true;
}

function getContentDiv(link) {
    var linkTo = link.getAttribute('href');
    // Make sure the link is meant to go to a div
    if(linkTo.substring(0, 2) != '#!') return;
    linkTo = linkTo.substring(2);
    return document.getElementById(linkTo);
}

function dataSpinWrapper(name, aSet, data) {
   var dat = JSON.stringify(data);
   $.ajax("console", data).then(function(res) {
      console.log('ok');
   }, function(res) {
     console.log('error');
   });
}

function simpleButtonWrapper(name, aSet, ct) {
   $("#"+aSet+"-btn").on("click", function(e) {
      e.preventDefault();
      var postData = {};
      postData[name] = aSet;
       postData['ctcnt'] = ct;
      postData['getset'] = 'get';
      dataSpinWrapper(name, aSet, postData);
   });
}

function rfmResult(delay, repeat) {
    if ((rfmrep == undefined) || (rfmrep == 0)) {
        repeat = 0;
        return;
    } else {
        repeat = rfmrep;
    }

    window.setTimeout(function() {
        $.ajax("/rfm69/update").then(function(data) {
            data = JSON.parse(data);
            Object.keys(data).forEach(function(v) {
                el = $("#rfm-" + v);
                if (el != null) {
                    if (el.nodeName === "INPUT") {
                        el.value = data[v]; }
                    else if (v.localeCompare('heap') == 0) {
                        console.log('heap!');
                        heapText.innerHTML = 'Free Heap: &nbsp;' + data[v];
                    } else {
                        //el.innerHTML = data[v];
                        el.html(data[v]);
                    }
                }
            });
            if (repeat) rfmResult(delay, repeat);
        }, function(res) {
            alert('Got an error, bro.');
        })
    }, delay);
}

function pwrSettingAndBtn(pSet, ct) {
    $.getId('pwr-btns').appendChild($.mkhtml(
    '<a id="'+pSet+'-btn" href="#" style="margin:0.5em 0.2em" class="btn btn-sm btn-d">'+pSet+'</a>'));
    simpleButtonWrapper('pwr', pSet, ct);
}

function ajaxSpin(method, url, ok_cb, err_cb) {
  $.getId("spinner").removeAttribute('hidden');
  $.ajax(url, function(resp) {
      $.getId("spinner").setAttribute('hidden', '');
      ok_cb(resp);
    }, function(status, statusText) {
      $.getId("spinner").setAttribute('hidden', '');
      //showWarning("Error: " + statusText);
      err_cb(status, statusText);
    });
}
var pwrSettings = ['power_on', 'power_off'];

var current;
var heapText;
var ct;

$(function(){
   heapText = $.getId("rfm-heap");

    ct = 0;

   var nodeid = localStorage.getItem('nodeid');  //get cached nodeid
   if (nodeid != "undefined" || nodeid != "null") {
       document.getElementById('node-id').value = nodeid;
   }


   $("#radio-refresh-button").on("click", function (e) {
      e.preventDefault();
       console.log('radio refresh click');
      getRadioStatus();
   });

   pwrSettings.forEach(function (pSet) {
      pwrSettingAndBtn(pSet, ct);
      ct += 1;
   });

   $("#send-btn").on("click", function (e) {
      e.preventDefault();
      var ackrq = $.getId('ackbox');
      var nodeid = $.getId('node-id');
      var nodeint = parseInt(nodeid.value);
      if ((nodeint > 0) && (nodeint < 255)) {
           if (localStorage) {
               localStorage.setItem('nodeid', nodeid.value);
           }
      } else {
           alert("Invalid id");
           nodeint = 0;
      }
      if (nodeint != 0) {
        txt = document.getElementById('send-txt');
        if (txt.value == '') {
            alert("Nothing to send");
        } else if (txt.value.length > 60) {
            alert("String is too long");
        } else {
          var postData = {};
          postData['to_node'] = nodeint;
          postData['data'] = txt.value;
          postData['ack'] = 0;
          if (ackrq.checked) { postData.ack = 1; }
          postData['ctcnt'] = ct;
          dataSpinWrapper('send',"rfm-data", postData);
        }
      }
      ct += 1;
   });

   $("#rfm-reset").on("click", function (e) {
      e.preventDefault();
      ajaxSpin('POST', "/rfm69/resetvals",
         function (resp) {
            co.textEnd = 0;
         },
         function (s, st) {
            showWarning("Error resetting");
         }
      );
   });

   $("#clear-button").on("click", function (e) {
      e.preventDefault();
      var co = $.getId("console");
      co.textEnd = 0;
      co.innerHTML = "";
      ajaxSpin('POST', "/console/clear",
         function (resp) {
            // showNotification("Log Cleared");
            co.textEnd = 0;
         },
         function (s, st) {
            showWarning("Error clearing log");
         }
      );
   });

   $("#output-button").on("click", function (e) {
      e.preventDefault();
      var postData = {};
      postData['disp'] = 'onoff';
      postData['ctcnt'] = ct;
      dataSpinWrapper('disp', "onoff", postData);
   });//39

});
