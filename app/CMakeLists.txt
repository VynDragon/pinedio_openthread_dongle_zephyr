cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(pinedio_openthread_dongle_zephyr)

target_sources(app PRIVATE
src/main.c
)

# Pick activity
zephyr_link_libraries(
-Wl,--wrap=otPlatRadioEnable
-Wl,--wrap=otPlatRadioDisable
-Wl,--wrap=otPlatRadioTxDone
-Wl,--wrap=otPlatRadioReceiveDone)
