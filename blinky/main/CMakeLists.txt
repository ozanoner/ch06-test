if(CONFIG_IDF_TARGET_ESP32C3)
    set(APP_BSP_SRC "app_bsp_esp32c3.c")
else()
    set(APP_BSP_SRC "app_bsp_esp32.c")
endif()

idf_component_register(SRCS ${APP_BSP_SRC} "blinky.c" "app_bsp_button.c"
                    INCLUDE_DIRS ".")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PWD="${WIFI_PWD}"
)


