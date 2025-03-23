#include <benchmark/benchmark.h>

#include "mutex_queue.hpp"
#include "spsc_queue.hpp"

// 通用测试模板
template <typename QueueType>
static void BM_QueueThroughput(benchmark::State& state) {
  QueueType queue;
  constexpr int kBatchSize = 1000000;
  std::atomic<bool> producer_done{false};
  std::atomic<int> total_consumed{0};

  // 消费者线程
  auto consumer = [&] {
    T value;
    while (!producer_done || (total_consumed.load() < kBatchSize)) {
      if (queue.Pop(value)) {
        benchmark::DoNotOptimize(value);
        total_consumed.fetch_add(1);
      }
    }
  };

  // 生产者线程
  auto producer = [&] {
    for (int i = 0; i < kBatchSize;) {
      if (queue.Push(T{i})) {
        ++i;
      }
    }
    producer_done = true;
  };

  std::thread consumer_thread(consumer);
  std::thread producer_thread(producer);

  // 基准测试循环
  for (auto _ : state) {
    // 等待测试完成
    producer_thread.join();
    consumer_thread.join();
  }

  state.SetItemsProcessed(kBatchSize);
}

// 注册测试用例
BENCHMARK_TEMPLATE(BM_QueueThroughput, SPSCQueue<int, 1024>)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_QueueThroughput, MutexQueue<int, 1024>)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

BENCHMARK_MAIN();