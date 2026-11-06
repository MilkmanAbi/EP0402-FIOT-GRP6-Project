# CMF Phone 2 Pro setup

The target handset is the user's CMF Phone 2 Pro, reported Nothing OS 4.3.
This guide does not assume its hotspot uses `192.168.43.1`.

1. Install the supplied ParcelGuardian APK from your Downloads/file manager.
   Allow installation for that source when Android requests it.
2. In Android Settings, search for **hotspot**. Open Wi-Fi hotspot / Hotspot &
   tethering. Set a simple name and password, choose **2.4 GHz**, and choose
   **WPA2-Personal** or the compatible WPA2 option. The ESP-01 cannot join a
   5-GHz-only or WPA3-only hotspot. Exact labels can vary with the OS build.
3. Turn the hotspot on. If there is an automatic hotspot-off option, disable
   it for the demonstration. Mobile data is needed only for map tiles and
   the optional cloud relay.
4. In ParcelGuardian Settings, refresh phone addresses. Select the hotspot-side
   IPv4 address. Several addresses may be listed; selection is not an automatic
   guarantee of reachability. A connected laptop's default gateway is usually
   the hotspot address (`ipconfig` on Windows).
5. Save the app settings, then copy its firmware configuration. Put those values
   in `arduino/ParcelGuardian/Secrets.h`, with the actual hotspot SSID/password.
   This file is gitignored. Leave `UPLINK_PHONE=1` in Config.h.
6. Compile and upload the Uno. First verify `AT` using EspAtTest and the separate
   GPS, ADXL and SD bench tools. Select the actual Uno USB port when uploading.
7. On the phone's Overview screen, press **Start**. Leave the app open. The
   receiver session keeps the display awake; closing/backgrounding the app is
   outside the guaranteed operating mode.
8. From a laptop connected to that hotspot, open
   `http://PHONE_IP:8080/health`. It should display `ParcelGuardian receiver`.
   If it cannot connect, check the chosen IP, receiver state, hotspot isolation
   settings and whether the laptop really joined that phone.
9. Put the tracker outdoors for GPS. A check-in without GPS should still show
   LIVE contact, with no current position. Demo thresholds are 20 seconds to
   QUIET and 60 seconds to LOST. Turn the hotspot off, observe both transitions,
   restore it and check that a new valid packet restores LIVE.

## Cloud assessment

Create your own private ThingSpeak channel with the five fields in README order.
In the app's Settings, enable the HTTPS relay and enter the **write** key. It
mirrors a queued local record every 20 seconds while open. Verify a positive
entry ID and matching channel fields. The phone is both local server and
optional cloud gateway; the Uno does not need internet access itself.

## Handover

Use READOUT DIP mode or console `d` at 9600 baud. Save the CSV text without
console chatter, or read PGLOG.CSV directly from the card. Import it using the
file icon on Evidence. Imported SD history is kept separate from received
check-ins and never makes the tracker appear newly contacted.

The APK is a sideloadable classroom build signed with this machine's Android
debug certificate. Preserve that certificate to install future updates over
this build. Play Store distribution would need a dedicated release key.

## References

* [Flutter upgrade instructions](https://docs.flutter.dev/install/upgrade)
* [Dart bundled with Flutter](https://dart.dev/get-dart)
* [Arduino CLI installation](https://docs.arduino.cc/arduino-cli/installation)
* [OpenStreetMap tile policy](https://operations.osmfoundation.org/policies/tiles/)

No exact Nothing OS 4.3 menu sequence is asserted from older factory-OS pages;
use the Settings search if the menu names differ.
