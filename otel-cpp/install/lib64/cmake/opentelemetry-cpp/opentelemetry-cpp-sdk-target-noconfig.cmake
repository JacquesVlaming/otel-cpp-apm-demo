#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "opentelemetry-cpp::common" for configuration ""
set_property(TARGET opentelemetry-cpp::common APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::common PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_common.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_common.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::common )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::common "${_IMPORT_PREFIX}/lib64/libopentelemetry_common.so" )

# Import target "opentelemetry-cpp::resources" for configuration ""
set_property(TARGET opentelemetry-cpp::resources APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::resources PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_resources.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_resources.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::resources )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::resources "${_IMPORT_PREFIX}/lib64/libopentelemetry_resources.so" )

# Import target "opentelemetry-cpp::version" for configuration ""
set_property(TARGET opentelemetry-cpp::version APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::version PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_version.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_version.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::version )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::version "${_IMPORT_PREFIX}/lib64/libopentelemetry_version.so" )

# Import target "opentelemetry-cpp::logs" for configuration ""
set_property(TARGET opentelemetry-cpp::logs APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::logs PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_logs.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_logs.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::logs )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::logs "${_IMPORT_PREFIX}/lib64/libopentelemetry_logs.so" )

# Import target "opentelemetry-cpp::trace" for configuration ""
set_property(TARGET opentelemetry-cpp::trace APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::trace PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_trace.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_trace.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::trace )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::trace "${_IMPORT_PREFIX}/lib64/libopentelemetry_trace.so" )

# Import target "opentelemetry-cpp::metrics" for configuration ""
set_property(TARGET opentelemetry-cpp::metrics APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::metrics PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_metrics.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_metrics.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::metrics )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::metrics "${_IMPORT_PREFIX}/lib64/libopentelemetry_metrics.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
