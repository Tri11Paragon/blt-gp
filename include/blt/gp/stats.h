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
#include <atomic>
#include <cmath>

namespace blt::gp
{
    
    class allocation_tracker_t
    {
        public:
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
            };
            
            void allocate(blt::size_t bytes)
            {
                allocations++;
                allocated_bytes += bytes;
            }
            
            void deallocate(blt::size_t bytes)
            {
                deallocations++;
                deallocated_bytes += bytes;
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
            
            [[nodiscard]] allocation_data_t start_measurement() const
            {
                allocation_data_t data{};
                data.start_allocations = allocations;
                data.start_deallocations = deallocations;
                data.start_allocated_bytes = allocated_bytes;
                data.start_deallocated_bytes = deallocated_bytes;
                return data;
            }
            
            void stop_measurement(allocation_data_t& data) const
            {
                data.end_allocations = allocations;
                data.end_deallocations = deallocations;
                data.end_allocated_bytes = allocated_bytes;
                data.end_deallocated_bytes = deallocated_bytes;
            }
        
        private:
            std::atomic_uint64_t allocations = 0;
            std::atomic_uint64_t deallocations = 0;
            std::atomic_uint64_t allocated_bytes = 0;
            std::atomic_uint64_t deallocated_bytes = 0;
    };
    
}

#endif //BLT_GP_STATS_H
