/*
	Scan and display the power reading Tx by power meter
	Switch calculates the power deltas
	Adjust the Power Meter peerAddr pmAddress in the code below to reflect your power meter BLE address
	Local Name is		: W: 1238
	
	This code works on nrf51 smart watch - and sleeps after 30 sec of no button press
*/

#include <BLE_API.h>
#include "SFE_MicroOLED.h"

// -------- smart watch pin defs ---------
#define SWITCH 		4
#define OLED_RESET	P0_1
#define OLED_DC		P0_0
#define OLED_CS		P0_2

#define BATT_VOLTAGE	P0_5
#define SLEEP_AFTER		30		// seconds

SPI my_spi(P0_29, NC, P0_30);
MicroOLED oled(my_spi, OLED_RESET, OLED_DC, OLED_CS);

uint8_t pmAddress[6] = {0x2F, 0x3B, 0xED, 0xB5, 0xB6, 0xEE};		// <--- adjust this
int currentPower = 0;
int snappedPower = 0;
boolean toggle = true;
int batt1, batt2;

BLE	ble;
Ticker ticker;
long lastMsClick = 0;
char oBuf[10];



// ------------------------
void t_handle(void)	{
// ------------------------
	if((millis() > lastMsClick))
		// has been a long time without a button press
		shutdown();
}



// ------------------------
void shutdown(void)	{
// ------------------------
	// shutdown OLED
	digitalWrite(OLED_RESET, LOW);
	// powerdown nrf
	NRF_POWER->SYSTEMOFF = 1;
}




// ------------------------
void handle_irq1(void)	{
// ------------------------
	snappedPower = currentPower;
	lastMsClick = millis() + (SLEEP_AFTER * 1000);
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
    oled.clear(PAGE);
    oled.setCursor(0, 0);
	oled.setFontType(0);
    sprintf(oBuf, "%d.%d  %d", batt1, batt2, params->rssi );
    oled.puts(oBuf);

	oled.setFontType(1);
	
	uint8_t len;
	uint8_t adv_name[31];
	if( NRF_SUCCESS == ble_advdata_decode(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, params->advertisingDataLen, (uint8_t *)params->advertisingData, &len, adv_name) )	 {
		oled.setCursor(0, 9);
	    oled.puts((const char *)adv_name);
		
		// extract the current power from name
		oled.setCursor(0, 21);
		currentPower = atoi((const char *)adv_name + 3);
		
		if(toggle)
			sprintf(oBuf, "D: %d", currentPower - snappedPower);
		else
			sprintf(oBuf, " : %d", currentPower - snappedPower);
			
		oled.puts(oBuf);
		toggle = !toggle;
	}
	
	oled.display();
}



// ------------------------
void setup() {
// ------------------------
	oled.init(0, 8000000);
	oled.clear(PAGE);
	oled.setCursor(0, 0);
	oled.puts("Scanning PM ...");
	oled.display();
	
	pinMode(SWITCH, INPUT_PULLUP);
	attachInterrupt(SWITCH, handle_irq1, RISING);
	
	// let power on voltage settle
	delay(100);

	// Ref = 1.2V internal without prescaling; input = no prescaling
	analogReference(REFSEL_VBG, INPSEL_AIN_NO_PS);
	uint16_t value = analogRead(BATT_VOLTAGE);
	// watch has hardware prescaler
	int16_t battmV = value * 7.09;
	batt1 = battmV / 1000;
	batt2 = (battmV - batt1 * 1000) / 10;
	
	if(battmV < 3100 )	{
		oled.clear(PAGE);
		oled.setCursor(5, 5);
		oled.puts("LO BATT");
		oled.setCursor(5, 15);
		sprintf(oBuf, "%d.%d V", batt1, batt2);
		oled.puts(oBuf);
		oled.display();
		
		delay(2000);
		shutdown();
	}
	else	{
		ble.init();
		// scan for 2 sec every 2 seconds
		ble.setScanParams(2000, 2000, 0, false);
		ble.startScan(scanCallBack);
		ticker.attach(t_handle, SLEEP_AFTER);
	}
	
}



// ------------------------
void loop() {
// ------------------------
	ble.waitForEvent();
}




