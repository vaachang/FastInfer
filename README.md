# 编译构建
cmake -S . -B build

cmake --build build

行数统计

cloc . --exclude-dir=build,.cache

# Dynamic Batching Benchmark

在 8 并发客户端、CPU 推理环境下，对不同最大 Batch Size 进行 5 秒预热、10 秒正式测试并重复 3 次。测试结果表明，Batch Size 从 1 增加至 4 时吞吐持续提升，在 Batch=4 时达到约 98.7K req/s，相比单请求推理提升约 24.8%；继续增大至 Batch=8 后，由于轻量模型下调度与 Runtime 固定开销占比增加，吞吐下降至约 43.8K req/s。

# benchmark

Max Batch	Throughput	E2E P99	Queue P99	Inference P99	Avg Batch
1	78,074 req/s	0.304 ms	0.230 ms	0.040 ms	1.00
2	80,138 req/s	0.324 ms	0.225 ms	0.067 ms	2.00
4	111,700 req/s	0.194 ms	0.106 ms	0.063 ms	4.00
8	45,409 req/s	0.431 ms	0.141 ms	0.190 ms	8.00

Batch Size = 4 是当前测试环境下的最佳点。

# MiniServe

MiniServe 是一个基于 **C++17** 实现的轻量级高并发模型推理服务，面向小型 ONNX 模型提供 HTTP 推理接口。

项目重点实现了从 HTTP 请求接入、并发请求排队、动态 Batch 聚合，到 ONNX Runtime 批量推理和结果回传的完整推理服务链路。

## Features

- C++17 / CMake
- 基于有界阻塞队列的请求调度
- 多线程并发请求处理
- Dynamic Batching
- Batch Size 与最大等待时间控制
- `std::promise` / `std::future` 异步结果回传
- ONNX Runtime 推理后端
- HTTP / JSON API
- Warmup 机制
- Throughput Benchmark
- P50 / P95 / P99 延迟统计
- E2E / Queue Wait / Inference 分阶段延迟统计
- 请求过载保护
- Graceful Shutdown

## Architecture

```text
                    HTTP Client
                        │
                        │ POST /infer
                        ▼
                ┌───────────────┐
                │  HTTP Server  │
                │ cpp-httplib   │
                └───────┬───────┘
                        │
                        │ InferenceRequest
                        ▼
                ┌───────────────┐
                │ Bounded Queue │
                │ 线程安全队列   │
                └───────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │Batch Scheduler│
                │Dynamic Batching│
                └───────┬───────┘
                        │
                  Batch Request
                        │
                        ▼
                ┌───────────────┐
                │ ONNX Runtime  │
                │    Backend    │
                └───────┬───────┘
                        │
                  Batch Output
                        │
                        ▼
                ┌───────────────┐
                │ Split Results │
                │ Promise/Future│
                └───────┬───────┘
                        │
                        ▼
                  HTTP Response
                  Core Components
                  BoundedBlockingQueue
                  
                  使用线程安全的有界阻塞队列作为请求入口。
                  
                  主要作用：
                  
                  限制请求队列长度
                  多线程安全 push/pop
                  队列为空时阻塞等待
                  队列关闭时支持退出
                  
                  通过有界队列避免请求无限堆积，并为服务提供基础的过载保护能力。
                  
                  BatchScheduler
                  
                  BatchScheduler 是 MiniServe 的核心调度模块。
                  
                  调度器从请求队列中获取请求：
                  
                  等待第一个请求
                  将请求加入当前 Batch
                  在最大 Batch Size 限制下继续聚合请求
                  达到 Batch Size 或最大等待时间后提交推理
                  将推理结果分别返回给原始请求
                  
                  核心参数：
                  
                  max_batch_size
                  max_wait_time
                  
                  例如：
                  
                  max_batch_size = 4
                  max_wait_time  = 5 ms
                  
                  表示调度器最多聚合 4 个请求，同时避免请求因为等待 Batch 而产生过高延迟。
                  
                  ONNXRuntimeBackend
                  
                  使用 ONNX Runtime C++ API 实现模型推理。
                  
                  对于一个 Batch：
                  
                  Request 1: [1, 2, 3]
                  Request 2: [4, 5, 6]
                  Request 3: [7, 8, 9]
                  Request 4: [10, 11, 12]
                  
                  首先将输入拼接为连续内存：
                  
                  [1,2,3,4,5,6,7,8,9,10,11,12]
                  
                  然后构造：
                  
                  shape = [4, 3]
                  
                  只执行一次 ONNX Runtime inference。
                  
                  最后将 Batch 输出拆分回各个请求：
                  
                  Request 1 -> [2,4,6]
                  Request 2 -> [8,10,12]
                  Request 3 -> [14,16,18]
                  Request 4 -> [20,22,24]
                  HTTP API
                  Health Check
                  GET /health
                  
                  Example:
                  
                  curl http://127.0.0.1:8080/health
                  
                  Response:
                  
                  {
                    "status": "ok"
                  }
                  Inference
                  POST /infer
                  Content-Type: application/json
                  
                  Request:
                  
                  curl -X POST http://127.0.0.1:8080/infer \
                    -H "Content-Type: application/json" \
                    -d '{"input":[1.0,2.0,3.0]}'
                  
                  Response:
                  
                  {
                    "output": [2.0,4.0,6.0],
                    "request_id": 2
                  }
                  Build
                  
                  Requirements:
                  
                  Linux
                  C++17 compiler
                  CMake >= 3.16
                  ONNX Runtime
                  cpp-httplib
                  nlohmann/json
                  
                  Build:
                  
                  cmake -S . -B build
                  cmake --build build -j
                  Run
                  
                  Start the server:
                  
                  ./build/minisrv
                  
                  Expected output:
                  
                  Model input: input
                  Input shape: -1 3
                  Feature size: 3
                  MiniServe started.
                  HTTP server listening on 0.0.0.0:8080
                  Benchmark
                  
                  MiniServe 提供独立 Benchmark 程序，用于测试不同 Batch Size 下的吞吐量和延迟。
                  
                  参数：
                  
                  ./build/benchmark <batch_size> <warmup_seconds> <duration_seconds> <rounds>
                  
                  例如：
                  
                  ./build/benchmark 4 2 5 1
                  
                  Benchmark 使用：
                  
                  8 个并发客户端
                  2 秒 Warmup
                  5 秒正式测试
                  1 个 Round
                  
                  同时统计：
                  
                  Requests
                  Completed Requests
                  Throughput
                  E2E P50/P95/P99
                  Queue Wait P50/P95/P99
                  Inference P50/P95/P99
                  Total Batches
                  Average Batch Size
                  Maximum Actual Batch Size
                  Benchmark Results
                  
                  测试模型为轻量级 ONNX 模型，输入 Shape：
                  
                  [-1, 3]
                  
                  8 个并发客户端，Warmup 2 秒，测试 5 秒。
                  
                  Batch Size	Throughput	E2E P50	E2E P95	E2E P99
                  1	78,074 req/s	0.081 ms	0.186 ms	0.304 ms
                  2	80,138 req/s	0.072 ms	0.207 ms	0.324 ms
                  4	111,700 req/s	0.056 ms	0.121 ms	0.194 ms
                  8	45,409 req/s	0.148 ms	0.287 ms	0.431 ms
                  
                  在当前测试环境和模型下，Batch Size = 4 获得最高吞吐量。
                  
                  Batch Size = 4
                  
                  一次测试结果：
                  
                  Requests: 559042
                  Completed: 559042
                  Elapsed: 5.00486 s
                  Throughput: 111700 req/s
                  
                  
                  E2E P50: 0.056116 ms
                  E2E P95: 0.121036 ms
                  E2E P99: 0.194494 ms
                  
                  
                  Queue Wait P50: 0.019685 ms
                  Queue Wait P95: 0.060016 ms
                  Queue Wait P99: 0.106390 ms
                  
                  
                  Inference P50: 0.021267 ms
                  Inference P95: 0.041635 ms
                  Inference P99: 0.062510 ms
                  
                  
                  Average Batch Size: 3.99996
                  Max Actual Batch Size: 4
                  HTTP Benchmark
                  
                  HTTP 端到端测试：
                  
                  Clients: 8
                  Duration: 5 s
                  
                  
                  Requests: 73347
                  Completed: 73347
                  Errors: 0
                  Elapsed: 5.10186 s
                  Throughput: 14376.5 req/s
                  
                  
                  P50: 0.492006 ms
                  P95: 0.928110 ms
                  P99: 1.451890 ms
                  
                  测试表明 HTTP 请求可以完整经过：
                  
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
                  
                  并成功完成全部请求，未出现错误请求。
                  
                  Performance Analysis
                  
                  Batching 并不是 Batch Size 越大越好。
                  
                  在当前 CPU + ONNX Runtime 测试环境中：
                  
                  Batch 1 → 78.1k req/s
                  Batch 2 → 80.1k req/s
                  Batch 4 → 111.7k req/s
                  Batch 8 → 45.4k req/s
                  
                  适当增加 Batch Size 可以降低单请求推理的固定开销，提高计算资源利用率。
                  
                  但是 Batch Size 继续增大后，单次 inference 的计算和内存访问成本增加，同时可能产生更高的调度和处理开销，因此吞吐量反而下降。
                  
                  这体现了推理服务中典型的：
                  
                  Throughput ↔ Latency
                  Batch Size ↔ Scheduling Overhead
                  
                  之间的 trade-off。
                  
                  Project Structure
                  MiniServe/
                  ├── include/
                  │   └── minisrv/
                  │       ├── core/
                  │       │   └── bounded_queue.h
                  │       ├── runtime/
                  │       │   ├── batch.h
                  │       │   ├── batch_scheduler.h
                  │       │   ├── inference_backend.h
                  │       │   ├── inference_request.h
                  │       │   └── onnxruntime_backend.h
                  │       └── server/
                  │           └── http_server.h
                  │
                  ├── src/
                  │   ├── core/
                  │   │   └── thread_pool.cpp
                  │   ├── runtime/
                  │   │   ├── batch.cpp
                  │   │   ├── batch_scheduler.cpp
                  │   │   ├── fake_backend.cpp
                  │   │   ├── onnxruntime_backend.cpp
                  │   │   ├── ort_test.cpp
                  │   │   └── ort_infer_test.cpp
                  │   ├── server/
                  │   │   └── http_server.cpp
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
                  Technical Highlights
                  1. Dynamic Batching
                  
                  通过最大 Batch Size 和最大等待时间动态聚合并发请求，在提高吞吐量的同时控制请求等待时间。
                  
                  2. Bounded Queue
                  
                  采用有界队列限制系统内部积压请求数量，避免高负载下无限制占用内存。
                  
                  3. Promise/Future
                  
                  每个 HTTP 请求对应独立的 InferenceRequest 和 std::promise。
                  
                  Scheduler 完成 Batch 推理后：
                  
                  request->promise.set_value(result);
                  
                  HTTP Handler 通过：
                  
                  auto result = future.get();
                  
                  等待对应请求的结果。
                  
                  这种设计将：
                  
                  请求接入
                  
                  与：
                  
                  Batch 调度 + 模型推理
                  
                  解耦。
                  
                  4. Latency Decomposition
                  
                  Benchmark 不仅统计端到端延迟，还进一步记录：
                  
                  Submit
                    ↓
                  Queue Wait
                    ↓
                  Batch Start
                    ↓
                  Inference
                    ↓
                  Inference End
                    ↓
                  Response
                  
                  从而可以定位性能瓶颈，而不是只观察一个最终 latency 数字。
                  
                  Future Work
                  
                  后续可以继续扩展：
                  
                  CUDA / GPU Execution Provider
                  GPU Memory Management
                  多模型管理
                  模型热加载
                  OpenAI-compatible API
                  更完善的 overload control
                  请求超时与取消
                  多 Worker / 多 GPU
                  更完善的性能 profiling
                  与其他推理框架进行 benchmark 对比
                  Tech Stack
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
                  std::future / std::promise
                  License
                  
                  For learning and research purposes.
