
var nid = 0;
$("#fwul-span").hide();
$("#fwid-span").hide();

function getSize(aname) {
    var sz = 0;
    fwlist.forEach(function(el, i) {
        if (el == aname)
        {
            sz = fwszlist[i];
        }
    });
    return sz;
}

function fwConfirm() {
    if (nid > 0)
    {
        var fname = $('#fwlabel').text();
        var r = confirm('Update Node #'+nid+' with '+fname+'?');
        if (r == true) {
            $.ajax('http://10.42.0.81/fs?file='+fname+'&action=fw&nid='+nid).done( function() {
                console.log('ok!');
            });
        } else {
            console.log('confirm abort');
        }
    }
    return false;
}

function makefwhtml() {
  fwlist.forEach(function(el, index) {
    var mkli = $.mk("li");
    $(mkli).attr('id', 'fwfno-'+index);
    var mka = $.mk("a");
    var mklb = $.mk("label");

    $(mklb).text('size: ' + getSize(el));
    $(mklb).addClass('sellabel');

    $(mka).attr('href', '#');
    $(mka).text(el);

    $(mkli).append($(mka));
    $(mkli).append($(mklb));
    $("#fw-list").append($(mkli));
  });


  $('li[id|=fwfno').each(function (elem, i) {
      $(elem).on('click', function() {
          $('li[id|=fwfno').each(function (el2, i2) {
              $(el2).attr('class', '');
          });
          $(this).addClass('fwsel');
          $('#fwlabel').text(fwlist[i]);
          $("#fwid-span").show();
          $("#fwul-span").show();
      });
  });


  $('#fw-idtxt').on('keyup', function(e) {
      e.preventDefault();
      if (!(this.value.match(/^[0-9]{0,3}$/))) {
         this.value = '';
         $('#fwul-btn').attr('disabled','');
         nid = 0;
         return;
      } else {
          var outVal = parseInt(this.value, 10);
          if ((outVal>=1) && (outVal<=255)) {
              nid = outVal;
              $('#fwul-btn').removeAttr('disabled');
          } else {
              this.value = '';
              nid = 0;
              $('#fwul-btn').attr('disabled','');
              return;
          }
      }
  });
}

function loadfwList() {
    $.ajax('/fs?file=list&action=list', function(data) {
        var resp = JSON.parse(data)
        resp.items.forEach(function(elm, ind) {
            var nm = elm.name.split('.')[1];
            if (nm == 'bin')
            {
               fwlist.push(elm.name);
               fwszlist.push(elm.size);
            }
        });
    }).done(function () {
   });
}

$(function(){
   fwlist = [];
   fwszlist = [];
   loadfwList();
   window.setTimeout(makefwhtml,250);
})
