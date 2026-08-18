# 编译构建
cmake -S . -B build

cmake --build build

行数统计

cloc . --exclude-dir=build,.cache

# Dynamic Batching Benchmark

在 8 并发客户端、CPU 推理环境下，对不同最大 Batch Size 进行 5 秒预热、10 秒正式测试并重复 3 次。测试结果表明，Batch Size 从 1 增加至 4 时吞吐持续提升，在 Batch=4 时达到约 98.7K req/s，相比单请求推理提升约 24.8%；继续增大至 Batch=8 后，由于轻量模型下调度与 Runtime 固定开销占比增加，吞吐下降至约 43.8K req/s。
