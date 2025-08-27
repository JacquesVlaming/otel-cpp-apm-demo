// #define _GNU_SOURCE
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
#include <opentelemetry/sdk/trace/samplers/parent.h>
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
// Helpers & Globals
// ------------------------------
static std::atomic<bool> g_inited{false};
static nostd::shared_ptr<trace::Tracer> g_tracer;

// Connection lifecycle spans per fd (server- or client-side)
struct ConnSpan {
  nostd::shared_ptr<trace::Span> span;
  std::string peer_ip;
  int peer_port = 0;
  bool is_client = false;
};
static std::unordered_map<int, ConnSpan> g_fd_spans;
static std::mutex g_fd_mu;

// Reentrancy guard (avoid infinite recursion during our own I/O)
thread_local bool g_in_hook = false;

static inline const char* getenv_str(const char* k, const char* defv) {
  const char* v = std::getenv(k);
  return v && *v ? v : defv;
}

static std::string get_service_name() {
  const char* s = std::getenv("OTEL_SERVICE_NAME");
  if (s && *s) return s;
  // fallback: process name from /proc/self/comm (Linux)
  char buf[256] = {0};
  FILE* f = std::fopen("/proc/self/comm", "r");
  if (f) {
    if (std::fgets(buf, sizeof(buf), f)) {
      // strip newline
      size_t n = std::strlen(buf);
      if (n && buf[n-1] == '\n') buf[n-1] = '\0';
      std::fclose(f);
      return std::string(buf);
    }
    std::fclose(f);
  }
  return "unknown-cpp-process";
}

static nostd::shared_ptr<trace::Tracer> get_tracer() {
  if (!g_tracer) {
    auto provider = trace::Provider::GetTracerProvider();
    g_tracer = provider->GetTracer("otel-preload", "1.0.0");
  }
  return g_tracer;
}

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
    // Avoid printing with std::cerr inside hooks without guard
    write(STDERR_FILENO, "[otel-preload] dlsym failed for ", 31);
    write(STDERR_FILENO, name, std::strlen(name));
    write(STDERR_FILENO, "\n", 1);
  }
  return reinterpret_cast<Fn>(sym);
}

// Real function pointers
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
    // Resource: set service.name (plus a few handy attrs)
    auto resource = sdk::resource::Resource::Create({
      {"service.name", get_service_name()},
      {"telemetry.sdk.language", "cpp"},
      {"telemetry.instrumentation_library", "otel-preload"},
    });

    // Sampler (env override: OTEL_TRACES_SAMPLER=ratio, OTEL_TRACES_SAMPLER_ARG=0.1)
    std::unique_ptr<t_sdk::Sampler> sampler{new t_sdk::Parent};
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
      // add more if you like
    }

    // Exporter
    auto exporter = std::unique_ptr<t_sdk::SpanExporter>(new otlp::OtlpGrpcExporter());

    // Batch processor (env: OTEL_BSP_* works via SDK)
    auto processor = std::unique_ptr<t_sdk::SpanProcessor>(new t_sdk::BatchSpanProcessor(std::move(exporter)));

    // Provider
    auto provider = nostd::shared_ptr<trace::TracerProvider>(
        new t_sdk::TracerProvider(std::move(processor), resource, std::move(sampler)));

    trace::Provider::SetTracerProvider(provider);
    g_tracer = provider->GetTracer("otel-preload", "1.0.0");

    // Resolve libc symbols only once here to reduce contention later
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
  // Ensure provider flushes
  auto provider = trace::Provider::GetTracerProvider();
  if (auto sdk_provider = dynamic_cast<t_sdk::TracerProvider*>(provider.get())) {
    sdk_provider->ForceFlush(std::chrono::microseconds(500000)); // 0.5s
    sdk_provider->Shutdown();
  }
}

// Eager init to ensure we have a provider even if no hooks fire early
__attribute__((constructor))
static void ctor_init() { init_tracing_once(); }

__attribute__((destructor))
static void dtor_shutdown() { shutdown_tracing(); }

// ------------------------------
// Thread context propagation
// ------------------------------
// Wrap pthread start routine to carry current context into the new thread.
struct ThreadStartCtx {
  void* (*user_start)(void*);
  void* user_arg;
  ctx::Context parent_ctx;
};

static void* thread_trampoline(void* arg) {
  std::unique_ptr<ThreadStartCtx> holder((ThreadStartCtx*)arg);
  // Attach the captured context in this thread
  auto token = ctx::RuntimeContext::Attach(holder->parent_ctx);
  return holder->user_start(holder->user_arg);
}

// ------------------------------
// Span helpers for connection lifecycle
// ------------------------------
static void start_fd_span_if_needed(int fd, bool is_client, const std::string &peer_ip, int peer_port, const char* why) {
  std::lock_guard<std::mutex> lk(g_fd_mu);
  if (g_fd_spans.find(fd) != g_fd_spans.end()) return;

  auto tr = get_tracer();
  auto span = tr->StartSpan(is_client ? "socket.client" : "socket.server");
  span->SetAttribute("net.peer.ip", peer_ip);
  span->SetAttribute("net.peer.port", peer_port);
  span->SetAttribute("net.transport", "ip_tcp");
  span->SetAttribute("net.sock.fd", fd);
  span->SetAttribute("lifecycle.event", why);

  g_fd_spans.emplace(fd, ConnSpan{span, peer_ip, peer_port, is_client});
}

static void annotate_bytes(int fd, const char* direction, ssize_t n) {
  std::lock_guard<std::mutex> lk(g_fd_mu);
  auto it = g_fd_spans.find(fd);
  if (it != g_fd_spans.end() && it->second.span) {
    it->second.span->AddEvent(std::string(direction) + "_bytes", {{"bytes", static_cast<int64_t>(n)}});
  }
}

static void end_fd_span(int fd, const char* why) {
  std::lock_guard<std::mutex> lk(g_fd_mu);
  auto it = g_fd_spans.find(fd);
  if (it != g_fd_spans.end() && it->second.span) {
    it->second.span->SetAttribute("lifecycle.close_reason", why);
    it->second.span->End();
    g_fd_spans.erase(it);
  }
}

// ------------------------------
// Hooked functions
// ------------------------------
extern "C" {

// accept()
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
  if (g_in_hook) return real_accept ? real_accept(sockfd, addr, addrlen) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_accept) real_accept = resolve<AcceptFn>("accept");

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.accept");
  set_common_net_attrs(*span.get(), sockfd, "accept");

  int client = -1;
  {
    trace::Scope s(span);
    client = real_accept ? real_accept(sockfd, addr, addrlen) : -1;
    saved_errno = errno;

    if (client >= 0) {
      std::string ip; int port = 0;
      if (addr && addrlen) {
        addr_to_ip_port(addr, *addrlen, ip, port);
      } else {
        capture_peer_from_fd(client, ip, port);
      }
      span->SetAttribute("net.peer.ip", ip);
      span->SetAttribute("net.peer.port", port);
      span->SetStatus(trace::StatusCode::kOk);
      start_fd_span_if_needed(client, /*is_client=*/false, ip, port, "accept");
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return client;
}

// connect()
int connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
  if (g_in_hook) return real_connect ? real_connect(fd, addr, addrlen) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_connect) real_connect = resolve<ConnectFn>("connect");

  std::string ip; int port = 0;
  addr_to_ip_port(addr, addrlen, ip, port);

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.connect");
  set_common_net_attrs(*span.get(), fd, "connect");
  span->SetAttribute("net.peer.ip", ip);
  span->SetAttribute("net.peer.port", port);

  int rc = -1;
  {
    trace::Scope s(span);
    rc = real_connect ? real_connect(fd, addr, addrlen) : -1;
    saved_errno = errno;
    if (rc == 0) {
      span->SetStatus(trace::StatusCode::kOk);
      start_fd_span_if_needed(fd, /*is_client=*/true, ip, port, "connect");
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return rc;
}

// read()
ssize_t read(int fd, void* buf, size_t count) {
  if (g_in_hook) return real_read ? real_read(fd, buf, count) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_read) real_read = resolve<ReadFn>("read");

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.read");
  set_common_net_attrs(*span.get(), fd, "read");
  span->SetAttribute("io.requested", static_cast<int64_t>(count));

  ssize_t n = -1;
  {
    trace::Scope s(span);
    n = real_read ? real_read(fd, buf, count) : -1;
    saved_errno = errno;
    if (n >= 0) {
      span->SetAttribute("io.read", static_cast<int64_t>(n));
      span->SetStatus(trace::StatusCode::kOk);
      annotate_bytes(fd, "in", n);
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return n;
}

// write()
ssize_t write(int fd, const void* buf, size_t count) {
  if (g_in_hook) return real_write ? real_write(fd, buf, count) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_write) real_write = resolve<WriteFn>("write");

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.write");
  set_common_net_attrs(*span.get(), fd, "write");
  span->SetAttribute("io.requested", static_cast<int64_t>(count));

  ssize_t n = -1;
  {
    trace::Scope s(span);
    n = real_write ? real_write(fd, buf, count) : -1;
    saved_errno = errno;
    if (n >= 0) {
      span->SetAttribute("io.written", static_cast<int64_t>(n));
      span->SetStatus(trace::StatusCode::kOk);
      annotate_bytes(fd, "out", n);
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return n;
}

// recv()
ssize_t recv(int fd, void* buf, size_t len, int flags) {
  if (g_in_hook) return real_recv ? real_recv(fd, buf, len, flags) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_recv) real_recv = resolve<RecvFn>("recv");

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.recv");
  set_common_net_attrs(*span.get(), fd, "recv");
  span->SetAttribute("io.requested", static_cast<int64_t>(len));
  span->SetAttribute("recv.flags", flags);

  ssize_t n = -1;
  {
    trace::Scope s(span);
    n = real_recv ? real_recv(fd, buf, len, flags) : -1;
    saved_errno = errno;
    if (n >= 0) {
      span->SetAttribute("io.read", static_cast<int64_t>(n));
      span->SetStatus(trace::StatusCode::kOk);
      annotate_bytes(fd, "in", n);
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return n;
}

// send()
ssize_t send(int fd, const void* buf, size_t len, int flags) {
  if (g_in_hook) return real_send ? real_send(fd, buf, len, flags) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_send) real_send = resolve<SendFn>("send");

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.send");
  set_common_net_attrs(*span.get(), fd, "send");
  span->SetAttribute("io.requested", static_cast<int64_t>(len));
  span->SetAttribute("send.flags", flags);

  ssize_t n = -1;
  {
    trace::Scope s(span);
    n = real_send ? real_send(fd, buf, len, flags) : -1;
    saved_errno = errno;
    if (n >= 0) {
      span->SetAttribute("io.written", static_cast<int64_t>(n));
      span->SetStatus(trace::StatusCode::kOk);
      annotate_bytes(fd, "out", n);
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return n;
}

// close()
int close(int fd) {
  if (g_in_hook) return real_close ? real_close(fd) : -1;
  g_in_hook = true;
  int saved_errno = 0;

  init_tracing_once();
  if (!real_close) real_close = resolve<CloseFn>("close");

  auto tr = get_tracer();
  auto span = tr->StartSpan("sys.close");
  set_common_net_attrs(*span.get(), fd, "close");

  int rc = -1;
  {
    trace::Scope s(span);
    rc = real_close ? real_close(fd) : -1;
    saved_errno = errno;
    if (rc == 0) {
      span->SetStatus(trace::StatusCode::kOk);
      end_fd_span(fd, "close");
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(saved_errno));
      span->SetAttribute("errno", saved_errno);
    }
  }
  span->End();
  errno = saved_errno;
  g_in_hook = false;
  return rc;
}

// pthread_create()
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
  if (g_in_hook) return real_pcreate ? real_pcreate(thread, attr, start_routine, arg) : -1;
  g_in_hook = true;

  init_tracing_once();
  if (!real_pcreate) real_pcreate = resolve<PCreateFn>("pthread_create");

  // Capture current context
  auto captured = new ThreadStartCtx{
    start_routine,
    arg,
    ctx::RuntimeContext::GetCurrent()
  };

  auto tr = get_tracer();
  auto span = tr->StartSpan("thread.create");
  int rc = -1;
  {
    trace::Scope s(span);
    rc = real_pcreate ? real_pcreate(thread, attr, &thread_trampoline, captured) : -1;
    if (rc == 0) {
      span->SetStatus(trace::StatusCode::kOk);
    } else {
      span->SetStatus(trace::StatusCode::kError, std::strerror(errno));
      delete captured; // failure path cleanup
    }
  }
  span->End();
  g_in_hook = false;
  return rc;
}

} // extern "C"
