function fetchText(delay, repeat) {
  var el = $("#console");
  if (el.textEnd == undefined) {
    el.textEnd = 0;
    el.innerHTML = "";
  }
  window.setTimeout(function() {
    ajaxJson('GET', console_url + "?start=" + el.textEnd,
      function(resp) {
        var dly = updateText(resp);
        if (repeat) fetchText(dly, repeat);
      },
      function() { retryLoad(repeat); });
  }, delay);
}

function updateText(resp) {
  var el = $("#console");
  var delay = 3000;
  if (resp != null && resp.len > 0) {
    console.log("updateText got", resp.len, "chars at", resp.start);
    if (resp.start > el.textEnd) {
      el.innerHTML = el.innerHTML.concat("\r\n<missing lines\r\n");
    }
    el.innerHTML = el.innerHTML.concat(resp.text);
    el.textEnd = resp.start + resp.len;
    delay = 500;
  }
  return delay;
}

function retryLoad(repeat) {
  fetchText(1000, repeat);
}

function showContent() {

    // hide old content
    if(current) current.style.display = 'none';

    current = getContentDiv(this);
    if(!current) return true;

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

function showTxtSettingBtns(fn) {
  txtSettings.forEach(function(f) {
    var el = $("#"+f+"-button");
    el.className = el.className.replace(" button-selected", "");
  });

  var el = $("#"+fn+"-button");
  if (el != null) el.className += " button-selected";
}

function doUpgrade()
{
	if(document.forms[0].filename.value == "")
	{
		alert(js_input_file="Select a file first!");
		return false;
	}

	var tmp = document.forms[0].filename.value;
	var filename=/\.bin$/;
	if(!filename.test(tmp))
	{
		alert(js_choose_file="Invalid filetype!");
		return false;
	}
	filename =/[^\\]*\.bin/;
	var arr=filename.exec(tmp);
	if(arr[0].length >= 64)
	{
		alert(js_bad_file="Filename too long!");
		return false;
	}
	if(!confirm(js_mksure_upgrade="Are you sure?"))
	{
		return false;
	}
	return true;
}
