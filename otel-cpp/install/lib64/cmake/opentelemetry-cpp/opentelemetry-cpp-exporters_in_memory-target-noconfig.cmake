#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "opentelemetry-cpp::in_memory_span_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::in_memory_span_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::in_memory_span_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_in_memory.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_in_memory.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::in_memory_span_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::in_memory_span_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_in_memory.so" )

# Import target "opentelemetry-cpp::in_memory_metric_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::in_memory_metric_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::in_memory_metric_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_in_memory_metric.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_in_memory_metric.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::in_memory_metric_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::in_memory_metric_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_in_memory_metric.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
