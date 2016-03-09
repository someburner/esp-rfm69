#Moteino OTA Updates
-----------------------

This file describes the Moteino OTA firmware update process.


##Prerequisites
-----------------------

1. Properly connected and configured RFM69 module for ESP8266 (see README)
2. Matching frequency Moteino with [DualOptiBoot bootloader](https://github.com/LowPowerLab/DualOptiboot) and configured with matching security key
3. Compiled Moteino .hex FW file


##Prep
-----------------------

1. Convert your Moteino .hex FW to a .bin. For this I use hex2bin on Linux, but
   there are probably other tools for other platforms, so find one that suits
	yours.

2. Connect to the ESP8266, open a web browser, and go to your configured host,
	which is http://smart.rfm.com/ or http://192.168.4.1/ by default.

3. Navigate to the File System tab. You should see a list of all files currently
 	on SPIFFS.

4. Using your system's file explorer, navigate to your .bin file. Then,
	in the browser, double click the 'upload' button. This will activate a
	websocket connection to the ESP8266 and a drag-and-drop zone will appear.

5. Drag your FW file to the dropzone. Uploading should begin immediately. At
	100%, sit tight for second and wait for the filesystem to refresh the files.


##The Update
-----------------------

1. Once you have at least one binary you'd like to flash on SPIFFS, go back
	to the browser and navigate to the 'RFM OTA' tab.

2. A list should populate with all '.bin' files found on the ESP8266.

3. Select a FW file by clicking on it. Then enter in the Node ID you wish to
	send it to in the box that appears.

4. Click the '>>' green button and confirm that you want to perform the OTA.

5. OTA will commence and complete on its own. If your ESP8266 is connected via
	UART, you can monitor the debug output.


##Sample Output
-----------------------

##Seq 0

**ESP8266:**
```
fw init
rfm_begin_ota
ota bin name: txrx_blink.bin
generate_next_OTA_msg: state 1
	RFM_OTA_INIT
```

**Moteino:**
```
FLX?OK (ACK sent)
```

**ESP8266:**
```
process_rfm_ota_msg:
	FLXOK rx
generate_next_OTA_msg: state 2
	RFM_OTA_NEXT
tx begin
fifo write
tx sent    ---> Try 1
tx begin
fifo write
tx sent    ---> Try 2
tx begin
fifo write
tx sent    ---> Try 3
```

**Moteino:**
```
FLX?OK (ACK sent)     ---> Try 1/2 resp?
got FLX:_:___        ---> Try 3 received
radio [22] > 464c583a303a62c000000c94cd0a0c94fa0a83c00000
FLX:0:OK              ---> Try 3 resp
```
