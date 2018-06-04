## A C library and C++ class for using semver in ESP32 IDF

A semver library packaged as an ESP IDF component for ESP32.

Note: the C++ class is not merged yet

### Installation

#### ESP-IDF build system

Clone this repository into your `components` directory inside the project:

    cd components
    git clone https://github.com/infincia/semver_espidf.git


#### PlatformIO

Add this repository as a library in your `platformio.ini` file in the root of 
your project:

    [env:development]
    platform = espressif32
    board = esp32dev
    framework = espidf
    lib_deps =
      https://github.com/infincia/semver_espidf.git#v0.1.0

### Full example of usage

This is a bare minimum `main.cpp` file you can refer to when using this library. 

    #include <esp_err.h>
    #include <esp_log.h>
    static const char *TAG = "[MyProject]";

    #include <c_semver.h>

    static char* VERSION = "1.0.0";

    extern "C" {
    void app_main();
    }

    int app_main() {

        struct semver_context current_version;

        int32_t cres;

        semver_init(&current_version, VERSION);

        cres = semver_parse(&current_version);

        if (cres != SEMVER_PARSE_OK) {
            ESP_LOGI(TAG, "current_version check failed: %d", cres);
            semver_free(&current_version);
            return 1;
        }

        printf("major = %d, minor = %d, patch = %d\n", current_version.major, current_version.minor, current_version.patch);

        // do something 

        return 0;
    }
