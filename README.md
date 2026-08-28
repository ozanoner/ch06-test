
# Notes

- Clone from https://github.com/ozanoner/embedded-devops-ch04
- Set remote repo
- remove tests and .github/workflows/ci.yml

- Update .devcontainer/devcontainer.json
- Add .env (defining WIFI_SSID and WIFI_PWD)
- Add wifi creds as secrets to the repo. Note: this prevent creds to be seen on the repo but still embedded in the firmware binary and can be extracted.
- Update .github/workflows/release.yml for wifi creds

- Update blinky/main/CMakeLists.txt to pass the WiFi SSID and password to the app from the environment
- Update blinky/main/idf_component.yml to import ozanoner/devops_easy_connect
- Update blinky/main/blinky.c to enable Wifi in the app.



- Add blinky/partitions.txt and blinky/sdkconfig.defaults


## Tags & releases

Remove existing tags and tag the current version

```bash
git tag -l # list
git tag -d v0.1.0 # delete
git tag -a v0.1.0 -m "Release version 0.1.0" # create a tag
git push origin v0.1.0 # push remote

git push && git push --tags
```

## updating to a new version
1. update version.txt
2. update code
3. build and verify
```
esptool.py --chip esp32 image_info build/blinky.bin

Application Information
=======================
Project name: blinky
App version: 0.1.1
Compile time: Aug 27 2026 16:34:00
ELF file SHA256: f71d4cc5c5157c29453f0f75b36f27d3925f3ebe144b35546b9d70906e501519
ESP-IDF: v6.0
Minimal eFuse block revision: 0.0
Maximal eFuse block revision: 0.99
MMU page size: 64 KB
Secure version: 0
```

4. commit, tag, and push


## signing

Application signing is enabled without hardware Secure Boot using the Secure
Boot version 2 RSA-3072 signing scheme. The bootloader checks the signature
when an app boots, and OTA updates are checked before they are accepted.
Hardware Secure Boot remains disabled, so this protects against unsigned or
tampered network updates but does not prevent physical bootloader replacement.
This configuration requires an ESP32 revision 3.1 or newer; the connected
devkit is revision 3.1.
It also uses a 4 MB flash image layout so the two OTA slots have room for
signed application images.

Keep the private signing key outside source control in
`../keys/signing_key.pem`, relative to the `blinky` project directory. The
repository-level `keys/` directory is ignored by Git.


```bash
root@d443f0cad006:/workspace/blinky# esptool chip_id
Warning: Deprecated: Command 'chip_id' is deprecated. Use 'chip-id' instead.
esptool v5.2.0
Connected to ESP32 on /dev/ttyUSB0:
Chip type:          ESP32-D0WD-V3 (revision v3.1)
Features:           Wi-Fi, BT, Dual Core + LP Core, 240MHz, Vref calibration in eFuse, Coding Scheme None
Crystal frequency:  40MHz
MAC:                34:5f:45:c4:f8:94

Stub flasher running.

Warning: ESP32 has no chip ID. Reading MAC address instead.
MAC:                34:5f:45:c4:f8:94

Hard resetting via RTS pin...
```

To build and verify a new version 2 image, the signing key must be an RSA-3072
key. Generate or replace the key first, then remove the stale temporary
configuration before rebuilding:

```bash
espsecure generate-signing-key --version 2 --scheme rsa3072 \
	../keys/signing_key.pem
rm -f sdkconfig sdkconfig.old
idf.py reconfigure
idf.py build
espsecure verify-signature --version 2 \
	--keyfile ../keys/signing_key.pem build/blinky.bin
```


Flash the signed bootloader, partition table, and application together when
enabling this configuration on a device:

```bash
idf.py -p /dev/ttyUSB0 flash
```

The same signing key must be available to release builds. Do not commit the
private key or expose it in workflow logs. A release build that cannot read
`../keys/signing_key.pem` will fail intentionally.
