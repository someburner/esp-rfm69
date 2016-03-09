
/**
 * FileInfo
 * @param id: id of file to upload
 * @param file: file object
 * @param pagesize: size of chunk to send
 * @returns {FileUpload}
 */
function FileInfo(id, file, pagesize) {
    this.size = file.size;
    this.file = file;
    this.FileType = file.type;
    this.fileName = file.name;
    this.pageSize = pagesize;
    this.pageIndex = 0;
    this.pages = 0;
    this.UploadError = null;
    this.dataBuffer = null;
    this.uploadBytes = 0;
    this.id = id;
    if (Math.floor(this.size % this.pageSize) > 0) {
        this.pages = Math.floor((this.size / this.pageSize)) + 1;
    } else {
        this.pages = Math.floor(this.size / this.pageSize);
    }
}

function doReList()
{
    window.setTimeout(reloadFSList, 1000);
}

/**
 * The FDSock object
 * @constructor
 * @param {DOMElement} uploadBtn The upload Button node
 * @param {String} wsUrl The websocket url
 */
function FDSock(uploadBtn, wsUrl){

	this.uploadBtn = uploadBtn;
	this.wsUrl = wsUrl;
	this.ready = false; //changes once ready
	this.ws = null;
	this.fileReader = null;
	this.uploading = false;
	this.uploaded = 0;//# of chunks uploaded

	if(typeof FDSock.initialized == "undefined"){

		FDSock.prototype.init = function(){
			if(this.ready) return;

            // on complete DOM wrapper
			var wrapper = document.createElement('div');
			var template = '<div id="dd_uploader_wrapper" class="dropzone">'+
					'<div id="dd_uploader_drap_drop_area" class="dropzone dz-drag-hover">' +
                    'Drop files here to upload' +
                    '<a id="dd_uploader_close" class="btn btn-c">X</a></div>'+
					'<div id="dd_uploader_file_list" class="dropzone dz-clickable dz-started"></div>'+
				'</div>';
			wrapper.innerHTML = template;
			this.uploadBtn.parentNode.insertBefore(wrapper, this.uploadBtn);

			this.initEventListeners();

			if ('WebSocket' in window)
		        this.ws = new WebSocket(this.wsUrl);
		    else if ('MozWebSocket' in window)
		    	this.ws = new MozWebSocket(this.wsUrl);
		    else
		        return;

			var that = this;
			//websocket init
			if(this.ws != null){
				this.ws.onopen = function(evt) {
					console.log("Openened connection to websocket");

					//Register close button
					document.getElementById('dd_uploader_close').addEventListener('click', function(){that.fileReader.abort();that.ws.send("fsStreamAbort"); }, false);
				};
				this.ws.onclose = function(evt) {
					console.log("Closed connection to websocket");
					// free
					this.ws = null;
					this.fileReader = null;
					this.uploading = false;
					wrapper.parentNode.removeChild(wrapper);
				};
			}

			this.ready = true;//标记初始化完成
		};

		/**
		 * Inits most the the event listeners
		 */
		FDSock.prototype.initEventListeners = function(){
			var that = this;
			// drag / drop event listeners
			document.getElementById('dd_uploader_drap_drop_area').addEventListener('dragover', that.handleDragOver, false);
			document.getElementById('dd_uploader_drap_drop_area').addEventListener('dragenter', that.handleDragEnter, false);
			document.getElementById('dd_uploader_drap_drop_area').addEventListener('dragleave', that.handleDragLeave, false);
	  		document.getElementById('dd_uploader_drap_drop_area').addEventListener('drop',function(evt){that.handleDrop(evt); }, false);
		};

		FDSock.prototype.handleDragOver = function(evt){
			evt.stopPropagation();
			evt.preventDefault();
			evt.dataTransfer.dropEffect = 'copy'; // Explicitly show this is a copy.
		};
		FDSock.prototype.handleDragEnter = function(evt){
			evt.stopPropagation();
			evt.preventDefault();
			//TODO dd_uploader_drap_drop_area
		};
		FDSock.prototype.handleDragLeave = function(evt){
			evt.stopPropagation();
			evt.preventDefault();
			//TODO dd_uploader_drap_drop_area
		};
		FDSock.prototype.handleDrop = function(evt){
			if(this.uploading) return;

			evt.stopPropagation();
		    evt.preventDefault();
		    //TODO dd_uploader_drap_drop_area

		    var files = evt.dataTransfer.files; // FileList object.
		    if (files.length > 0) {
		    	// only allow one file
                    var info = new FileInfo(this.uploaded, files[0], 256); //256 chunk size
                    this.addUploadItem(info);
            }
		};

		/**
		 * addUploadItem: get info from file
		 * @param info
		 */
		FDSock.prototype.addUploadItem = function(info) {
			var list = document.getElementById('dd_uploader_file_list');
			var item = '<div id="file_'+info.id+'">'+info.fileName+'<div class="upl_prog" id="progress_'+info.id+'">0%</div></div>';
			list.innerHTML += item;
			this.uploading = true;
			this.upload(info);//begin upload
        };

		/**
		 * Upload file
		 * @param ws websocket
		 */
		FDSock.prototype.upload = function (info) {
			var that = this;
			this.ws.send("fsStreamStart"+info.fileName+"fsStreamSize"+info.size);
			// if we get the OK, begin sending
			this.ws.onmessage = function(msg) {
                if(msg.data == "ok") {
					that.onLoadData(info);
				}
			};
		};

		/**
		 * onLoadData
		 * @param info
		 */
		FDSock.prototype.onLoadData = function(info) {
			if (this.fileReader == null)
				this.fileReader = new FileReader();

			var reader = this.fileReader;
			reader["tag"] = info;
			reader["this"] = this;
			reader.onloadend = this.onLoadDataCallBack;
			var count = info.size - info.pageIndex * info.pageSize;
			if (count > info.pageSize)
				count = info.pageSize;
			info.uploadBytes += count;
            /** Slice blob     start = (        index * 256       )    end =    index * (         256 + n  )    **/
			var blob = info.file.slice(info.pageIndex * info.pageSize, info.pageIndex * info.pageSize + count);

			reader.readAsArrayBuffer(blob);
		};
		/**
		 * onLoadDataCallBack: Sends WS frame
		 * @param evt
		 */
		FDSock.prototype.onLoadDataCallBack = function(evt) {
			var obj = evt.target["tag"];
			var that = evt.target["this"];
			if (evt.target.readyState == FileReader.DONE) {
				obj.dataBuffer = evt.target.result;
				that.ws.send(obj.dataBuffer);
				//
				that.ws.onmessage = function(msg) {
					console.log(msg.data);
					obj.pageIndex++;
					evt.target["tag"] = obj;
					that.updateProgress(obj);
					if (obj.pageIndex < obj.pages) {
						that.onLoadData(obj);
					}else{
						that.uploaded++;
						that.ws.send("fsStreamDone");
						that.uploading = false;
                  sn('Upload Complete!')
                  doReList();
					}
				};
			}
		};

		/**
		 * updateProgress
		 * @param file
		 */
		FDSock.prototype.updateProgress = function(file) {
			var percentLoaded = parseInt((file.pageIndex / file.pages) * 100);
			var progress = document.getElementById('progress_' + file.id);
			if (0 < percentLoaded < 100) {
				progress.style.width = percentLoaded + '%';
				progress.textContent = percentLoaded + '%';
			}
		};

		/**
		 * Setup the FDSock
		 */
		FDSock.prototype.setup = function(){
			var that = this;
			this.uploadBtn.addEventListener('click', function(){ that.init(); }, false);
		};

		FDSock.initialized = true;
	}

	/**
	 * Init the FDSock
	 */
	this.setup();
};
