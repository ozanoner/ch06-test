
# Notes

- Clone from https://github.com/ozanoner/embedded-devops-ch04
- Set remote repo
- Update .devcontainer/devcontainer.json
- Add .env (defining WIFI_SSID and WIFI_PWD)
- Update blinky/main/CMakeLists.txt to pass the WiFi SSID and password to the app from the environment
- Update blinky/main/idf_component.yml to import ozanoner/devops_easy_connect
- Update blinky/main/blinky.c to enable Wifi in the app.

- remove .github/workflows/ci.yml and update .github/workflows/release.yml to include the version.txt file in the releases.

- Add wifi creds as secrets to the repo. Note: this prevent creds to be seen on the repo but still embedded in the firmware binary and can be extracted.