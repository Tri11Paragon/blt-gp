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
#include <blt/gp/program.h>
#include <blt/gp/tree.h>
#include <blt/std/logging.h>

static const auto SEED_FUNC = [] { return std::random_device()(); };

blt::gp::prog_config_t config = blt::gp::prog_config_t().set_elite_count(2);

blt::gp::gp_program program(SEED_FUNC, config); // NOLINT
std::array<float, 500> result_container;

blt::gp::operation_t add([](float a, float b) { return a + b; }, "add"); // 0
blt::gp::operation_t sub([](float a, float b) { return a - b; }, "sub"); // 1
blt::gp::operation_t mul([](float a, float b) { return a * b; }, "mul"); // 2
blt::gp::operation_t pro_div([](float a, float b) { return b == 0 ? 0.0f : a / b; }, "div"); // 3

blt::gp::operation_t op_if([](bool b, float a, float c) { return b ? a : c; }, "if"); // 4
blt::gp::operation_t eq_f([](float a, float b) { return a == b; }, "eq_f"); // 5
blt::gp::operation_t eq_b([](bool a, bool b) { return a == b; }, "eq_b"); // 6
blt::gp::operation_t lt([](float a, float b) { return a < b; }, "lt"); // 7
blt::gp::operation_t gt([](float a, float b) { return a > b; }, "gt"); // 8
blt::gp::operation_t op_and([](bool a, bool b) { return a && b; }, "and"); // 9
blt::gp::operation_t op_or([](bool a, bool b) { return a || b; }, "or"); // 10
blt::gp::operation_t op_xor([](bool a, bool b) { return static_cast<bool>(a ^ b); }, "xor"); // 11
blt::gp::operation_t op_not([](bool b) { return !b; }, "not"); // 12

auto lit = blt::gp::operation_t([]() {
    //static std::uniform_real_distribution<float> dist(-32000, 32000);
//    static std::uniform_real_distribution<float> dist(0.0f, 10.0f);
    return program.get_random().get_float(0.0f, 10.0f);
}).set_ephemeral();

void print_best()
{
    BLT_TRACE("----{Current Gen: %ld}----", program.get_current_generation());
    auto best = program.get_best_indexes<10>();
    
    for (auto& i : best)
    {
        auto& v = program.get_current_pop().get_individuals()[i];
        auto& tree = v.tree;
        auto size = tree.get_values().size();
        BLT_TRACE("%lf [index %ld] (fitness: %lf, raw: %lf) (depth: %ld) (size: t: %ld u: %ld r: %ld) filled: %f%%",
                  tree.get_evaluation_value<float>(), i, v.fitness.adjusted_fitness, v.fitness.raw_fitness,
                  tree.get_depth(program), size.total_size_bytes, size.total_used_bytes,
                  size.total_remaining_bytes,
                  static_cast<double>(size.total_used_bytes) / (size.total_size_bytes == 0 ? 1 : static_cast<double>(size.total_size_bytes)));
    }
    //std::string small("--------------------------");
    //for (blt::size_t i = 0; i < std::to_string(program.get_current_generation()).size(); i++)
    //    small += "-";
    //BLT_TRACE(small);
}

constexpr auto fitness_function = [](blt::gp::tree_t& current_tree, blt::gp::fitness_t& fitness, blt::size_t index) {
    /*auto size = current_tree.get_values().size();
    BLT_DEBUG("(depth: %ld) (blocks: %ld) (size: t: %ld m: %ld u: %ld r: %ld) filled: %f%%",
              current_tree.get_depth(program), size.blocks, size.total_size_bytes, size.total_no_meta_bytes, size.total_used_bytes,
              size.total_remaining_bytes, static_cast<double>(size.total_used_bytes) / static_cast<double>(size.total_no_meta_bytes));*/
    result_container[index] = current_tree.get_evaluation_value<float>();
    fitness.raw_fitness = result_container[index] / 1000000000.0;
    fitness.standardized_fitness = fitness.raw_fitness;
    fitness.adjusted_fitness = 1.0 - (1.0 / (1.0 + fitness.raw_fitness));
};

/**
 * This is a test using multiple types with blt::gp
 */
int main()
{
    blt::gp::operator_builder builder{};
    program.set_operations(builder.build(add, sub, mul, pro_div, op_if, eq_f, eq_b, lt, gt, op_and, op_or, op_xor, op_not, lit));
    
    auto sel = blt::gp::select_tournament_t{};
    program.generate_population(program.get_typesystem().get_type<float>().id(), fitness_function, sel, sel, sel);
    
    while (!program.should_terminate())
    {
        print_best();
        program.create_next_generation();
        program.next_generation();
        program.evaluate_fitness();
    }
    
    print_best();
    
    return 0;
}