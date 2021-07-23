# ESP32_presence_detection
### ESP32 program to detect devices in range using bluetooth low energy and trigger actions through MQTT

I created this program because I wanted to use an ESP32 to detect if my iPhone entered the room in order to turn on the lights, but I couldn't use any of the examples already made online because they would detect specific BLE addresses, and iPhones change their address about every 30 minutes or so. Instead, I made this program to detect any device in range as long as it was not in the list of addresses to ignore (such as a TV or something that does not leave the room). If any device enters or leaves the range after a specific number of consecutive scans, it will publish to an MQTT topic which can then be used to control any smart device.
