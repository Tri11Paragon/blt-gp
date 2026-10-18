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
#include <type_traits>

namespace blt::gp
{
    namespace detail
    {
        struct empty_callable
        {
            void operator()() const
            {
            }
        };
    }

    template <typename EnumId>
    class task_builder_t;

    template <typename EnumId, typename Parallel, typename Single = detail::empty_callable>
    class task_t
    {
        static_assert(std::is_enum_v<EnumId>, "Enum ID must be of enum type!");
        static_assert(std::is_invocable_v<Parallel, int>, "Parallel must be invocable with exactly one argument (thread index)");
        static_assert(std::is_invocable_v<Single>, "Single must be invocable with no arguments");

        friend task_builder_t<EnumId>;

    public:
        task_t(const EnumId task_id, const Parallel& parallel, const Single& single): parallel(std::forward<Parallel>(parallel)),
                                                                                      single(std::forward<Single>(single)),
                                                                                      requires_single_sync(true), task_id(task_id)
        {
        }

        explicit task_t(const EnumId task_id, const Parallel& parallel): parallel(std::forward<Parallel>(parallel)), single(detail::empty_callable{}),
                                                                         task_id(task_id)
        {
        }

        void call_parallel(size_t thread_index) const
        {
            parallel(thread_index);
        }

        void call_single() const
        {
            single();
        }

        [[nodiscard]] EnumId get_task_id() const
        {
            return task_id;
        }

    private:
        const Parallel& parallel;
        const Single& single;
        bool requires_single_sync = false;
        EnumId task_id;
    };

    template <typename EnumId, typename Parallel, typename Single = detail::empty_callable>
    task_t(EnumId, Parallel, Single) -> task_t<EnumId, Parallel, Single>;

    template <typename EnumId>
    class task_builder_t
    {
        static_assert(std::is_enum_v<EnumId>, "Enum ID must be of enum type!");
        using EnumInt = std::underlying_type_t<EnumId>;

    public:
        task_builder_t() = default;

        template <typename... Tasks>
        static std::function<void(barrier_t&, EnumId, size_t)> make_callable(Tasks&&... tasks)
        {
            return [&tasks...](barrier_t& sync_barrier, EnumId task, size_t thread_index)
            {
                call_jmp_table(sync_barrier, task, thread_index, tasks...);
            };
        }

    private:
        template <typename Task>
        static void execute(barrier_t& sync_barrier, const size_t thread_index, Task&& task)
        {
            // sync_barrier.wait();
            if (task.requires_single_sync)
            {
                if (thread_index == 0)
                    task.call_single();
                sync_barrier.wait();
            }
            task.call_parallel(thread_index);
            // sync_barrier.wait();
        }

        template <typename Task>
        static bool call(barrier_t& sync_barrier, const EnumId current_task, const size_t thread_index, Task&& task)
        {
            if (static_cast<EnumInt>(current_task) == static_cast<EnumInt>(task.get_task_id()))
            {
                execute(sync_barrier, thread_index, std::forward<Task>(task));
                return false;
            }
            return true;
        }

        template <typename... Tasks>
        static void call_jmp_table(barrier_t& sync_barrier, const EnumId current_task, const size_t thread_index, Tasks&&... tasks)
        {
            if (static_cast<EnumInt>(current_task) >= sizeof...(tasks))
                BLT_UNREACHABLE;
            (call(sync_barrier, current_task, thread_index, std::forward<Tasks>(tasks)) && ...);
        }
    };

    template <typename EnumId>
    class thread_manager_t
    {
        static_assert(std::is_enum_v<EnumId>, "Enum ID must be of enum type!");

    public:
        explicit thread_manager_t(const size_t thread_count, std::function<void(barrier_t&, EnumId, size_t)> task_func,
                                  const bool will_main_block = true): barrier(thread_count), will_main_block(will_main_block)
        {
            thread_callable = [this, task_func = std::move(task_func)](const size_t thread_index)
            {
                while (should_run)
                {
                    barrier.wait();
                    if (tasks_remaining > 0)
                        task_func(barrier, tasks.back(), thread_index);
                    barrier.wait();
                    if (thread_index == 0)
                    {
                        if (this->will_main_block)
                        {
                            tasks.pop_back();
                            --tasks_remaining;
                        }
                        else
                        {
                            std::scoped_lock lock{task_lock};
                            tasks.pop_back();
                            --tasks_remaining;
                        }
                    }
                }
            };
            for (size_t i = 0; i < will_main_block ? thread_count - 1 : thread_count; ++i)
                threads.emplace_back(thread_callable, will_main_block ? i + 1 : i);
        }

        void execute() const
        {
            BLT_ASSERT(will_main_block &&
                "You attempted to call this function without specifying that "
                "you want an external blocking thread (try passing will_main_block = true)");
            thread_callable(0);
        }

        void add_task(EnumId task)
        {
            if (will_main_block)
            {
                tasks.push_back(task);
                ++tasks_remaining;
            }
            else
            {
                std::scoped_lock lock(task_lock);
                tasks.push_back(task);
                ++tasks_remaining;
            }
        }

        bool has_tasks_left()
        {
            if (will_main_block)
            {
                return !tasks.empty();
            }
            std::scoped_lock lock{task_lock};
            return tasks.empty();
        }

        ~thread_manager_t()
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

        blt::barrier_t barrier;
        std::atomic_bool should_run = true;
        bool will_main_block;
        std::vector<EnumId> tasks;
        std::atomic_uint64_t tasks_remaining = 0;
        std::vector<std::thread> threads;
        std::mutex task_lock;

        std::function<void(size_t)> thread_callable;
    };
}

#endif //BLT_GP_THREADING_H
