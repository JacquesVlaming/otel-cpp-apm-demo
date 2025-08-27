#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "opentelemetry-cpp::ostream_span_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::ostream_span_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::ostream_span_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_ostream_span.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_ostream_span.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::ostream_span_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::ostream_span_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_ostream_span.so" )

# Import target "opentelemetry-cpp::ostream_metrics_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::ostream_metrics_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::ostream_metrics_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_ostream_metrics.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_ostream_metrics.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::ostream_metrics_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::ostream_metrics_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_ostream_metrics.so" )

# Import target "opentelemetry-cpp::ostream_log_record_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::ostream_log_record_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::ostream_log_record_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_ostream_logs.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_ostream_logs.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::ostream_log_record_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::ostream_log_record_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_ostream_logs.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
