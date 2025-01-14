#pragma once
/*
 *  Copyright (C) 2024  Brett Terpstra
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BLT_GP_THREADING_H
#define BLT_GP_THREADING_H

#include <blt/std/types.h>
#include <blt/std/thread.h>
#include <thread>
#include <functional>
#include <atomic>

namespace blt::gp
{
    template <typename Parallel, typename Single = void>
    class task_t
    {
    public:
        task_t(Parallel parallel, Single single): parallel(parallel), single(single), requires_single_sync(true)
        {
        }

        explicit task_t(Parallel parallel): parallel(parallel), requires_single_sync(false)
        {
        }

        void call_parallel(size_t thread_index)
        {
            parallel(thread_index);
        }

        void call_single()
        {
            single();
        }
    private:
        Parallel parallel;
        Single single;
        bool explicit_sync_begin : 1 = true;
        bool explicit_sync_end : 1 = true;
        bool requires_single_sync : 1 = false;
    };

    template <typename... Tasks>
    class task_storage_t
    {
    };

    class thread_manager_t
    {
    public:
        explicit thread_manager_t(const size_t thread_count, const bool will_main_block = true): barrier(thread_count),
                                                                                                 will_main_block(will_main_block)
        {
            for (size_t i = 0; i < will_main_block ? thread_count - 1 : thread_count; ++i)
            {
                threads.emplace_back([i, this]()
                {
                    while (should_run)
                    {
                    }
                });
            }
        }

        ~thread_manager()
        {
            should_run = false;
            for (auto& thread : threads)
            {
                if (thread.joinable())
                    thread.join();
            }
        }

    private:
        [[nodiscard]] size_t thread_count() const
        {
            return will_main_block ? threads.size() + 1 : threads.size();
        }

        barrier barrier;
        std::atomic_bool should_run = true;
        bool will_main_block;
        std::vector<size_t> tasks;
        std::vector<std::thread> threads;
    };
}

#endif //BLT_GP_THREADING_H
