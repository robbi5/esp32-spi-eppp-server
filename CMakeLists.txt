cmake_minimum_required(VERSION 3.22)

# ESP-IDF compatibility patches are now handled by apply_patches.sh
# which should be run before building (done automatically in CI).

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32-spi-eppp-server)
