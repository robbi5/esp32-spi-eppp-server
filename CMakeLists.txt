cmake_minimum_required(VERSION 3.22)

# These patches fix compatibility with ESP-IDF 6.x and a SPI DMA bug.
# They are applied both immediately (if managed_components already exists)
# and deferred (after project.cmake runs the component manager on first download).
macro(apply_managed_component_patches)
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
