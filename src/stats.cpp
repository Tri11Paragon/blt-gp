/*
 *  <Short Description>
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
#include <blt/gp/stats.h>
#include <blt/std/logging.h>
#include "blt/std/format.h"

namespace blt::gp
{
    
    void allocation_tracker_t::allocation_data_t::pretty_print(const std::string& name) const
    {
        BLT_TRACE("%s Allocations: %ld times with a total of %s", name.c_str(), getAllocationDifference(),
                  blt::byte_convert_t(getAllocatedByteDifference()).convert_to_nearest_type().to_pretty_string().c_str());
    }
}