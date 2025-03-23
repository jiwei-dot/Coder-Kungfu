// Copyright (c) 2017-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DALI_PIPELINE_UTIL_THREAD_POOL_HPP_
#define DALI_PIPELINE_UTIL_THREAD_POOL_HPP_

#include <functional>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#define DISABLE_COPY_MOVE_ASSIGN(name)      \
    name(const name &) = delete;            \
    name(name &&) = delete;                 \
    name &operator=(const name &) = delete; \
    name &operator=(name &&) = delete

namespace dali
{

    class ThreadPool
    {
    public:
        using Work = std::function<void(int)>;

        ThreadPool(int num_thread, int device_id, bool set_affinity, const char *name);

        ThreadPool(int num_thread, int device_id, bool set_affinity, const std::string &name)
            : ThreadPool(num_thread, device_id, set_affinity, name.c_str())
        {
        }

        ~ThreadPool();

        void AddWork(Work work, int64_t priority = 0, bool start_immediately = false);

        void RunAll(bool wait = true);

        void WaitForWork(bool checkForErrors = true);

        int NumThreads() const;

        std::vector<std::thread::id> GetThreadIds() const;

        DISABLE_COPY_MOVE_ASSIGN(ThreadPool);

    private:
        void ThreadMain(int thread_id, int device_id, bool set_affinity, const std::string &name);

    private:
        std::vector<std::thread> threads_;

        using PrioritizedWork = std::pair<int64_t, Work>;

        struct SortByPriority
        {
            bool operator() (const PrioritizedWork &a, const PrioritizedWork &b)
            {
                return a.first < b.first;
            }
        };

        std::priority_queue<PrioritizedWork, std::vector<PrioritizedWork>, SortByPriority> work_queue_;

        bool running_;
        bool work_complete_;
        bool started_;
        int active_threads_;
        std::mutex mutex_;
        std::condition_variable condition_;
        std::condition_variable completed_;

        std::vector<std::queue<std::string>> tl_errors_;
    };

} // namespace dali

#endif // DALI_PIPELINE_UTIL_THREAD_POOL_HPP_