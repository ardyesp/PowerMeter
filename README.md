# PowerMeter
nrf51822 based power meter. The meter measures the AC current on two incoming hot lines at breaker panel and calculates real power. 

The measured power is transmitted in the Bluetooth Advertisement packets (Short Local Name) like: **"W 2465"**. 

Use any BLE app like LightBlue or BLE Scanner to read power data.
The meter has proven to be accurate down to one watt when compared with GE I210+ meter.

# Schematic
![PowerMeter Schematic](https://github.com/ardyesp/PowerMeter/blob/master/docs/Schematic.png)

# Build
The potential divider network created with R3/R4 and with R5/R6 set the voltage at pin P0.01, P0.02 and P0.03 at Vcc/2.
Adjust the feed resistor R7, so that input AC voltage swing, stays within Vcc and Gnd. The input diode D2 protects against reverse biasing on this junction. 

Sketch can be compiled using [Arduino nrf51822](https://github.com/rogerclarkmelbourne/Arduino_nrf51822)

## Bill Of Material
1. Current Transformer: SCT-013-050 (50A/1V) (has integrated burden resistor of ~ 37 ohm)
2. Voltage Transformer: 110V -> 9V (any voltage will do; adjust potential divider and code accordingly)
3. Waveshare nrf51822 mini module (disconnected PCB antenna and added quarter wave whip for increased range)



