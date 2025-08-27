#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "opentelemetry-cpp::otlp_grpc_client" for configuration ""
set_property(TARGET opentelemetry-cpp::otlp_grpc_client APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::otlp_grpc_client PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NOCONFIG "gRPC::grpc++"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc_client.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_otlp_grpc_client.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::otlp_grpc_client )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::otlp_grpc_client "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc_client.so" )

# Import target "opentelemetry-cpp::proto_grpc" for configuration ""
set_property(TARGET opentelemetry-cpp::proto_grpc APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::proto_grpc PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_proto_grpc.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_proto_grpc.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::proto_grpc )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::proto_grpc "${_IMPORT_PREFIX}/lib64/libopentelemetry_proto_grpc.so" )

# Import target "opentelemetry-cpp::otlp_grpc_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::otlp_grpc_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::otlp_grpc_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_otlp_grpc.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::otlp_grpc_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::otlp_grpc_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc.so" )

# Import target "opentelemetry-cpp::otlp_grpc_log_record_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::otlp_grpc_log_record_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::otlp_grpc_log_record_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc_log.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_otlp_grpc_log.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::otlp_grpc_log_record_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::otlp_grpc_log_record_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc_log.so" )

# Import target "opentelemetry-cpp::otlp_grpc_metrics_exporter" for configuration ""
set_property(TARGET opentelemetry-cpp::otlp_grpc_metrics_exporter APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(opentelemetry-cpp::otlp_grpc_metrics_exporter PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc_metrics.so"
  IMPORTED_SONAME_NOCONFIG "libopentelemetry_exporter_otlp_grpc_metrics.so"
  )

list(APPEND _cmake_import_check_targets opentelemetry-cpp::otlp_grpc_metrics_exporter )
list(APPEND _cmake_import_check_files_for_opentelemetry-cpp::otlp_grpc_metrics_exporter "${_IMPORT_PREFIX}/lib64/libopentelemetry_exporter_otlp_grpc_metrics.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
