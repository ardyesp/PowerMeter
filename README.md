# PowerMeter
nrf51822 based power meter. The meter measures the AC current on two incoming hot lines at breaker panel and calculates real power. 

The measured power is transmitted in the Bluetooth Advertisement packets (Short Local Name) like: **"W 2465"**. 

Use any BLE app like LightBlue or BLE Scanner to read power data.
The meter has proven to be accurate down to one watt when compared with GE I210+ meter.

# Schematic
[logo]:  docs/Schematic.png "Schematic"

## Bill Of Material
1. Current Transformer: SCT-013-050 (50A/1V) (has integrated burden resistor of ~ 37 ohm)
2. Voltage Transformer: 110V -> 9V (any voltage will do; adjust potential divider and code accordingly)
3. Waveshare nrf51822 mini module (disconnected PCB antenna and added quarter wave whip for increased range)



