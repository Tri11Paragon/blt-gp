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

large_18290 base{};

blt::gp::type_provider type_system;
blt::gp::gp_program program{type_system, SEED};

blt::gp::op_container_t make_container(blt::gp::operator_id id)
{
    auto& info = program.get_operator_info(id);
    return {type_system.get_type(info.return_type).size(), id, false};
}

blt::gp::op_container_t make_value(const blt::gp::type& id)
{
    return {id.size(), 0, true};
}

blt::gp::operation_t add([](float a, float b) {
    return a + b;
});

blt::gp::operation_t sub([](float a, float b) {
    return a - b;
});

blt::gp::operation_t basic_2t([](float a, bool b) {
    return b ? a : 0.0f;
});

blt::gp::operation_t cross_large_type([](const large_18290& input, const float a, const float b) {
    BLT_TRACE("%f, %f", a, b);
    large_18290 output{};
    for (const auto& [index, ref] : blt::enumerate(input.data))
    {
        if (ref > static_cast<blt::u8>(a) && ref < static_cast<blt::u8>(b))
            output.data[index] = ref;
    }
    return output;
});

blt::gp::operation_t f_literal([]() {
    return 555.0f;
});

blt::gp::operation_t b_literal([]() {
    return false;
});

blt::gp::operation_t large_literal([]() {
    return base;
});

void basic_tree()
{
    BLT_INFO("Testing if we can get a basic tree going.");
    blt::gp::tree_t tree{program};
    
    tree.get_operations().push_back(make_container(sub.id));
    tree.get_operations().push_back(make_value(type_system.get_type<float>()));
    tree.get_operations().push_back(make_value(type_system.get_type<float>()));
    tree.get_values().push(50.0f);
    tree.get_values().push(120.0f);
    
    auto val = tree.get_evaluation_value<float>(nullptr);
    BLT_TRACE(val);
    BLT_ASSERT(val == (120 - 50));
}

void large_cross_type_tree()
{
    blt::gp::tree_t tree{program};
    auto& ops = tree.get_operations();
    auto& vals = tree.get_values();
    
    ops.push_back(make_container(cross_large_type.id));
    ops.push_back(make_container(sub.id));
    ops.push_back(make_value(type_system.get_type<float>()));
    ops.push_back(make_value(type_system.get_type<float>()));
    ops.push_back(make_value(type_system.get_type<float>()));
    ops.push_back(make_container(large_literal.id));
    
    vals.push(50.0f);
    vals.push(120.0f);
    vals.push(5555.0f);
    
    auto val = tree.get_evaluation_value<large_18290>(nullptr);
    blt::black_box(val);
}

int main()
{
    for (auto& v : base.data)
        v = static_cast<blt::u8>(blt::random::murmur_random64c(691, std::numeric_limits<blt::u8>::min(), std::numeric_limits<blt::u8>::max()));
    
    type_system.register_type<float>();
    type_system.register_type<bool>();
    type_system.register_type<large_18290>();
    
    blt::gp::operator_builder builder{type_system};
    program.set_operations(builder.build(f_literal, b_literal, add, basic_2t, sub, large_literal, cross_large_type));
    
    basic_tree();
    large_cross_type_tree();
}