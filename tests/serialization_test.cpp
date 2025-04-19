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
#include <blt/std/variant.h>
#include <filesystem>

#include "../examples/symbolic_regression.h"
#include <blt/gp/program.h>
#include <blt/logging/logging.h>
#include <ostream>
#include <istream>
#include <fstream>
#include <blt/fs/stream_wrappers.h>


struct no_default
{
    no_default() = delete;

    no_default(int)
    {
    }
};

struct not_copyable
{
    not_copyable() = default;
    not_copyable(const not_copyable&) = delete;
};

struct copyable
{
    copyable() = default;
    copyable(const copyable&) = default;
};

struct copyable_nothrow
{
    copyable_nothrow() = default;
    copyable_nothrow(const copyable_nothrow&) noexcept = default;
};

struct not_movable
{
    not_movable() = default;
    not_movable(not_movable&&) = delete;
};

struct movable
{
    movable() = default;
    movable(movable&&) = delete;
};

struct movable_nothrow
{
    movable_nothrow() = default;
    movable_nothrow(movable_nothrow&&) noexcept = delete;
};

blt::variant_t<no_default> no_default_variant;
blt::variant_t<not_copyable> not_copyable_variant;
blt::variant_t<copyable> copyable_variant;
blt::variant_t<copyable_nothrow> copyable_nothrow_variant;
blt::variant_t<not_movable> not_movable_variant;
blt::variant_t<movable> movable_variant;
blt::variant_t<movable_nothrow> movable_nothrow_variant;

using namespace blt::gp;

struct context
{
    float x, y;
};

prog_config_t config = prog_config_t()
                       .set_initial_min_tree_size(2)
                       .set_initial_max_tree_size(6)
                       .set_elite_count(2)
                       .set_crossover_chance(0.8)
                       .set_mutation_chance(0.1)
                       .set_reproduction_chance(0.1)
                       .set_max_generations(50)
                       .set_pop_size(500)
                       .set_thread_count(0);


example::symbolic_regression_t regression{691ul, config};

operation_t addf{[](const float a, const float b) { return a + b; }, "addf"};
operation_t subf([](const float a, const float b) { return a - b; }, "subf");
operation_t mulf([](const float a, const float b) { return a * b; }, "mulf");
operation_t pro_divf([](const float a, const float b) { return b == 0.0f ? 0.0f : a / b; }, "divf");
operation_t op_sinf([](const float a) { return std::sin(a); }, "sinf");
operation_t op_cosf([](const float a) { return std::cos(a); }, "cosf");
operation_t op_expf([](const float a) { return std::exp(a); }, "expf");
operation_t op_logf([](const float a) { return a <= 0.0f ? 0.0f : std::log(a); }, "logf");

auto litf = operation_t([]()
{
    return regression.get_program().get_random().get_float(-1.0f, 1.0f);
}, "litf").set_ephemeral();

operation_t op_xf([](const context& context)
{
    return context.x;
}, "xf");

bool fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t)
{
    constexpr static double value_cutoff = 1.e15;
    for (auto& fitness_case : regression.get_training_cases())
    {
        BLT_GP_UPDATE_CONTEXT(fitness_case);
        auto val = current_tree.get_evaluation_ref<float>(fitness_case);
        const auto diff = std::abs(fitness_case.y - val.get());
        if (diff < value_cutoff)
        {
            fitness.raw_fitness += diff;
            if (diff <= 0.01)
                fitness.hits++;
        }
        else
            fitness.raw_fitness += value_cutoff;
    }
    fitness.standardized_fitness = fitness.raw_fitness;
    fitness.adjusted_fitness = (1.0 / (1.0 + fitness.standardized_fitness));
    return static_cast<size_t>(fitness.hits) == regression.get_training_cases().size();
}

int main()
{
    operator_builder<context> builder{};
    const auto& operators = builder.build(addf, subf, mulf, pro_divf, op_sinf, op_cosf, op_expf, op_logf, litf, op_xf);
    regression.get_program().set_operations(operators);

    auto& program = regression.get_program();
    static auto sel = select_tournament_t{};

    gp_program test_program{691};
    test_program.set_operations(operators);
    test_program.setup_generational_evaluation(fitness_function, sel, sel, sel, false);

    // simulate a program which is similar but incompatible with the other programs.
    operator_builder<context> builder2{};
    gp_program bad_program{691};
    bad_program.set_operations(builder2.build(addf, subf, mulf, op_sinf, op_cosf, litf, op_xf));
    bad_program.setup_generational_evaluation(fitness_function, sel, sel, sel, false);

    program.generate_initial_population(program.get_typesystem().get_type<float>().id());
    program.setup_generational_evaluation(fitness_function, sel, sel, sel);
    while (!program.should_terminate())
    {
        BLT_TRACE("---------------\\{Begin Generation {}}---------------", program.get_current_generation());
        BLT_TRACE("Creating next generation");
        program.create_next_generation();
        BLT_TRACE("Move to next generation");
        program.next_generation();
        BLT_TRACE("Evaluate Fitness");
        program.evaluate_fitness();
        {
            std::ofstream stream{"serialization_test.data", std::ios::binary | std::ios::trunc};
            blt::fs::fstream_writer_t writer{stream};
            program.save_generation(writer);
        }
        {
            std::ifstream stream{"serialization_test.data", std::ios::binary};
            blt::fs::fstream_reader_t reader{stream};
            test_program.load_generation(reader);
        }
        // do a quick validation check
        for (const auto& [saved, loaded] : blt::zip(program.get_current_pop(), test_program.get_current_pop()))
        {
            if (saved.tree != loaded.tree)
            {
                BLT_ERROR("Serializer Failed to correctly serialize tree to disk, trees are not equal!");
                std::exit(1);
            }
        }
    }
    {
        std::ofstream stream{"serialization_test2.data", std::ios::binary | std::ios::trunc};
        blt::fs::fstream_writer_t writer{stream};
        program.save_state(writer);
    }
    {
        std::ifstream stream{"serialization_test2.data", std::ios::binary};
        blt::fs::fstream_reader_t reader{stream};
        if (auto err = test_program.load_state(reader))
        {
            BLT_ERROR("Error: {}", blt::gp::errors::serialization::to_string(*err));
            BLT_ABORT("Expected program to succeeded without returning an error state!");
        }

        for (const auto [saved, loaded] : blt::zip(program.get_stats_histories(), test_program.get_stats_histories()))
        {
            if (saved != loaded)
            {
                BLT_ERROR("Serializer Failed to correctly serialize histories to disk, histories are not equal!");
                std::exit(1);
            }
        }
    }
    {
        std::ifstream stream{"serialization_test2.data", std::ios::binary};
        blt::fs::fstream_reader_t reader{stream};
        if (!bad_program.load_state(reader).has_value())
        {
            BLT_ABORT("Expected program to throw an exception when parsing state data into an incompatible program!");
        }
    }
}
