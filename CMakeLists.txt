cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "components/lvgl__lvgl"
    "components/lv_mem_psram"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mycube3_5)
