/*
	Scan and display the power reading Tx by power meter
	Switch calculates the power deltas
	Adjust the Power Meter peerAddr pmAddress in the code below to reflect your power meter BLE address
	Local Name is		: W: 1238
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_nrf_SSD1306.h"

// -------- nrf51822 pin defs ---------
#define NANO_OLED_RESET 6
#define SWITCH			D20

// I2C interface
// nrF - P29 (SDA1) and P28 (SCL1)
Adafruit_SSD1306 display(NANO_OLED_RESET);
uint8_t pmAddress[6] = {0x2F, 0x3B, 0xED, 0xB5, 0xB6, 0xEE};		// <--- adjust this
int currentPower = 0;
int snappedPower = 0;
boolean toggle = true;

BLE	ble;


// ------------------------
void handle_irq1(void)	{
// ------------------------
	snappedPower = currentPower;
}


// p_advdata = [length type payload] [length type payload] [length type payload]
// ------------------------
uint32_t ble_advdata_decode(uint8_t type, uint8_t advdata_len, uint8_t *p_advdata, uint8_t *len, uint8_t *p_field_data) {
// ------------------------
	uint8_t index = 0;
	uint8_t field_length, field_type;

	while(index < advdata_len)	{
		field_length = p_advdata[index];
		field_type	 = p_advdata[index + 1];

		if(field_type == type)		{
			memcpy(p_field_data, &p_advdata[index + 2], (field_length - 1));
			*len = field_length - 1;
			return NRF_SUCCESS;
		}

		index += field_length + 1;
	}
	return NRF_ERROR_NOT_FOUND;
}



// ------------------------
void scanCallBack(const Gap::AdvertisementCallbackParams_t *params) {
// ------------------------
	// does the address match our power meter
	for(uint8_t index = 0; index < 6; index++)
		if(pmAddress[index] != params->peerAddr[index])
			return;

	// PM adv packet found
	display.clearDisplay();
	display.setCursor(0,0);
	display.setTextSize(1);
	display.print("RSSI: ");
	display.print(params->rssi);

	display.setTextSize(2);
	
	uint8_t len;
	uint8_t adv_name[31];
	if( NRF_SUCCESS == ble_advdata_decode(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, params->advertisingDataLen, (uint8_t *)params->advertisingData, &len, adv_name) )	 {
		display.setCursor(0, 18);
		display.println((const char *)adv_name);
		
		display.print("S: ");
		display.println(snappedPower);
		
		// extract the current power from name
		currentPower = atoi((const char *)adv_name + 3);
		
		if(toggle)
			display.print("D: ");
		else
			display.print(" : ");
			
		toggle = !toggle;
		
		display.println(currentPower - snappedPower);
	}
	 
	display.display();
}



// ------------------------
void setup() {
// ------------------------
	Serial.begin(115200);
	Serial.println("Start...");
	
	pinMode(D20, INPUT_PULLUP);
	attachInterrupt(D20, handle_irq1, RISING);

	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.println("Scanning for power meter...");
	display.display();

	ble.init();
	// scan for 2 sec every 2 seconds
	ble.setScanParams(2000, 2000, 0, false);
	ble.startScan(scanCallBack);
}


// ------------------------
void loop() {
// ------------------------
	ble.waitForEvent();
}




