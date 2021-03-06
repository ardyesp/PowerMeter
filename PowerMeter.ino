/*
	nrf51822 based Power Meter
	Measure the current from two 50A/1V current transformer on the power line (hot-neutral-hot)
	Measure the AC voltage using a voltage transformer

	Both CT's on two hot lines are measured separately, and three waveforms are DC offset to Vcc/2.
	CT (SCT-013-050) internal burden R ~= 37 ohm

	nrf51288 ADC = crap. Serial.begin has to be called for accurate readings
	ADC output has to be adjusted for gain and offset constants
	nrf analogRead = ~70 uS
	
	Circuit:
	-------
	AC 9V adapter is connected in series with 220ohm and 1N4007 diode and 220 uF filter capacitor.
	AMS1117 3.3V regulator feeds off this DC input and generates a 3.3V Vcc
	
	A voltage divider is created from Vcc and Gnd and adapter input is connected to it through 15K resistor
	A germanium diode prevents reverse biasing of this junction. This creates a DC offset input AC voltage which is read at A3
	
	Another voltage divider from Vcc and Gnd is stabilized with a 100 uF cap and one end of both CT is connected to this junction
	Other end of CT is connected to A1 and A2. This creates DC offset AC current waveforms
*/


#define CT_1_IN_PIN				P0_2		// CT1 input connected here, A1 = P0_2
#define CT_2_IN_PIN				P0_3		// CT1 input connected here, A2 = P0_3
#define VOLTAGE_PIN				P0_4		// AC adapter lead connected here
#define WAVE_FREQUENCY			60			// Power line = 60 Hz
#define NUMBER_CYCLES_SAMPLE	10			// how long to sample input waveform
#define NUM_ADV_SEND			1			// how many advertisements sent before turning adv off
#define AVG_SIZE				5			// how many reading averaged before Tx
#define ADC_SPEED_US			80

#define VOLTAGE_CURRENT_FACTOR	0.0688		// adjust this multiplier when calibrating


// derived constants
#define WAVE_TIME_PERIOD_US		(1000000/WAVE_FREQUENCY)
#define NUM_SAMPLES_WAVE		(WAVE_TIME_PERIOD_US/ADC_SPEED_US)
#define NUM_SAMPLES_HALF_WAVE	(NUM_SAMPLES_WAVE/2)
#define NUM_SAMPLES_FULL_SAMPLE	(NUM_SAMPLES_WAVE * NUMBER_CYCLES_SAMPLE) 
#define TOTAL_SAMPLE_TIME_US	(WAVE_TIME_PERIOD_US * NUMBER_CYCLES_SAMPLE)

//#define APP_DBG

// Setup debug printing macros.
#ifdef APP_DBG
	#define DBG_INIT(...) 		{ Serial.begin(__VA_ARGS__); 	}
	#define DBG_PRINT(...) 		{ Serial.print(__VA_ARGS__); 	}
	#define DBG_PRINTLN(...) 	{ Serial.println(__VA_ARGS__); }
#else
	#define DBG_INIT(...) 		{}
	#define DBG_PRINT(...) 		{}
	#define DBG_PRINTLN(...) 	{}
#endif


#include <BLE_API.h>

BLE ble;

boolean advDone = false;
uint8_t numAdvSent;
uint8_t name[10];

// ADC built in constants
int8_t offsetError;
int8_t gainError;
uint16_t adcLookup[1024];

uint16_t runAvg[AVG_SIZE];
uint8_t counter = 0;

// waveforms are sampled and stored here before processing to improve sampling speed
int16_t voltageWave[NUM_SAMPLES_FULL_SAMPLE], currentWave[NUM_SAMPLES_FULL_SAMPLE];
uint16_t vOffset, cOffset;


// ------------------------
void rnCallback(bool radio_active)	{
// ------------------------
	DBG_PRINT("Rn cb: "); DBG_PRINTLN(radio_active);

	// radio messes analog reading sometimes, so synchronize with radio
	if(radio_active)
		numAdvSent++;

	if(!radio_active && (numAdvSent == NUM_ADV_SEND))
		advDone = true;
}



// ------------------------
void initWDT(int seconds)	{
// ------------------------
	NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos);
	NRF_WDT->CRV = seconds * 32768; // 32k tick
	NRF_WDT->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;
	NRF_WDT->TASKS_START = 1;
}



// ------------------------
void kickWDT() {
// ------------------------
	NRF_WDT->RR[0] = WDT_RR_RR_Reload;
}



// ------------------------
void setup() {
// ------------------------
	Serial.begin(250000);		// ??? The reading from ADC is incorrect if Serial not begin ??
	DBG_PRINTLN("Power meter start");

	ble.init();
	// set adv_type
	ble.setAdvertisingType(GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED);
	// set tx power,valid values are -40, -20, -16, -12, -8, -4, 0, 4
	ble.setTxPower(4);
	// set adv_interval to 100ms
	ble.setAdvertisingInterval(100);
	// set adv_timeout, in seconds
	ble.setAdvertisingTimeout(0);
	// hookup for radio events
	ble.gap().onRadioNotification(rnCallback);
	ble.gap().initRadioNotification();

	// initialize the ADC module
	// Ref = 1.2V internal without prescaling; input = 1/3 prescaling
	analogReference(REFSEL_VBG, INPSEL_AIN_1_3_PS);

	// read the built in gain and offset constants
	uint32_t ficr_value_32 = *(uint32_t*) 0x10000024;
	offsetError = ficr_value_32;
	gainError = ficr_value_32 >> 8;
	DBG_PRINT("ADC OFFSET: "); DBG_PRINT(offsetError); DBG_PRINT(", GAIN: "); DBG_PRINTLN(gainError);

	// build a lookup table for converting readings instead of using floating point calculation at run time
	for(int i = 0; i < 1024; i++)	{
		int16_t corrected = i * (1024 + gainError)/1024 + offsetError - 0.5;
		if(corrected < 0)
			adcLookup[i] = 0;
		else
			adcLookup[i] = corrected;
	}

	// force the 0th reading to 0
	adcLookup[0] = 0;
	
	// get DC offset for voltage and current
	vOffset = getDCOffset(VOLTAGE_PIN);
	cOffset = getDCOffset(CT_1_IN_PIN);
	
	DBG_PRINTLN("Starting Advertising");	
	// manually advertise first time
	ble.startAdvertising();
	// start the watchdog timer
	initWDT(5);
}




// ------------------------
void loop() {
// ------------------------
	ble.waitForEvent();
	// WFE is triggered after every radio notification callback
	if(advDone)	{
		DBG_PRINTLN("Adv done");
		advDone = false;
		ble.stopAdvertising();
		numAdvSent = 0;

		// 170 ms approx
		accumulateCyclePower();

		// update payload once all cycles accumulated
		if(!counter)	{
			updateAdvPayload();
			kickWDT();
		}

		// advertise again
		ble.startAdvertising();
	}
}



// ------------------------
void updateAdvPayload()	{
// ------------------------
	uint32_t avgWatts = 0;
	for(int i = 0; i < AVG_SIZE; i++)
		avgWatts += runAvg[i];
	avgWatts = avgWatts/AVG_SIZE;

	// convert data it into String
	sprintf((char*) name, "W: %d", avgWatts);

	ble.clearAdvertisingPayload();
	// setup adv_data and srp_data
	ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
	ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME, name, sizeof(name) - 1);
}



// ------------------------
void accumulateCyclePower()	{
// ------------------------
	DBG_PRINTLN("Getting cycle power");

	// accumulate the power for averaging
	runAvg[counter] = getCyclePower(CT_1_IN_PIN);
	runAvg[counter] += getCyclePower(CT_2_IN_PIN);

	counter++;
	if(counter == AVG_SIZE)
		counter = 0;
}




// ------------------------
uint16_t getCyclePower(int ctPin)	{
// ------------------------
	// Measuring the instantaneous power of waveform
	int idx = 0;
	uint32_t tStart = micros();
	uint32_t tEnd = tStart + TOTAL_SAMPLE_TIME_US;
	
	// will microseconds overflow from 32 bit counter
	if(tEnd < tStart)	{
		delayMicroseconds(TOTAL_SAMPLE_TIME_US * 2);
		tStart = micros();
		tEnd = tStart + TOTAL_SAMPLE_TIME_US;
	}
	
	// start the full wave multiple cycle sampling
	// this loop takes 101 samples of both voltage and current per wave cycle
	// 1.6416 degree lag from V -> I measurement can be ignored
	while(micros() < tEnd)	{
		voltageWave[idx] = readAnalog(VOLTAGE_PIN) - vOffset;
		currentWave[idx] = readAnalog(ctPin) - cOffset;
		idx++;
	}
	

	// debug captured data
#ifdef APP_DBG
		if(!counter)	{
			for(int i = 0; i < idx; i++)	{
				DBG_PRINT(voltageWave[i]);
				DBG_PRINT("\t");
				DBG_PRINTLN(currentWave[i]);
			}		
		}
#endif

	
	// calculate power = average of sum of instantaneous power
	int32_t sumIPower = 0;
	int32_t Vsum = 0;
	int32_t Csum = 0;
	for(int i = 0; i < idx; i++)	{
		sumIPower += voltageWave[i] * currentWave[i];
		// if old offset is correct, then sum should be zero 
		Vsum += voltageWave[i];
		Csum += currentWave[i];
	}		
	
	// average the sum of instantaneous power and multiply it by factor of voltage and current to get real power
	// Voltage: using adapter and potential divider: 423.5 units = 169.70 V => 0.4007 V/reading
	// Current: using 50A/1V CT: 1 unit = 3.6/1024 = 3.515 mV => 0.17578125 A/reading
	int32_t realPower = (sumIPower * VOLTAGE_CURRENT_FACTOR)/ idx;
	vOffset += Vsum / idx;
	cOffset += Csum / idx;

	// voltage is sensed in one phase only. Other CT will produce negative power
	if(realPower < 0)
		realPower = -realPower;
	
	DBG_PRINT(counter);DBG_PRINT(":");DBG_PRINTLN(realPower);
	return realPower;
}




// ------------------------
uint16_t getDCOffset(int pin)	{
// ------------------------
	uint32_t sumVal = 0;

	long tStart = micros();
	long tEnd = tStart + TOTAL_SAMPLE_TIME_US * 2;
	int idx = 0;
	
	while(micros() < tEnd)	{
		sumVal += readAnalog(pin);
		idx++;
	}
	
	// return the average of values
	return sumVal/idx;
}




// ------------------------
uint16_t readAnalog(int ctPin)	{
// ------------------------
	// read the analog input
	uint16_t x = analogRead(ctPin);
	return adcLookup[x];
}

