#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <iostream>
#include <sstream>
#include <memory>

#include <opentelemetry/trace/tracer_provider.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/common/global_log_handler.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/samplers/always_on.h>
#include <opentelemetry/sdk/trace/samplers/trace_id_ratio.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>

namespace otel  = opentelemetry;
namespace trace = opentelemetry::trace;
namespace ctx   = opentelemetry::context;
namespace nostd = opentelemetry::nostd;
namespace sdk   = opentelemetry::sdk;
namespace t_sdk = opentelemetry::sdk::trace;
namespace otlp  = opentelemetry::exporter::otlp;

// ------------------------------
// Forward declarations for missing helpers
// ------------------------------
static void start_fd_span_if_needed(int fd, bool is_client, const std::string &ip, int port, const char* func_name);
static void annotate_bytes(int fd, const char* direction, ssize_t n);
static void end_fd_span(int fd, const char* func_name);

// ------------------------------
// Globals
// ------------------------------
static std::atomic<bool> g_inited{false};
static nostd::shared_ptr<trace::Tracer> g_tracer;

struct ConnSpan {
  nostd::shared_ptr<trace::Span> span;
  std::string peer_ip;
  int peer_port = 0;
  bool is_client = false;
};
static std::unordered_map<int, ConnSpan> g_fd_spans;
static std::mutex g_fd_mu;

thread_local bool g_in_hook = false;

// ------------------------------
// Environment helpers
// ------------------------------
static inline const char* getenv_str(const char* k, const char* defv) {
  const char* v = std::getenv(k);
  return v && *v ? v : defv;
}

static std::string get_service_name() {
  const char* s = std::getenv("OTEL_SERVICE_NAME");
  if (s && *s) return s;
  char buf[256] = {0};
  FILE* f = std::fopen("/proc/self/comm", "r");
  if (f) {
    if (std::fgets(buf, sizeof(buf), f)) {
      size_t n = std::strlen(buf);
      if (n && buf[n-1] == '\n') buf[n-1] = '\0';
      std::fclose(f);
      return std::string(buf);
    }
    std::fclose(f);
  }
  return "unknown-cpp-process";
}

// ------------------------------
// Tracer helper
// ------------------------------
static nostd::shared_ptr<trace::Tracer> get_tracer() {
  if (!g_tracer) {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    g_tracer = provider->GetTracer("otel-preload", "1.0.0");
  }
  return g_tracer;
}

// ------------------------------
// Socket helpers
// ------------------------------
static void set_common_net_attrs(trace::Span &span, int fd, const char* op) {
  span.SetAttribute("syscall", op);
  span.SetAttribute("net.sock.fd", fd);
}

static void addr_to_ip_port(const struct sockaddr *sa, socklen_t salen, std::string &ip, int &port) {
  char host[NI_MAXHOST], serv[NI_MAXSERV];
  host[0] = serv[0] = 0;
  if (sa && getnameinfo(sa, salen, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
    ip = host;
    port = std::atoi(serv);
  }
}

static void capture_peer_from_fd(int fd, std::string &ip, int &port) {
  struct sockaddr_storage ss{};
  socklen_t len = sizeof(ss);
  if (getpeername(fd, (struct sockaddr*)&ss, &len) == 0) {
    addr_to_ip_port((struct sockaddr*)&ss, len, ip, port);
  }
}

// ------------------------------
// dlsym wrappers
// ------------------------------
template <typename Fn>
static Fn resolve(const char* name) {
  void* sym = dlsym(RTLD_NEXT, name);
  if (!sym) {
    write(STDERR_FILENO, "[otel-preload] dlsym failed for ", 31);
    write(STDERR_FILENO, name, std::strlen(name));
    write(STDERR_FILENO, "\n", 1);
  }
  return reinterpret_cast<Fn>(sym);
}

// Function pointer types
using AcceptFn  = int(*)(int, struct sockaddr*, socklen_t*);
using ReadFn    = ssize_t(*)(int, void*, size_t);
using WriteFn   = ssize_t(*)(int, const void*, size_t);
using RecvFn    = ssize_t(*)(int, void*, size_t, int);
using SendFn    = ssize_t(*)(int, const void*, size_t, int);
using CloseFn   = int(*)(int);
using ConnectFn = int(*)(int, const struct sockaddr*, socklen_t);
using PCreateFn = int(*)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);

static AcceptFn  real_accept  = nullptr;
static ReadFn    real_read    = nullptr;
static WriteFn   real_write   = nullptr;
static RecvFn    real_recv    = nullptr;
static SendFn    real_send    = nullptr;
static CloseFn   real_close   = nullptr;
static ConnectFn real_connect = nullptr;
static PCreateFn real_pcreate = nullptr;

// ------------------------------
// OpenTelemetry init/shutdown
// ------------------------------
static void init_tracing_once() {
  bool expected = false;
  if (!g_inited.compare_exchange_strong(expected, true)) return;

  try {
    auto resource = sdk::resource::Resource::Create({
      {"service.name", get_service_name()},
      {"telemetry.sdk.language", "cpp"},
      {"telemetry.instrumentation_library", "otel-preload"},
    });

    std::unique_ptr<t_sdk::Sampler> sampler{new t_sdk::AlwaysOnSampler()};
    if (const char* smp = std::getenv("OTEL_TRACES_SAMPLER")) {
      if (std::strcmp(smp, "ratio") == 0) {
        double ratio = 1.0;
        if (const char* arg = std::getenv("OTEL_TRACES_SAMPLER_ARG")) {
          ratio = std::atof(arg);
          if (ratio < 0) ratio = 0;
          if (ratio > 1) ratio = 1;
        }
        sampler.reset(new t_sdk::TraceIdRatioBasedSampler(ratio));
      }
    }

    auto exporter = std::unique_ptr<t_sdk::SpanExporter>(new otlp::OtlpGrpcExporter());
    t_sdk::BatchSpanProcessorOptions bsp_opts;
    auto processor = std::unique_ptr<t_sdk::SpanProcessor>(
        new t_sdk::BatchSpanProcessor(std::move(exporter), bsp_opts)
    );

    auto provider = nostd::shared_ptr<trace::TracerProvider>(
        new t_sdk::TracerProvider(std::move(processor), resource, std::move(sampler))
    );

    opentelemetry::trace::Provider::SetTracerProvider(provider);
    g_tracer = provider->GetTracer("otel-preload", "1.0.0");

    real_accept  = resolve<AcceptFn>("accept");
    real_read    = resolve<ReadFn>("read");
    real_write   = resolve<WriteFn>("write");
    real_recv    = resolve<RecvFn>("recv");
    real_send    = resolve<SendFn>("send");
    real_close   = resolve<CloseFn>("close");
    real_connect = resolve<ConnectFn>("connect");
    real_pcreate = resolve<PCreateFn>("pthread_create");

    write(STDERR_FILENO, "[otel-preload] OpenTelemetry initialized\n", 41);
  } catch (...) {
    write(STDERR_FILENO, "[otel-preload] OTEL init failed\n", 32);
  }
}

static void shutdown_tracing() {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  if (auto sdk_provider = dynamic_cast<t_sdk::TracerProvider*>(provider.get())) {
    sdk_provider->ForceFlush(std::chrono::microseconds(500000));
    sdk_provider->Shutdown();
  }
}

__attribute__((constructor)) static void ctor_init() { init_tracing_once(); }
__attribute__((destructor)) static void dtor_shutdown() { shutdown_tracing(); }

// ------------------------------
// Minimal stub implementations
// ------------------------------
static void start_fd_span_if_needed(int fd, bool is_client, const std::string &ip, int port, const char* func_name) {
  std::lock_guard<std::mutex> lk(g_fd_mu);
  // minimal: store a dummy span
  g_fd_spans[fd] = ConnSpan{nullptr, ip, port, is_client};
}

static void annotate_bytes(int fd, const char* direction, ssize_t n) {
  (void)fd; (void)direction; (void)n;
  // minimal: no-op
}

static void end_fd_span(int fd, const char* func_name) {
  std::lock_guard<std::mutex> lk(g_fd_mu);
  g_fd_spans.erase(fd);
}

