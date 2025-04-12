/*
 *  <Short Description>
 *  Copyright (C) 2025  Brett Terpstra
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
#include <blt/gp/sync.h>
#include <thread>
#include <atomic>
#include <blt/std/time.h>

namespace blt::gp
{
    struct global_sync_state_t
    {
        std::vector<sync_t*> syncs;
        std::mutex mutex;
        std::thread* thread = nullptr;
        std::atomic_bool should_run = true;
        std::condition_variable condition_variable;

        void add(sync_t* sync)
        {
            if (thread == nullptr)
            {
                thread = new std::thread([this]()
                {
                    while (should_run)
                    {
                        std::unique_lock lock(mutex);
                        condition_variable.wait_for(lock, std::chrono::milliseconds(100));
                        const auto current_time = system::getCurrentTimeMilliseconds();
                        for (const auto& sync : syncs)
                            sync->trigger(current_time);
                    }
                });
            }
            std::scoped_lock lock(mutex);
            syncs.push_back(sync);
        }

        void remove(const sync_t* sync)
        {
            if (thread == nullptr)
            {
                BLT_WARN("Tried to remove sync from global sync state, but no thread was running");
                return;
            }
            std::unique_lock lock(mutex);
            const auto iter = std::find(syncs.begin(), syncs.end(), sync);
            std::iter_swap(iter, syncs.end() - 1);
            syncs.pop_back();
            if (syncs.empty())
            {
                lock.unlock();
                should_run = false;
                condition_variable.notify_all();
                thread->join();
                delete thread;
                thread = nullptr;
            }
        }
    };

    global_sync_state_t& get_state()
    {
        static global_sync_state_t state;
        return state;
    }

    sync_t::sync_t(gp_program& program): m_program(&program)
    {
        get_state().add(this);
    }

    void sync_t::trigger(const u64 current_time)
    {
        if ((m_timer_seconds && (current_time % *m_timer_seconds == 0)) || (m_generations && (current_time % *m_generations == 0)))
        {
            
        }
    }

    sync_t::~sync_t()
    {
        get_state().remove(this);
    }
}
