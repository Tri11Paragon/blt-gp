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
#include <blt/gp/stack.h>
#include <blt/std/logging.h>
#include <blt/std/types.h>

struct large_2048
{
    blt::u8 data[2048];
};

struct large_4096
{
    blt::u8 data[4096];
};

struct large_6123
{
    blt::u8 data[6123];
};

struct large_18290
{
    blt::u8 data[18290];
};

template<typename T>
T make_data(T t)
{
    for (const auto& [index, v] : blt::enumerate(t.data))
    {
        v = index % 256;
    }
    return t;
}

void test_basic_types()
{
    blt::gp::stack_allocator stack;
    stack.push(50.0f);
    stack.push(make_data(large_2048{}));
}

int main()
{
    test_basic_types();
}