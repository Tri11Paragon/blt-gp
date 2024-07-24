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
#include <blt/std/logging.h>
#include <blt/gp/tree.h>
#include <blt/gp/operations.h>
#include <blt/gp/generators.h>
#include <blt/gp/program.h>
#include <random>

const blt::u64 SEED = std::random_device()();

struct large_256
{
    blt::u8 data[256];
};

struct large_2048
{
    blt::u8 data[2048];
};

// not actually 4096 but will fill the whole page (4096)
struct large_4096
{
    blt::u8 data[blt::gp::stack_allocator::page_size_no_block()];
};

struct large_6123
{
    blt::u8 data[6123];
};

struct large_18290
{
    blt::u8 data[18290];
};

blt::gp::type_provider type_system;
blt::gp::gp_program program{type_system, SEED};

blt::gp::op_container_t make_container(blt::gp::operator_id id)
{
    auto& info = program.get_operator_info(id);
    return {info.function, type_system.get_type(info.return_type).size(), id, false};
}

blt::gp::op_container_t make_value(const blt::gp::type& id)
{
    static blt::gp::detail::callable_t empty([](void*, blt::gp::stack_allocator&, blt::gp::stack_allocator&) {});
    return {empty, id.size(), 0, true};
}

blt::gp::operation_t basic_2([](float a, float b) {
    return a + b;
});

blt::gp::operation_t basic_2t([](float a, bool b) {
    return b ? a : 0.0f;
});

blt::gp::operation_t f_literal([]() {
    return 0.0f;
});

blt::gp::operation_t b_literal([]() {
    return false;
});

void basic_tree()
{
    BLT_INFO("Testing if we can get a basic tree going.");
    blt::gp::tree_t tree;
    
    tree.get_operations().push_back(make_container(2));
    tree.get_operations().push_back(make_value(type_system.get_type<float>()));
    tree.get_operations().push_back(make_value(type_system.get_type<float>()));
    tree.get_values().push(50.0f);
    tree.get_values().push(120.0f);
    
    auto val = tree.get_evaluation_value<float>(nullptr);
    BLT_TRACE(val);
    BLT_ASSERT(val == (50 + 120));
}

int main()
{
    type_system.register_type<float>();
    type_system.register_type<bool>();
    
    blt::gp::operator_builder builder{type_system};
    
    builder.add_operator(f_literal); // 0
    builder.add_operator(b_literal); // 1
    builder.add_operator(basic_2);   // 2
    builder.add_operator(basic_2t);  // 3
    
    program.set_operations(builder.build());
    
    basic_tree();
}