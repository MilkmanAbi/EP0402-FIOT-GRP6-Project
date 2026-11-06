# ParcelGuardian Arduino firmware

Arduino Uno R3 with the supplied tracker shield: ADXL345, NEO-6M, ESP-01,
microSD, LED, buzzer and two DIP switches.

## Configure and upload

1. Install **Arduino AVR Boards** and the **SD** library by Arduino.
2. Copy `arduino/ParcelGuardian/Secrets.example.h` to
   `arduino/ParcelGuardian/Secrets.h`.
3. In the Android app, save the pairing settings and copy the firmware config.
   Enter the matching phone address, API key and shipment ID in Secrets.h,
   alongside the actual 2.4 GHz WPA2 hotspot credentials.
4. Open `ParcelGuardian/ParcelGuardian.ino` in Arduino IDE. Select Arduino Uno
   and the actual USB port, then upload. Serial Monitor: 9600 baud.

Run from the extracted bundle/repository root with Arduino CLI:

```powershell
arduino-cli core install arduino:avr
arduino-cli lib install SD
Copy-Item arduino/ParcelGuardian/Secrets.example.h arduino/ParcelGuardian/Secrets.h
# Edit Secrets.h; do not overwrite an already configured file.
arduino-cli compile --fqbn arduino:avr:uno --warnings all arduino/ParcelGuardian
arduino-cli board list
arduino-cli upload --fqbn arduino:avr:uno --port COM_PORT arduino/ParcelGuardian
```

Replace `COM_PORT` with the detected port. `tools/build-arduino.ps1` compiles the
main sketch and all four bench tools. It creates an example Secrets.h only if
one is missing. The release ZIP contains no configured Secrets.h or credentials.

## Bench tools

| Sketch | Purpose |
|---|---|
| tools/EspAtTest | Verify ESP-01 AT responses at 9600 baud |
| tools/GpsRawTest | Verify GPS raw NMEA before seeking an outdoor fix |
| tools/SdCardTest | Verify FAT16/FAT32 card write/read on CS D10 |
| tools/AdxlDropTest | Verify accelerometer and measure impact thresholds |

Use each sketch independently before the combined tracker. See
`docs/CALIBRATION.md` for the physical threshold worksheet and
`docs/PHONE_SETUP.md` for the phone/hotspot instructions.

## Pin map

ADXL345 A4/A5; SD MISO D12, MOSI D11, SCK D13, CS D10; buzzer D9;
shield LED D8; DIP D7/D6; GPS Uno RX/TX D4/D5; ESP Uno RX/TX D2/D3.
The ESP-01 requires 3.3 V power/logic handling and AT firmware at 9600 baud.

## Operation

SD stores the full recorded history. The default uplink is the phone's HTTP
receiver on port 8080; `UPLINK_PHONE=0` retains the legacy ThingSpeak path.
Default impact tiers are 0.40/1.20 g with a 2-second debounce, pending physical
calibration. Console `?` lists commands, `d` dumps the SD log, and DIP mode 11
enters READOUT. DIP polarity needs checking on the actual shield.

Keep credentials private. The local HTTP link is unencrypted. GPS reception
pauses during ESP transactions, brief shocks can be missed, and SD writes are
not guaranteed against sudden power loss. See the included protocol, security
and verification documents for the exact operating limits.
