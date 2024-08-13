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
#include <blt/profiling/profiler_v2.h>
#include <blt/gp/tree.h>
#include <blt/std/logging.h>
#include <iostream>
#include <atomic>
#include <type_traits>

//static constexpr long SEED = 41912;
static const unsigned long SEED = std::random_device()();

inline std::atomic_uint64_t last_value = 0;
inline std::atomic_uint64_t constructions = 0;
inline std::atomic_uint64_t destructions = 0;

class move_float
{
    public:
        move_float(): f(new float()), assignment(++last_value)
        {
            constructions++;
            //BLT_TRACE("Value %ld Default Constructed", assignment);
        }
        
        explicit move_float(float f): f(new float(f)), assignment(++last_value)
        {
            constructions++;
            //BLT_TRACE("Value %ld Constructed", assignment);
        }
        
        explicit operator float() const
        {
            //BLT_TRACE("Using value %ld", assignment);
            return *f;
        }
        
        [[nodiscard]] float get() const
        {
            //BLT_TRACE("Using value %ld", assignment);
            return *f;
        }
        
        float operator*() const
        {
            //BLT_TRACE("Using value %ld", assignment);
            return *f;
        }
        
        void drop() // NOLINT
        {
            //BLT_TRACE("Drop Called On %ld", assignment);
            delete f;
            f = nullptr;
            destructions++;
        }
        
        friend std::ostream& operator<<(std::ostream& stream, const move_float& e)
        {
            stream << *e;
            return stream;
        }
        
    private:
        float* f = nullptr;
        blt::size_t assignment;
};

static_assert(std::is_trivially_copyable_v<move_float>);
//static_assert(std::is_standard_layout_v<move_float>);

struct context
{
    float x, y;
};

std::array<context, 200> fitness_cases;

blt::gp::prog_config_t config = blt::gp::prog_config_t()
        .set_initial_min_tree_size(2)
        .set_initial_max_tree_size(6)
        .set_elite_count(0)
        .set_crossover_chance(0)
        .set_mutation_chance(0)
        .set_reproduction_chance(1.0)
        .set_max_generations(5)
        .set_pop_size(500)
        .set_thread_count(0);

blt::gp::type_provider type_system;
blt::gp::gp_program program{type_system, SEED, config};

blt::gp::operation_t add([](const move_float& a, const move_float& b) { return move_float(*a + *b); }, "add");                          // 0
blt::gp::operation_t sub([](const move_float& a, const move_float& b) { return move_float(*a - *b); }, "sub");                          // 1
blt::gp::operation_t mul([](const move_float& a, const move_float& b) { return move_float(*a * *b); }, "mul");                          // 2
blt::gp::operation_t pro_div([](const move_float& a, const move_float& b) { return move_float(*b == 0.0f ? 1.0f : *a / *b); }, "div");  // 3
blt::gp::operation_t op_sin([](const move_float& a) { return move_float(std::sin(*a)); }, "sin");                                       // 4
blt::gp::operation_t op_cos([](const move_float& a) { return move_float(std::cos(*a)); }, "cos");                                       // 5
blt::gp::operation_t op_exp([](const move_float& a) { return move_float(std::exp(*a)); }, "exp");                                       // 6
blt::gp::operation_t op_log([](const move_float& a) { return move_float(*a == 0.0f ? 0.0f : std::log(*a)); }, "log");                   // 7

blt::gp::operation_t lit([]() {                                                                                                         // 8
    return move_float(program.get_random().get_float(-320.0f, 320.0f));
}, "lit");
blt::gp::operation_t op_x([](const context& context) {                                                                                  // 9
    return move_float(context.x);
}, "x");

constexpr auto fitness_function = [](blt::gp::tree_t& current_tree, blt::gp::fitness_t& fitness, blt::size_t) {
    constexpr double value_cutoff = 1.e15;
    for (auto& fitness_case : fitness_cases)
    {
        auto ctx = current_tree.evaluate(&fitness_case);
        auto diff = std::abs(fitness_case.y - *current_tree.get_evaluation_ref<move_float>(ctx));
        // this will call the drop function.
        current_tree.get_evaluation_value<move_float>(ctx);
        if (diff < value_cutoff)
        {
            fitness.raw_fitness += diff;
            if (diff < 0.01)
                fitness.hits++;
        } else
            fitness.raw_fitness += value_cutoff;
    }
    fitness.standardized_fitness = fitness.raw_fitness;
    fitness.adjusted_fitness = (1.0 / (1.0 + fitness.standardized_fitness));
    return static_cast<blt::size_t>(fitness.hits) == fitness_cases.size();
};

float example_function(float x)
{
    return x * x * x * x + x * x * x + x * x + x;
}

int main()
{
    BLT_INFO("Starting BLT-GP Symbolic Regression Example");
    BLT_START_INTERVAL("Symbolic Regression", "Main");
    BLT_DEBUG("Setup Fitness cases");
    for (auto& fitness_case : fitness_cases)
    {
        constexpr float range = 10;
        constexpr float half_range = range / 2.0;
        auto x = program.get_random().get_float(-half_range, half_range);
        auto y = example_function(x);
        fitness_case = {x, y};
    }
    
    BLT_DEBUG("Setup Types and Operators");
    type_system.register_type<move_float>();
    
    blt::gp::operator_builder<context> builder{type_system};
    builder.add_operator(add);
    builder.add_operator(sub);
    builder.add_operator(mul);
    builder.add_operator(pro_div);
    builder.add_operator(op_sin);
    builder.add_operator(op_cos);
    builder.add_operator(op_exp);
    builder.add_operator(op_log);
    
    builder.add_operator(lit, true);
    builder.add_operator(op_x);
    
    program.set_operations(builder.build());
    
    BLT_DEBUG("Generate Initial Population");
    program.generate_population(type_system.get_type<float>().id(), fitness_function);
    
    BLT_DEBUG("Begin Generation Loop");
    while (!program.should_terminate())
    {
        BLT_TRACE("------------{Begin Generation %ld}------------", program.get_current_generation());
        BLT_START_INTERVAL("Symbolic Regression", "Gen");
        auto sel = blt::gp::select_fitness_proportionate_t{};
        program.create_next_generation(sel, sel, sel);
        BLT_END_INTERVAL("Symbolic Regression", "Gen");
        BLT_TRACE("Move to next generation");
        BLT_START_INTERVAL("Symbolic Regression", "Fitness");
        program.next_generation();
        BLT_TRACE("Evaluate Fitness");
        program.evaluate_fitness();
        BLT_END_INTERVAL("Symbolic Regression", "Fitness");
        BLT_TRACE("----------------------------------------------");
        std::cout << std::endl;
    }
    
    BLT_END_INTERVAL("Symbolic Regression", "Main");
    
    auto best = program.get_best_individuals<1>();
    
    BLT_INFO("Best approximations:");
    for (auto& i_ref : best)
    {
        auto& i = i_ref.get();
        BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);
        i.tree.print(program, std::cout);
        std::cout << "\n";
    }
    auto& stats = program.get_population_stats();
    BLT_INFO("Stats:");
    BLT_INFO("Average fitness: %lf", stats.average_fitness.load());
    BLT_INFO("Best fitness: %lf", stats.best_fitness.load());
    BLT_INFO("Worst fitness: %lf", stats.worst_fitness.load());
    BLT_INFO("Overall fitness: %lf", stats.overall_fitness.load());
    // TODO: make stats helper
    
    BLT_PRINT_PROFILE("Symbolic Regression", blt::PRINT_CYCLES | blt::PRINT_THREAD | blt::PRINT_WALL);
    
    BLT_TRACE("Constructions %ld Destructions %ld Difference %ld", constructions.load(), destructions.load(),
              std::abs(static_cast<blt::ptrdiff_t>(constructions) - static_cast<blt::ptrdiff_t>(destructions)));
    
    return 0;
}