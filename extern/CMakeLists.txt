set(BUILD_TESTING OFF CACHE BOOL "Disable expected testing.")
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "Disable expected testing.")
set(FMT_TEST OFF CACHE BOOL "Disable fmt testing.")

add_subdirectory(expected)
add_subdirectory(fmt)
add_subdirectory(libelfin)