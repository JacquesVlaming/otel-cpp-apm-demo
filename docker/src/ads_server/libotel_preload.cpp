// #define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <chrono>

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#include <opentelemetry/nostd/shared_ptr.h>

namespace trace = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp = opentelemetry::exporter::otlp;

// Function pointer types
using AcceptFuncType = int(*)(int, struct sockaddr*, socklen_t*);
using ReadFuncType = ssize_t(*)(int, void*, size_t);

AcceptFuncType real_accept = nullptr;
ReadFuncType real_read = nullptr;

// Tracer and init flag
opentelemetry::nostd::shared_ptr<trace::Tracer> tracer;
bool otel_initialized = false;

// Lazy initialization
void init_tracing_lazy() {
    if (otel_initialized) return;
    otel_initialized = true;

    try {
        auto exporter = std::unique_ptr<trace_sdk::SpanExporter>(new otlp::OtlpGrpcExporter());
        auto processor = std::unique_ptr<trace_sdk::SpanProcessor>(new trace_sdk::SimpleSpanProcessor(std::move(exporter)));
        auto provider = std::shared_ptr<trace::TracerProvider>(new trace_sdk::TracerProvider(std::move(processor)));
        trace::Provider::SetTracerProvider(provider);

        tracer = provider->GetTracer("ads_server", "1.0");

        std::cout << "[OTEL PRELOAD] Tracing initialized (lazy)" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[OTEL PRELOAD] Failed to initialize tracing: " << e.what() << std::endl;
    }
}

extern "C" {

// Hook accept()
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    init_tracing_lazy();
    if (!real_accept) real_accept = (AcceptFuncType)dlsym(RTLD_NEXT, "accept");

    int client = real_accept(sockfd, addr, addrlen);
    if (client != -1 && tracer) {
        auto span = tracer->StartSpan("accept_connection");
        span->AddEvent("Accepted client socket: " + std::to_string(client));
        span->End();
    }
    return client;
}

// Hook read()
ssize_t read(int fd, void* buf, size_t count) {
    init_tracing_lazy();
    if (!real_read) real_read = (ReadFuncType)dlsym(RTLD_NEXT, "read");

    ssize_t bytes = real_read(fd, buf, count);
    if (bytes > 0 && tracer) {
        auto span = tracer->StartSpan("read_from_socket");
        span->AddEvent("Read " + std::to_string(bytes) + " bytes from fd: " + std::to_string(fd));
        span->End();
    }
    return bytes;
}

} // extern "C"
