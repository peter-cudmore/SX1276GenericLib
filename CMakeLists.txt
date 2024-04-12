
add_library(SX1276 INTERFACE)

target_include_directories(
        SX1276
        INTERFACE
        radio
        registers
        sx1276
)

target_sources(SX1276
    INTERFACE
        sx1276/sx1276.cpp
        sx1276/sx1276-mbed-hal.cpp
        radio/radio.cpp
)

target_link_libraries(
        SX1276
        INTERFACE
        mbed-baremetal
)