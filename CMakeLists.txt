cmake_minimum_required(VERSION 3.22)

# These patches fix compatibility with ESP-IDF 6.x and a SPI DMA bug.
# They are applied both immediately (if managed_components already exists)
# and deferred (after project.cmake runs the component manager on first download).
macro(apply_managed_component_patches)
    # tuanpmt__esp_wifi_manager: use espressif__cjson instead of removed built-in 'json'
    set(_f "${CMAKE_SOURCE_DIR}/managed_components/tuanpmt__esp_wifi_manager/CMakeLists.txt")
    if(EXISTS "${_f}")
        file(READ "${_f}" _c)
        if("${_c}" MATCHES "    json\n")
            string(REPLACE "    json\n" "    espressif__cjson\n" _c "${_c}")
            file(WRITE "${_f}" "${_c}")
            message(STATUS "Patched tuanpmt__esp_wifi_manager: json -> espressif__cjson")
        endif()
    endif()

    # tuanpmt__esp_bus: use esp_driver_gpio instead of removed monolithic 'driver'
    set(_f "${CMAKE_SOURCE_DIR}/managed_components/tuanpmt__esp_bus/CMakeLists.txt")
    if(EXISTS "${_f}")
        file(READ "${_f}" _c)
        if("${_c}" MATCHES "        driver\n")
            string(REPLACE "        driver\n" "        esp_driver_gpio\n" _c "${_c}")
            file(WRITE "${_f}" "${_c}")
            message(STATUS "Patched tuanpmt__esp_bus: driver -> esp_driver_gpio")
        endif()
    endif()

    # espressif__eppp_link: align SPI transaction length to 4 bytes (DMA crash on ESP32-S3)
    set(_f "${CMAKE_SOURCE_DIR}/managed_components/espressif__eppp_link/eppp_spi.c")
    if(EXISTS "${_f}")
        file(READ "${_f}" _c)
        if("${_c}" MATCHES "perform_transaction.h, sizeof.struct header. [+]")
            string(REPLACE
                "perform_transaction(h, sizeof(struct header) + h->transaction_size,"
                "perform_transaction(h, SPI_ALIGN(sizeof(struct header) + h->transaction_size),"
                _c "${_c}")
            file(WRITE "${_f}" "${_c}")
            message(STATUS "Patched espressif__eppp_link: SPI_ALIGN on transaction length")
        endif()
    endif()
endmacro()

# Apply now (subsequent builds where managed_components already exists)
apply_managed_component_patches()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Apply again after project.cmake (first build: component manager downloads here)
apply_managed_component_patches()

project(esp32-spi-eppp-server)
