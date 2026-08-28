
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
