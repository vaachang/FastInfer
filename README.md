# MiniServe

MiniServe 是一个基于 **C++17** 实现的轻量级高并发模型推理服务，面向小型 ONNX 模型提供 HTTP 推理接口。

项目实现了从 **HTTP 请求接入 → 有界队列 → Dynamic Batching → ONNX Runtime 批量推理 → Promise/Future 结果回传** 的完整推理服务链路。

项目重点关注：

- C++ 多线程并发
- 有界队列与请求调度
- Dynamic Batching
- ONNX Runtime 推理
- Throughput / Latency Benchmark
- E2E / Queue Wait / Inference 延迟分解
- HTTP 端到端性能测试

---

## Architecture

```text
                         HTTP Client
                              │
                              │ POST /infer
                              ▼
                    ┌──────────────────┐
                    │    HTTP Server   │
                    │    cpp-httplib   │
                    └────────┬─────────┘
                             │
                             │ InferenceRequest
                             ▼
                    ┌──────────────────┐
                    │  Bounded Queue   │
                    │ thread-safe queue│
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │ Batch Scheduler  │
                    │ Dynamic Batching │
                    └────────┬─────────┘
                             │
                         Batch
                             │
                             ▼
                    ┌──────────────────┐
                    │  ONNX Runtime    │
                    │     Backend      │
                    └────────┬─────────┘
                             │
                       Batch Output
                             │
                             ▼
                    ┌──────────────────┐
                    │ Split Results    │
                    │ Promise/Future   │
                    └────────┬─────────┘
                             │
                             ▼
                       HTTP Response
````

---

## Features

* C++17 / CMake
* Thread-safe bounded blocking queue
* Concurrent request processing
* Dynamic Batching
* Maximum Batch Size control
* Maximum Batch Waiting Time control
* `std::promise` / `std::future` asynchronous result delivery
* ONNX Runtime C++ backend
* HTTP / JSON inference API
* Warmup mechanism
* Throughput Benchmark
* P50 / P95 / P99 latency statistics
* E2E / Queue Wait / Inference latency decomposition
* Bounded queue based overload protection
* Graceful shutdown

---

# Core Components

## 1. BoundedBlockingQueue

使用线程安全的有界阻塞队列作为请求入口。

主要功能：

* 限制内部请求队列长度
* 多线程安全 `push/pop`
* 队列为空时阻塞等待
* 支持超时等待
* 支持队列关闭
* 避免高负载情况下请求无限堆积

请求进入系统后首先被封装为：

```cpp
InferenceRequest
```

然后进入：

```text
HTTP Server
     │
     ▼
BoundedBlockingQueue
```

---

## 2. BatchScheduler

`BatchScheduler` 是 MiniServe 的核心调度模块。

调度器从请求队列中获取请求，并动态构建 Batch：

```text
                 Request Queue
                      │
                      ▼
              Get first request
                      │
                      ▼
                Start Batch
                      │
          ┌───────────┴───────────┐
          │                       │
   Batch Size reached       Timeout reached
          │                       │
          └───────────┬───────────┘
                      ▼
               Execute inference
```

调度策略由两个参数控制：

```cpp
max_batch_size
max_wait_time
```

例如：

```cpp
max_batch_size = 4;
max_wait_time = 5ms;
```

表示：

* 一个 Batch 最多包含 4 个请求
* 如果请求数量不足 4 个，则最多等待 5 ms
* 达到任一条件后立即执行推理

这样可以在吞吐量和请求延迟之间取得平衡。

---

# 3. ONNXRuntimeBackend

使用 ONNX Runtime C++ API 执行模型推理。

假设有 4 个请求：

```text
Request 1: [1, 2, 3]
Request 2: [4, 5, 6]
Request 3: [7, 8, 9]
Request 4: [10, 11, 12]
```

首先将多个请求拼接为连续内存：

```text
[1,2,3,4,5,6,7,8,9,10,11,12]
```

并构造：

```text
shape = [4, 3]
```

然后只执行一次 ONNX Runtime inference：

```text
4 requests
    │
    ▼
one batched inference
```

最后将 Batch 输出拆分：

```text
Request 1 -> [2,4,6]
Request 2 -> [8,10,12]
Request 3 -> [14,16,18]
Request 4 -> [20,22,24]
```

并通过对应请求的 `std::promise` 返回结果。

---

# HTTP API

## Health Check

### Request

```bash
curl http://127.0.0.1:8080/health
```

### Response

```json
{
  "status": "ok"
}
```

---

## Inference

### Request

```bash
curl -X POST http://127.0.0.1:8080/infer \
  -H "Content-Type: application/json" \
  -d '{"input":[1.0,2.0,3.0]}'
```

### Response

```json
{
  "output": [2.0,4.0,6.0],
  "request_id": 2
}
```

HTTP 请求完整经过：

```text
HTTP
  │
  ▼
JSON Parsing
  │
  ▼
InferenceRequest
  │
  ▼
Bounded Queue
  │
  ▼
Dynamic Batching
  │
  ▼
ONNX Runtime
  │
  ▼
Promise/Future
  │
  ▼
HTTP Response
```

---

# Build

## Requirements

* Linux
* C++17 compiler
* CMake >= 3.16
* ONNX Runtime
* cpp-httplib
* nlohmann/json

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

---

# Run

启动 MiniServe：

```bash
./build/minisrv
```

启动后：

```text
Model input: input
Input shape: -1 3
Feature size: 3
MiniServe started.
HTTP server listening on 0.0.0.0:8080
```

---

# Benchmark

MiniServe 提供独立 Benchmark 程序，用于测试 Dynamic Batching 在不同 Batch Size 下的性能。

## Usage

```bash
./build/benchmark \
    <batch_size> \
    <warmup_seconds> \
    <duration_seconds> \
    <rounds>
```

例如：

```bash
./build/benchmark 4 2 5 1
```

表示：

* Maximum Batch Size: 4
* Warmup: 2 seconds
* Benchmark Duration: 5 seconds
* Rounds: 1
* Concurrent Clients: 8

Benchmark 统计：

* Throughput
* E2E P50 / P95 / P99
* Queue Wait P50 / P95 / P99
* Inference P50 / P95 / P99
* Total Batches
* Average Batch Size
* Maximum Actual Batch Size

---

# Dynamic Batching Benchmark

测试模型为轻量级 ONNX 模型：

```text
Input Shape: [-1, 3]
```

测试环境：

```text
Concurrent Clients: 8
Warmup: 2 s
Duration: 5 s
Rounds: 1
```

测试结果：

| Max Batch Size |        Throughput |      E2E P50 |      E2E P95 |      E2E P99 | Avg Batch |
| -------------: | ----------------: | -----------: | -----------: | -----------: | --------: |
|              1 |      78,074 req/s |     0.081 ms |     0.186 ms |     0.304 ms |      1.00 |
|              2 |      80,138 req/s |     0.072 ms |     0.207 ms |     0.324 ms |      2.00 |
|              4 | **111,700 req/s** | **0.056 ms** | **0.121 ms** | **0.194 ms** |      4.00 |
|              8 |      45,409 req/s |     0.148 ms |     0.287 ms |     0.431 ms |      8.00 |

在当前测试环境和模型下：

```text
Batch Size = 4
```

获得最高吞吐量。

相较 Batch Size = 1：

```text
78,074 req/s
      ↓
111,700 req/s
```

吞吐提升约：

```text
43.1%
```

---

# Latency Decomposition

MiniServe 不仅统计端到端延迟，还对请求生命周期进行拆分：

```text
Submit
  │
  ▼
Queue Wait
  │
  ▼
Batch Start
  │
  ▼
Inference
  │
  ▼
Inference End
  │
  ▼
Response
```

Batch Size = 4 的一次测试结果：

```text
Throughput: 111,700 req/s
```

### E2E Latency

```text
P50: 0.056116 ms
P95: 0.121036 ms
P99: 0.194494 ms
```

### Queue Wait

```text
P50: 0.019685 ms
P95: 0.060016 ms
P99: 0.106390 ms
```

### Inference

```text
P50: 0.021267 ms
P95: 0.041635 ms
P99: 0.062510 ms
```

Batch statistics：

```text
Average Batch Size: 3.99996
Max Actual Batch Size: 4
```

Latency decomposition 可以帮助定位系统性能瓶颈，而不仅仅观察最终的 E2E latency。

---

# HTTP Benchmark

除了内部 Scheduler Benchmark，MiniServe 还提供 HTTP 端到端 Benchmark。

测试：

```text
Clients: 8
Duration: 5 s
```

结果：

```text
Requests: 73347
Completed: 73347
Errors: 0
Elapsed: 5.10186 s
Throughput: 14376.5 req/s
```

HTTP Latency：

```text
P50: 0.492006 ms
P95: 0.928110 ms
P99: 1.451890 ms
```

请求完整经过：

```text
HTTP
 ↓
JSON Parsing
 ↓
Request Queue
 ↓
Dynamic Batching
 ↓
ONNX Runtime
 ↓
Promise/Future
 ↓
HTTP Response
```

全部请求成功完成：

```text
Completed = Requests
Errors = 0
```

说明 HTTP 层、请求队列、Batch Scheduler、ONNX Runtime 和结果回传链路已经能够完整协同工作。

> 注意：HTTP Benchmark 的吞吐量明显低于内部 Scheduler Benchmark，主要用于衡量完整 HTTP 服务链路，而不是单纯测量推理调度器性能。

---

# Performance Analysis

测试结果表明：

```text
Batch 1 → 78.1k req/s
Batch 2 → 80.1k req/s
Batch 4 → 111.7k req/s
Batch 8 → 45.4k req/s
```

在当前 CPU + ONNX Runtime + 轻量级模型测试环境中，适当增加 Batch Size 能够减少单请求推理中的固定开销，从而提高整体吞吐量。

但 Batch Size 并非越大越好。

当 Batch Size 从 4 增加到 8 后：

```text
111.7k req/s
      ↓
45.4k req/s
```

吞吐量明显下降。

这说明 Dynamic Batching 存在明显的最优区间，其性能受到以下因素共同影响：

```text
Batch Size
    │
    ├── Scheduling Overhead
    │
    ├── Memory Access
    │
    ├── Runtime Overhead
    │
    └── Model Computation
```

因此推理服务中的 Batch Size 需要根据模型和硬件环境进行实际 Benchmark，而不能简单认为 Batch 越大性能越好。

---

# Promise / Future Result Delivery

每个请求包含独立的：

```cpp
std::promise<InferenceResult>
```

HTTP Handler 获取对应的 Future：

```cpp
auto future =
    inference_request->promise.get_future();
```

Scheduler 完成 Batch 推理后，将结果写回对应请求：

```cpp
request->promise.set_value(
    std::move(result)
);
```

HTTP Handler 最终通过：

```cpp
auto result = future.get();
```

获取对应结果。

这种设计将：

```text
Request Handling
```

与：

```text
Batch Scheduling + Model Inference
```

进行解耦。

---

# Overload Protection

MiniServe 使用有界请求队列：

```text
HTTP Requests
     │
     ▼
┌──────────────┐
│ Bounded Queue│
│    size=16   │
└──────────────┘
```

当队列达到容量限制时，新请求不会无限制占用内存，而是受到队列容量约束。

HTTP 服务同时提供过载响应：

```http
503 Service Unavailable
```

从而形成基础的 overload protection。

---

# Project Structure

```text
MiniServe/
├── include/
│   └── minisrv/
│       ├── core/
│       │   └── bounded_queue.h
│       │
│       ├── runtime/
│       │   ├── batch.h
│       │   ├── batch_scheduler.h
│       │   ├── inference_backend.h
│       │   ├── inference_request.h
│       │   └── onnxruntime_backend.h
│       │
│       └── server/
│           └── http_server.h
│
├── src/
│   ├── core/
│   │   └── thread_pool.cpp
│   │
│   ├── runtime/
│   │   ├── batch.cpp
│   │   ├── batch_scheduler.cpp
│   │   ├── fake_backend.cpp
│   │   ├── onnxruntime_backend.cpp
│   │   ├── ort_test.cpp
│   │   └── ort_infer_test.cpp
│   │
│   ├── server/
│   │   └── http_server.cpp
│   │
│   └── main.cpp
│
├── tests/
│   ├── benchmark.cpp
│   └── http_benchmark.cpp
│
├── models/
│   └── mul2.onnx
│
├── CMakeLists.txt
└── README.md
```

---

# Technical Highlights

## 1. Dynamic Batching

通过：

```text
max_batch_size
max_wait_time
```

动态聚合并发请求，在提高吞吐量的同时控制请求等待时间。

---

## 2. Bounded Queue

使用有界阻塞队列：

```text
Thread-safe
+
Bounded
+
Blocking
+
Graceful Close
```

限制系统内部请求积压，避免高负载情况下无限制占用内存。

---

## 3. Batch Inference

多个独立请求被聚合为一个 Tensor Batch：

```text
N × FeatureSize
```

从而通过一次 ONNX Runtime `Run()` 完成多个请求的推理。

---

## 4. Promise / Future

每个请求拥有独立的 Future/Promise 状态，实现：

```text
HTTP Thread
     │
     │ future.get()
     │
     ▼
Batch Scheduler
     │
     │ promise.set_value()
     ▼
HTTP Thread
```

实现请求接入与推理解耦。

---

## 5. Latency Decomposition

将 E2E Latency 拆分为：

```text
Queue Wait
+
Inference
+
Response Path
```

从而能够进一步定位性能瓶颈。

---

## 6. Benchmark

提供独立 Benchmark 程序，通过多客户端持续发送请求，并统计：

```text
Throughput
P50
P95
P99
Batch Size
Queue Wait
Inference Time
```

同时支持 Warmup 和多轮测试，降低模型初始化和运行状态对测试结果的影响。

---

# Future Work

当前版本主要面向 CPU + ONNX Runtime + 小型模型。

后续可以进一步扩展：

* CUDA Execution Provider
* GPU Memory Management
* 多模型管理
* 模型热加载
* OpenAI-compatible API
* 请求超时与取消
* 更完善的 overload control
* Multi-worker
* Multi-GPU
* 性能 profiling
* 与 vLLM / Triton 等推理框架进行 benchmark 对比

---

# Tech Stack

```text
C++17
CMake
Linux
ONNX Runtime
cpp-httplib
nlohmann/json

std::thread
std::atomic
std::mutex
std::condition_variable
std::future
std::promise
```

---

# License

For learning and research purposes.
