# ESP-IDF component registration.
# This file is used when FR_Math is consumed as an ESP-IDF component
# (via the ESP Component Registry or as a managed/local component).
# It is ignored by Arduino IDE, PlatformIO, and plain Make builds.

if(ESP_PLATFORM)
    idf_component_register(
        SRCS "src/FR_math.c"
        INCLUDE_DIRS "src"
    )
endif()
