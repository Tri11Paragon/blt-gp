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
#include <iostream>
#include <blt/gp/program.h>

int main()
{
    blt::gp::operation<float, float, int, bool> silly([](float f, int i, bool b) -> float {
        return static_cast<float>(b);
    });
    
    float f = 10.5;
    int i = 412;
    bool b = true;
    
    std::array<void*, 3> arr{reinterpret_cast<void*>(&f), reinterpret_cast<void*>(&i), reinterpret_cast<void*>(&b)};
    
    blt::span<void*, 3> spv{arr};
    
    std::cout << silly.operator()(spv) << std::endl;
    
    std::cout << "Hello World!" << std::endl;
}