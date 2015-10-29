#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>

//Node Network Info
#define NODEID        5    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
#define GATEWAYID     1
// #define FREQUENCY     RF69_433MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "Rfm69_EncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define ACK_TIME      30 // max # of ms to wait for an ack

//Moteino Pins
#define LED           9 // Moteinos have LEDs on D9
#define FLASH_SS      8 // and FLASH SS on D8
#define PWR           7 // Digital output for power strip

RFM69 radio;
bool promiscuousMode = true;

void setup () {

  Serial.begin(115200);

  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);

}

byte payload[3] = {'a', 'b', 'c' };

void loop () {

  if (millis()%10000 == 0) {
    // radio.send(GATEWAYID, payload, 3);
  	if (radio.sendWithRetry(GATEWAYID, payload, 3)) {
      Serial.println("Received Ack!");
      Serial.print("Rssi: ");
      Serial.println(radio.RSSI);
    } else {
      Serial.println("No Ack..");
    }
  	Blink(LED, 3);
  }

  if (radio.receiveDone()) {
    if (promiscuousMode) {
      Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    } else {
      if (radio.ACKRequested()) {
        radio.sendACK();
        Serial.println("Ack sent");
      }
    }

    Serial.println();
    Serial.print("DATALEN: ");
    Serial.println(radio.DATALEN);
    Serial.print("DATA = ");

    for (int i=0; i < radio.DATALEN; i++) {
      Serial.print((char)radio.DATA[i]);
    }
    Serial.println();

  }

}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
