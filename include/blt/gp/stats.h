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

#ifndef BLT_GP_STATS_H
#define BLT_GP_STATS_H

#include <blt/std/types.h>
#include <blt/std/hashmap.h>
#include <blt/std/assert.h>
#include <thread>
#include <cmath>
#include <string>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace blt::gp
{
    class allocation_tracker_t
    {
        public:
            class tl_t
            {
                    friend allocation_tracker_t;
                public:
                    [[nodiscard]] blt::u64 getAllocations() const
                    {
                        return get_map(allocations);
                    }
                    
                    [[nodiscard]] blt::u64 getDeallocations() const
                    {
                        return get_map(deallocations);
                    }
                    
                    [[nodiscard]] blt::u64 getAllocatedBytes() const
                    {
                        return get_map(allocated_bytes);
                    }
                    
                    [[nodiscard]] blt::u64 getDeallocatedBytes() const
                    {
                        return get_map(deallocated_bytes);
                    }
                    
                    [[nodiscard]] blt::u64 getAllocationDifference() const
                    {
                        return std::abs(static_cast<blt::i64>(getAllocations()) - static_cast<blt::i64>(getDeallocations()));
                    }
                    
                    [[nodiscard]] blt::u64 getCurrentlyAllocatedBytes() const
                    {
                        return getAllocatedBytes() - getDeallocatedBytes();
                    }
                
                private:
                    blt::hashmap_t<std::thread::id, std::unique_ptr<blt::u64>> allocations;
                    blt::hashmap_t<std::thread::id, std::unique_ptr<blt::u64>> deallocations;
                    blt::hashmap_t<std::thread::id, std::unique_ptr<blt::u64>> allocated_bytes;
                    blt::hashmap_t<std::thread::id, std::unique_ptr<blt::u64>> deallocated_bytes;
                    
                    std::mutex mutex;
                    std::condition_variable var;
            };
            
            struct allocation_data_t
            {
                blt::u64 start_allocations = 0;
                blt::u64 start_deallocations = 0;
                blt::u64 start_allocated_bytes = 0;
                blt::u64 start_deallocated_bytes = 0;
                
                blt::u64 end_allocations = 0;
                blt::u64 end_deallocations = 0;
                blt::u64 end_allocated_bytes = 0;
                blt::u64 end_deallocated_bytes = 0;
                
                [[nodiscard]] blt::u64 getAllocationDifference() const
                {
                    return end_allocations - start_allocations;
                }
                
                [[nodiscard]] blt::u64 getDeallocationDifference() const
                {
                    return end_deallocations - start_deallocations;
                }
                
                [[nodiscard]] blt::u64 getAllocatedByteDifference() const
                {
                    return end_allocated_bytes - start_allocated_bytes;
                }
                
                [[nodiscard]] blt::u64 getDeallocatedByteDifference() const
                {
                    return end_deallocated_bytes - start_deallocated_bytes;
                }
                
                void pretty_print(const std::string& name) const;
            };
            
            void reserve()
            {
                {
                    std::scoped_lock lock(tl.mutex);
                    tl.allocations.insert({std::this_thread::get_id(), std::make_unique<blt::u64>()});
                    tl.deallocations.insert({std::this_thread::get_id(), std::make_unique<blt::u64>()});
                    tl.allocated_bytes.insert({std::this_thread::get_id(), std::make_unique<blt::u64>()});
                    tl.deallocated_bytes.insert({std::this_thread::get_id(), std::make_unique<blt::u64>()});
                }
                tl.var.notify_all();
            }
            
            blt::size_t reserved_threads()
            {
                return tl.allocations.size();
            }
            
            void await_thread_loading_complete(blt::u64 required_threads)
            {
                std::unique_lock lock(tl.mutex);
                tl.var.wait(lock, [this, required_threads]() {
                    return reserved_threads() == required_threads;
                });
            }
            
            void allocate(blt::size_t bytes)
            {
                allocations++;
                allocated_bytes += bytes;
                
                auto diff = getCurrentlyAllocatedBytes();
                auto atomic_val = peak_allocated_bytes.load(std::memory_order_relaxed);
                while (diff > atomic_val &&
                       !peak_allocated_bytes.compare_exchange_weak(atomic_val, diff, std::memory_order_relaxed, std::memory_order_relaxed));
                
                add_map(tl.allocations, 1);
                add_map(tl.allocated_bytes, bytes);
            }
            
            void deallocate(blt::size_t bytes)
            {
                deallocations++;
                deallocated_bytes += bytes;
                add_map(tl.deallocations, 1);
                add_map(tl.deallocated_bytes, bytes);
            }
            
            [[nodiscard]] blt::u64 getAllocations() const
            {
                return allocations;
            }
            
            [[nodiscard]] blt::u64 getDeallocations() const
            {
                return deallocations;
            }
            
            [[nodiscard]] blt::u64 getAllocatedBytes() const
            {
                return allocated_bytes;
            }
            
            [[nodiscard]] blt::u64 getDeallocatedBytes() const
            {
                return deallocated_bytes;
            }
            
            [[nodiscard]] blt::u64 getAllocationDifference() const
            {
                return std::abs(static_cast<blt::i64>(getAllocations()) - static_cast<blt::i64>(getDeallocations()));
            }
            
            [[nodiscard]] blt::u64 getCurrentlyAllocatedBytes() const
            {
                return getAllocatedBytes() - getDeallocatedBytes();
            }
            
            [[nodiscard]] blt::u64 getPeakAllocatedBytes() const
            {
                return peak_allocated_bytes;
            }
            
            allocation_tracker_t::tl_t& get_thread_local()
            {
                return tl;
            }
            
            [[nodiscard]] allocation_data_t start_measurement() const
            {
                allocation_data_t data{};
                data.start_allocations = getAllocations();
                data.start_deallocations = getDeallocations();
                data.start_allocated_bytes = getAllocatedBytes();
                data.start_deallocated_bytes = getDeallocatedBytes();
                return data;
            }
            
            [[nodiscard]] allocation_data_t start_measurement_thread_local() const
            {
                allocation_data_t data{};
                data.start_allocations = tl.getAllocations();
                data.start_deallocations = tl.getDeallocations();
                data.start_allocated_bytes = tl.getAllocatedBytes();
                data.start_deallocated_bytes = tl.getDeallocatedBytes();
                return data;
            }
            
            void stop_measurement(allocation_data_t& data) const
            {
                data.end_allocations = getAllocations();
                data.end_deallocations = getDeallocations();
                data.end_allocated_bytes = getAllocatedBytes();
                data.end_deallocated_bytes = getDeallocatedBytes();
            }
            
            void stop_measurement_thread_local(allocation_data_t& data) const
            {
                data.end_allocations = tl.getAllocations();
                data.end_deallocations = tl.getDeallocations();
                data.end_allocated_bytes = tl.getAllocatedBytes();
                data.end_deallocated_bytes = tl.getDeallocatedBytes();
            }
        
        private:
            static void add_map(blt::hashmap_t<std::thread::id, std::unique_ptr<blt::u64>>& map, blt::u64 value)
            {
                auto l = map.find(std::this_thread::get_id());
                if (l == map.end())
                    BLT_ABORT("Thread doesn't exist inside this map!");
                auto& v = *l->second;
                v += value;
            }
            
            static blt::u64 get_map(const blt::hashmap_t<std::thread::id, std::unique_ptr<blt::u64>>& map)
            {
                auto l = map.find(std::this_thread::get_id());
                if (l == map.end())
                    BLT_ABORT("Thread doesn't exist inside this map!");
                return *l->second;
            }
            
            tl_t tl;
            
            std::atomic_uint64_t allocations = 0;
            std::atomic_uint64_t deallocations = 0;
            std::atomic_uint64_t allocated_bytes = 0;
            std::atomic_uint64_t deallocated_bytes = 0;
            
            std::atomic_uint64_t peak_allocated_bytes = 0;
    };
    
    class call_tracker_t
    {
        public:
            struct call_data_t
            {
                blt::u64 start_calls = 0;
                blt::u64 start_value = 0;
                blt::u64 end_calls = 0;
                blt::u64 end_value = 0;
                
                [[nodiscard]] inline auto get_call_difference() const
                {
                    return end_calls - start_calls;
                }
                
                [[nodiscard]] inline auto get_value_difference() const
                {
                    return end_value - start_value;
                }
            };
            
            void value(blt::u64 value)
            {
                secondary_value += value;
            }
            
            void set_value(blt::u64 value)
            {
                secondary_value = value;
            }
            
            void call()
            {
                primary_calls++;
            }
            
            void call(blt::u64 v)
            {
                primary_calls++;
                value(v);
            }
            
            [[nodiscard]] auto get_calls() const
            {
                return primary_calls.load();
            }
            
            [[nodiscard]] auto get_value() const
            {
                return secondary_value.load();
            }
            
            call_data_t start_measurement() const
            {
                return {primary_calls.load(), secondary_value.load()};
            }
            
            void stop_measurement(call_data_t& data) const
            {
                data.end_calls = primary_calls.load();
                data.end_value = secondary_value.load();
            }
        
        private:
            std::atomic_uint64_t primary_calls = 0;
            std::atomic_uint64_t secondary_value = 0;
    };
}

#endif //BLT_GP_STATS_H
