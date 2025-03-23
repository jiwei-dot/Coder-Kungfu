// Copyright (c) 2018-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <sys/sysinfo.h>
#include "dali_thread_pool.hpp"

namespace dali
{

    std::vector<std::string> string_split(const std::string &s, const char delim)
    {
        std::vector<std::string> ret;
        size_t pos = 0;
        while (pos != std::string::npos)
        {
            size_t newpos = s.find(delim, pos);
            ret.push_back(s.substr(pos, newpos - pos));
            if (newpos != std::string::npos)
                pos = newpos + 1;
            else
                pos = newpos;
        }
        return ret;
    }

    ThreadPool::ThreadPool(int num_thread, int device_id, bool set_affinity, const char *name)
        : threads_(num_thread), running_(true), work_complete_(true),
          started_(false), active_threads_(0), tl_errors_(num_thread)
    {
        // DALI_ENFORCE(num_thread > 0, "Thread pool must have non-zero size");

        for (size_t i = 0; i < num_thread; ++i)
        {
            threads_[i] = std::thread(std::bind(&ThreadPool::ThreadMain, this, i, device_id, set_affinity, "[DALI]"));
        }
    }

    ThreadPool::~ThreadPool()
    {
        WaitForWork(false);

        std::unique_lock<std::mutex> lock(mutex_);
        running_ = false;
        condition_.notify_all();
        lock.unlock();

        for (std::thread &thread : threads_)
        {
            thread.join();
        }
    }

    void ThreadPool::AddWork(Work work, int64_t priority, bool start_immediately)
    {
        bool started_before = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            work_queue_.push({priority, std::move(work)});
            work_complete_ = false;
            started_before = started_;
            started_ |= start_immediately;
        }

        if (started_)
        {
            if (!started_before)
                condition_.notify_all();
            else 
                condition_.notify_one();
        }
    }

    void ThreadPool::WaitForWork(bool checkForErrors)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        completed_.wait(lock, [this]
                        { return this->work_complete_; });
        started_ = false;
        if (checkForErrors)
        {
            for (size_t i = 0; i < threads_.size(); ++i)
            {
                if (!tl_errors_[i].empty())
                {
                    std::string error = std::string("Error in thread") + std::to_string(i) + ": " + tl_errors_[i].front();
                    tl_errors_[i].pop();
                    throw std::runtime_error(error);
                }
            }
        }
    }

    void ThreadPool::RunAll(bool wait)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            started_ = true;
        }

        condition_.notify_all();
        if (wait)
        {
            WaitForWork();
        }
    }

    int ThreadPool::NumThreads() const
    {
        return threads_.size();
    }

    std::vector<std::thread::id> ThreadPool::GetThreadIds() const
    {
        std::vector<std::thread::id> tids;
        tids.reserve(threads_.size());
        for (const auto &thread : threads_)
        {
            tids.emplace_back(thread.get_id());
        }
        return tids;
    }

    void ThreadPool::ThreadMain(int thread_id, int device_id, bool set_affinity,
                                const std::string &name)
    {
        pthread_setname_np(pthread_self(), name.c_str());
        try
        {
            if (set_affinity)
            {
                const char *env_affinity = std::getenv("DALI_AFFINITY_MASK");
                int core = -1;
                if (env_affinity)
                {
                    const std::vector<std::string> &vec = string_split(env_affinity, ',');
                    if ((size_t)thread_id < vec.size())
                    {
                        core = std::stoi(vec[thread_id]);
                    }
                    cpu_set_t requested_set;
                    CPU_ZERO(&requested_set);
                    if (core != -1)
                    {
                        size_t num_cpus = get_nprocs_conf();
                        if ((core >= 0) && ((size_t)(core) < num_cpus))
                        {
                            CPU_SET(core, &requested_set);
                            pthread_setaffinity_np(pthread_self(), sizeof(requested_set), &requested_set);
                        }
                    }
                }
            }
        }
        catch (std::exception &e)
        {
            tl_errors_[thread_id].push(e.what());
        }
        catch (...)
        {
            tl_errors_[thread_id].push("Caught unknown exception");
        }

        while (running_)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]
                            { return !running_ || (!work_queue_.empty() && started_); });

            if (!running_)
                break;

            Work work = std::move(work_queue_.top().second);
            work_queue_.pop();
            ++active_threads_;

            lock.unlock();

            try
            {
                work(thread_id);
            }
            catch (std::exception &e)
            {
                lock.lock();
                tl_errors_[thread_id].push(e.what());
                lock.unlock();
            }
            catch (...)
            {
                lock.lock();
                tl_errors_[thread_id].push("Caught unknown exception");
                lock.unlock();
            }

            lock.lock();
            --active_threads_;
            if (work_queue_.empty() && active_threads_ == 0)
            {
                work_complete_ = true;
                lock.unlock();
                completed_.notify_one();
            }
        }
    }

} // namespace dali