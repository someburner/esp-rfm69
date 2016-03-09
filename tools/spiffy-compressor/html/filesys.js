var filemanager = $('.filemanager')
var breadcrumbs = $('.breadcrumbs')
var fileList = $('.data')
var isfirst = 1;
var files = []

function loadList() {
    // Start by fetching the file data from scan route with an AJAX request
    $.ajax('/fs?file=list&action=list', function(data) {
        var response = [JSON.parse(data)]
        var currentPath = ''
        var breadcrumbsUrls = []
        var folders = []
        var files = []

        /* This event listener monitors changes on the URL. We use it to
        *  capture back/forward navigation in the browser.
        */
        window.onhashchange = function() {
            if (window.location.hash == '') { dogo('/files')}
            else { dogo(window.location.hash)}
            /* We are triggering the event. This will execute
            *  this function on page load, so that we show the correct folder:
            */
        }
        $(window.onhashchange)

        // Hiding and showing the search box
        $('div.search').on('click', function() {
            $('#s1').get('span').hide()
            $('#s1').attr('style','display:inline-block;')
        })

        /* Listening for keyboard input on the search field.
        *  We are using the "input" event which detects cut and paste
        *  in addition to keyboard input.
        */
        $('input').on('input', function(e) {
            folders = []
            files = []
            var value = this.value.trim()

            if (value.length) {
                filemanager.addClass('searching')

                // Update the hash on every key stroke
                window.location.hash = 'search=' + value.trim()
            }
            else {
                filemanager.removeClass('searching')
                window.location.hash = encodeURIComponent(currentPath)
            }
        }).on('keyup', function(e) {
            // Clicking 'ESC' button triggers focusout and cancels the search
            var search = $(this)

            if(e.keyCode == 27) {
                search.trigger('focusout')
            }

        }).on('focusout',function(e) {
            // Cancel the search
            var search = $('#s1')

            if(!this.value.trim().length) {
                window.location.hash = encodeURIComponent(currentPath)
                search.hide()
                search.parent().get('span').show()
            }
        })

        // Clicking on folders
        fileList.on('click', 'li.folders', function(e) {
            e.preventDefault()

            var nextDir = $(this).get('a.folders').attr('href')

            if(filemanager.hasClass('searching')) {
                // Building the breadcrumbs
                breadcrumbsUrls = generateBreadcrumbs(nextDir)

                filemanager.removeClass('searching')
                $('input[type=search]').value = ''
                $('input[type=search]').hide()
                filemanager.get('span').show()
            }
            else {
                breadcrumbsUrls.push(nextDir)
            }

            window.location.hash = encodeURIComponent(nextDir)
            currentPath = nextDir
        })


        // Clicking on breadcrumbs
        breadcrumbs.on('click', 'a', function(e){
            e.preventDefault()

            var index = breadcrumbs.get('a').index($(this))
            var nextDir = breadcrumbsUrls[index]

            breadcrumbsUrls.length = Number(index)

            window.location.hash = encodeURIComponent(nextDir)
        })


        // Navigates to the given hash (path)
        function dogo(hash) {
            hash = decodeURIComponent(hash).slice(1).split('=')

            if (hash.length) {
                var rendered = ''

                // if hash has search in it
                if (hash[0] === 'search') {
                    filemanager.addClass('searching')
                    rendered = searchData(response, hash[1].toLowerCase())

                    if (rendered.length) {
                        currentPath = hash[0]
                        render(rendered)
                    }
                    else {
                        render(rendered)
                    }
                }

                // if hash is some path
                else if (hash[0].trim().length) {
                    rendered = searchByPath(hash[0])

                    if (rendered.length) {
                        currentPath = hash[0]
                        breadcrumbsUrls = generateBreadcrumbs(hash[0])
                        render(rendered)
                    }
                    else {
                        currentPath = hash[0]
                        breadcrumbsUrls = generateBreadcrumbs(hash[0])
                        render(rendered)
                    }
                }

                // if there is no hash
                else {
                    currentPath = data.path
                    breadcrumbsUrls.push(data.path)
                    render(searchByPath(data.path))
                }
            }
        }

        // Splits a file path and turns it into clickable breadcrumbs
        function generateBreadcrumbs(nextDir){
            var path = nextDir.split('/').slice(0)
            for (var i=1; i<path.length; i++){
                path[i] = path[i-1]+ '/' +path[i]
            }
            return path
        }

        // Locates a file by path
        function searchByPath(dir) {
            var path = dir.split('/')
            var demo = response
            var flag = 0

            for (var i=0; i<path.length; i++) {
                for (var j=0; j<demo.length; j++) {
                    if (demo[j].name === path[i]) {
                        flag = 1
                        demo = demo[j].items
                        break
                    }
                }
            }

            demo = flag ? demo : []
            return demo
        }


        // Recursively search through the file tree
        function searchData(data, searchTerms) {
            data.forEach(function(d) {
                if (d.type === 'folder') {
                    searchData(d.items,searchTerms)

                    if (d.name.toLowerCase().match(searchTerms)) {
                        folders.push(d)
                    }
                }
                else if (d.type === 'file') {
                    if (d.name.toLowerCase().match(searchTerms)) {
                        files.push(d)
                    }
                }
            })
            return {folders: folders, files: files}
        }


        // Render the HTML for the file manager
        function render(data) {
            console.log('render data =')
            console.log(data)
            var scannedFolders = []
            var scannedFiles = []


            if (Array.isArray(data)) {
                data.forEach(function(d) {
                    if (d.type === 'folder') {
//						console.log('folder found');
                        scannedFolders.push(d)
                    }
                    else if (d.type === 'file') {
//						console.log('file found');
                        scannedFiles.push(d)
                    }
                })
            }
            else if (typeof data === 'object') {
                scannedFolders = data.folders
                scannedFiles = data.files
            }


            // Empty the old result and make the new one
            if (!isfirst)
            {

            } else {
                isfirst = 0;
            }


            if (!scannedFolders.length && !scannedFiles.length) {
                filemanager.get('.nothingfound').show()
            }
            else {
                $('div.nothingfound').hide()
            }

            if (scannedFolders.length) {
                scannedFolders.forEach(function(f) {
                    var itemsLength = f.items.length
                    var name = escapeHTML(f.name)
                    var icon = '<span class="icon folder"></span>'

                    if (itemsLength) {
                        icon = '<span class="icon folder full"></span>'
                    }
                    if (itemsLength == 1) {
                        itemsLength += ' item'
                    }
                    else if (itemsLength > 1) {
                        itemsLength += ' items'
                    }
                    else {
                        itemsLength = 'Empty'
                    }

                    var test = '<a href="'+ f.path +'" title="'+ f.path +'" class="folders">'+icon+'<span class="name">' + name + '</span> <span class="details">' + itemsLength + '</span></a>'

                    var ftest = document.createElement('li')
                    ftest.className = 'folders';

                    ftest.innerHTML = test;
                    fileList.append($(ftest))
                })
            }

            if (scannedFiles.length) {
                scannedFiles.forEach(function(f) {
                    var fileSize = bytesToSize(f.size)
                    var name = escapeHTML(f.name)
                    var fileType = name.split('.')
                    var icon = '<span class="icon file"></span>'

                    fileType = fileType.length > 1 ? fileType[fileType.length-1] : ''

                    icon = '<span class="icon file f-' + fileType + '">' + fileType + '</span>'

                    var fhtml = ''
                            +'<a href="#files" style="width:80%;" title="'+f.path+'" '
                            +'class="files">'+icon
                            +'<span class="name">'+name+'</span>'
                            +'<span class="details">'+fileSize+'</span>'
                            +'</a>'
                            +'<a class="btn btn-c btn-sm smooth" onclick=doDel("'+f.path+'");><i class="ico">☒</i></a>'
                            +'<a class="btn btn-a btn-sm smooth" onclick=doDl("'+f.path+'");><i class="ico">⇩</i></a>'

                    var fnew = document.createElement('li')
                    fnew.className = 'files';

                    fnew.innerHTML = fhtml;
                    fileList.append($(fnew))
                })
            }

            // Generate the breadcrumbs
            var url = ''

            if (filemanager.hasClass('searching')) {
                url = '<span>Search results: </span>'
                fileList.removeClass('animated')
            }
            else {
                fileList.addClass('animated')
                breadcrumbsUrls.forEach(function (u, i) {
                    var name = u.split('/')

                    if (i !== breadcrumbsUrls.length - 1) {
                        url += '<a href="'+u+'"><span class="folderName">' + name[name.length-1] + '</span></a> <span class="arrow">→</span> '
                    }
                    else {
                        url += '<span class="folderName">' + name[name.length-1] + '</span>'
                    }
                })
            }
            breadcrumbs.text('').html(url)
            // Show the generated elements
            fileList.show()
        }

        // This function escapes special html characters in names
        function escapeHTML(text) {
            return text.replace(/\&/g,'&amp;').replace(/\</g,'&lt;').replace(/\>/g,'&gt;')
        }

        // Convert file sizes from bytes to human readable units
        function bytesToSize(bytes) {
            var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB']
            if (bytes == 0) return '0 Bytes'
            var i = parseInt(Math.floor(Math.log(bytes) / Math.log(1024)))
            return Math.round(bytes / Math.pow(1024, i), 2) + ' ' + sizes[i]
        }
    })
}

$(function(){
    loadList();
})

function reloadFSList() {
    $('li.files').remove();
    loadList();
}

var dlfile = function downloadURL(url) {
    var hiddenIFrameID = 'hiddenDownloader',
    iframe = document.getElementById(hiddenIFrameID);
    if (iframe === null) {
        iframe = document.createElement('iframe');
        iframe.id = hiddenIFrameID;
        iframe.style.display = 'none';
        document.body.appendChild(iframe);
    }
    iframe.src = url;
}

function doDl(fn) {
    var f = fn.split('/');
    dlfile('/fs?file='+f[1]+'&action=dl');
}

function doDel(fn) {
    var f = fn.split('/');
    var r = confirm('Are you sure you want to delete '+f[1]+'?');
    if (r == true) {
        $.ajax('/fs?file='+f[1]+'&action=del').done( function() {
                $('a[title*="'+f[1]+'"]').remove();
                sn(f[1]+' deleted from FS')
                window.setTimeout(reloadFSList, 1000);
       });
    } else {
        console.log('delete abort');
    }
}
