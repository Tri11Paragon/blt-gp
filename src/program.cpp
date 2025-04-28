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
#include <iostream>
#include <blt/std/variant.h>

#ifndef BLT_ASSERT_RET
#define BLT_ASSERT_RET(expr) if (!(expr)) { return false; }
#endif

#define BLT_READ(read_statement, size) do { auto read = read_statement; if (read != size) { return blt::gp::errors::serialization::invalid_read_t{read, size}; } } while (false)

namespace blt::gp
{
    // default static references for mutation, crossover, and initializer
    // this is largely to not break the tests :3
    // it's also to allow for quick setup of a gp program if you don't care how crossover or mutation is handled
    static advanced_mutation_t s_mutator;
    static subtree_crossover_t s_crossover;
    static ramped_half_initializer_t s_init;

    prog_config_t::prog_config_t(): mutator(s_mutator), crossover(s_crossover), pop_initializer(s_init)
    {
    }

    prog_config_t::prog_config_t(const std::reference_wrapper<population_initializer_t>& popInitializer):
        mutator(s_mutator), crossover(s_crossover), pop_initializer(popInitializer)
    {
    }

    prog_config_t::prog_config_t(size_t populationSize, const std::reference_wrapper<population_initializer_t>& popInitializer):
        population_size(populationSize), mutator(s_mutator), crossover(s_crossover), pop_initializer(popInitializer)
    {
    }

    prog_config_t::prog_config_t(size_t populationSize):
        population_size(populationSize), mutator(s_mutator), crossover(s_crossover), pop_initializer(s_init)
    {
    }

    random_t& gp_program::get_random() const
    {
        thread_local static blt::gp::random_t random_engine{seed_func()};
        return random_engine;
    }

    stack_allocator::Allocator& stack_allocator::get_allocator()
    {
        static Allocator allocator;
        return allocator;
    }

    void gp_program::save_generation(fs::writer_t& writer)
    {
        const auto individuals = current_pop.get_individuals().size();
        writer.write(&individuals, sizeof(individuals));
        for (const auto& individual : current_pop.get_individuals())
        {
            writer.write(&individual.fitness, sizeof(individual.fitness));
            individual.tree.to_file(writer);
        }
    }

    bool gp_program::load_generation(fs::reader_t& reader)
    {
        size_t individuals;
        BLT_ASSERT_RET(reader.read(&individuals, sizeof(individuals)) == sizeof(individuals));
        if (current_pop.get_individuals().size() != individuals)
        {
            for (size_t i = current_pop.get_individuals().size(); i < individuals; i++)
                current_pop.get_individuals().emplace_back(tree_t{*this});
        }
        for (auto& individual : current_pop.get_individuals())
        {
            BLT_ASSERT_RET(reader.read(&individual.fitness, sizeof(individual.fitness)) == sizeof(individual.fitness));
            individual.tree.clear(*this);
            individual.tree.from_file(reader);
        }
        return true;
    }

    void write_stat(fs::writer_t& writer, const population_stats& stat)
    {
        const auto overall_fitness = stat.overall_fitness.load();
        const auto average_fitness = stat.average_fitness.load();
        const auto best_fitness = stat.best_fitness.load();
        const auto worst_fitness = stat.worst_fitness.load();
        writer.write(&overall_fitness, sizeof(overall_fitness));
        writer.write(&average_fitness, sizeof(average_fitness));
        writer.write(&best_fitness, sizeof(best_fitness));
        writer.write(&worst_fitness, sizeof(worst_fitness));
        const size_t fitness_count = stat.normalized_fitness.size();
        writer.write(&fitness_count, sizeof(fitness_count));
        for (const auto& fitness : stat.normalized_fitness)
            writer.write(&fitness, sizeof(fitness));
    }

    bool load_stat(fs::reader_t& reader, population_stats& stat)
    {
        BLT_ASSERT_RET(reader.read(&stat.overall_fitness, sizeof(stat.overall_fitness)) == sizeof(stat.overall_fitness));
        BLT_ASSERT_RET(reader.read(&stat.average_fitness, sizeof(stat.average_fitness)) == sizeof(stat.average_fitness));
        BLT_ASSERT_RET(reader.read(&stat.best_fitness, sizeof(stat.best_fitness)) == sizeof(stat.best_fitness));
        BLT_ASSERT_RET(reader.read(&stat.worst_fitness, sizeof(stat.worst_fitness)) == sizeof(stat.worst_fitness));
        size_t fitness_count;
        BLT_ASSERT_RET(reader.read(&fitness_count, sizeof(fitness_count)) == sizeof(size_t));
        stat.normalized_fitness.resize(fitness_count);
        for (auto& fitness : stat.normalized_fitness)
            BLT_ASSERT_RET(reader.read(&fitness, sizeof(fitness)) == sizeof(fitness));
        return true;
    }

    void gp_program::save_state(fs::writer_t& writer)
    {
        const size_t operator_count = storage.operators.size();
        writer.write(&operator_count, sizeof(operator_count));
        for (const auto& [i, op] : enumerate(storage.operators))
        {
            writer.write(&i, sizeof(i));
            bool has_name = storage.names[i].has_value();
            writer.write(&has_name, sizeof(has_name));
            if (has_name)
            {
                auto size = storage.names[i]->size();
                writer.write(&size, sizeof(size));
                writer.write(storage.names[i]->data(), size);
            }
            writer.write(&storage.operator_metadata[i].arg_size_bytes, sizeof(storage.operator_metadata[i].arg_size_bytes));
            writer.write(&storage.operator_metadata[i].return_size_bytes, sizeof(storage.operator_metadata[i].return_size_bytes));
            writer.write(&op.argc, sizeof(op.argc));
            writer.write(&op.return_type, sizeof(op.return_type));
            const size_t argc_type_count = op.argument_types.size();
            writer.write(&argc_type_count, sizeof(argc_type_count));
            for (const auto argument : op.argument_types)
                writer.write(&argument, sizeof(argument));
        }
        const size_t history_count = statistic_history.size();
        writer.write(&history_count, sizeof(history_count));
        for (const auto& stat : statistic_history)
            write_stat(writer, stat);
        write_stat(writer, current_stats);
        save_generation(writer);
    }

    std::optional<errors::serialization::serializer_error_t> gp_program::load_state(fs::reader_t& reader)
    {
        size_t operator_count;
        BLT_READ(reader.read(&operator_count, sizeof(operator_count)), sizeof(operator_count));
        if (operator_count != storage.operators.size())
            return errors::serialization::unexpected_size_t{operator_count, storage.operators.size()};
        for (size_t i = 0; i < operator_count; i++)
        {
            size_t expected_i;
            BLT_READ(reader.read(&expected_i, sizeof(expected_i)), sizeof(expected_i));
            if (expected_i != i)
                return errors::serialization::invalid_operator_id_t{i, expected_i};
            bool has_name;
            BLT_READ(reader.read(&has_name, sizeof(has_name)), sizeof(has_name));
            if (has_name)
            {
                size_t size;
                BLT_READ(reader.read(&size, sizeof(size)), sizeof(size));
                std::string name;
                name.resize(size);
                BLT_READ(reader.read(name.data(), size), static_cast<i64>(size));
                if (!storage.names[i].has_value())
                    return errors::serialization::invalid_name_t{i, name, "NO NAME"};
                if (name != *storage.names[i])
                    return errors::serialization::invalid_name_t{i, name, std::string{*storage.names[i]}};
                const auto& op = storage.operators[i];
                const auto& op_meta = storage.operator_metadata[i];

                decltype(std::declval<decltype(storage.operator_metadata)::value_type>().arg_size_bytes) arg_size_bytes;
                decltype(std::declval<decltype(storage.operator_metadata)::value_type>().return_size_bytes) return_size_bytes;
                BLT_READ(reader.read(&arg_size_bytes, sizeof(arg_size_bytes)), sizeof(arg_size_bytes));
                BLT_READ(reader.read(&return_size_bytes, sizeof(return_size_bytes)), sizeof(return_size_bytes));

                if (op_meta.arg_size_bytes != arg_size_bytes)
                    return errors::serialization::mismatched_bytes_t{i, arg_size_bytes, op_meta.arg_size_bytes};

                if (op_meta.return_size_bytes != return_size_bytes)
                    return errors::serialization::mismatched_bytes_t{i, return_size_bytes, op_meta.return_size_bytes};

                argc_t argc;
                BLT_READ(reader.read(&argc, sizeof(argc)), sizeof(argc));
                if (argc.argc != op.argc.argc)
                    return errors::serialization::mismatched_argc_t{i, argc.argc, op.argc.argc};
                if (argc.argc_context != op.argc.argc_context)
                    return errors::serialization::mismatched_argc_t{i, argc.argc_context, op.argc.argc_context};

                type_id return_type;
                BLT_READ(reader.read(&return_type, sizeof(return_type)), sizeof(return_type));
                if (return_type != op.return_type)
                    return errors::serialization::mismatched_return_type_t{i, return_type, op.return_type};
                size_t arg_type_count;
                BLT_READ(reader.read(&arg_type_count, sizeof(arg_type_count)), sizeof(return_type));
                if (arg_type_count != op.argument_types.size())
                    return errors::serialization::unexpected_size_t{arg_type_count, op.argument_types.size()};
                for (size_t j = 0; j < arg_type_count; j++)
                {
                    type_id type;
                    BLT_READ(reader.read(&type, sizeof(type)), sizeof(type));
                    if (type != op.argument_types[j])
                        return errors::serialization::mismatched_arg_type_t{i, j, type, op.argument_types[j]};
                }
            }
        }
        size_t history_count;
        BLT_READ(reader.read(&history_count, sizeof(history_count)), sizeof(history_count));
        statistic_history.resize(history_count);
        for (size_t i = 0; i < history_count; i++)
            load_stat(reader, statistic_history[i]);
        load_stat(reader, current_stats);
        load_generation(reader);

        return {};
    }

    void gp_program::create_threads()
    {
#ifdef BLT_TRACK_ALLOCATIONS
        tracker.reserve();
#endif
        statistic_history.reserve(config.max_generations + 1);
        if (config.threads == 0)
            config.set_thread_count(std::thread::hardware_concurrency());
        // main thread is thread0
        for (blt::size_t i = 1; i < config.threads; i++)
        {
            thread_helper.threads.emplace_back(new std::thread([i, this]()
            {
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.reserve();
                tracker.await_thread_loading_complete(config.threads);
#endif
                std::function<void(blt::size_t)>* execution_function = nullptr;
                while (!should_thread_terminate())
                {
                    if (execution_function == nullptr)
                    {
                        std::unique_lock lock(thread_helper.thread_function_control);
                        while (thread_execution_service == nullptr)
                        {
                            thread_helper.thread_function_condition.wait(lock);
                            if (should_thread_terminate())
                                return;
                        }
                        execution_function = thread_execution_service.get();
                    }
                    if (execution_function != nullptr)
                        (*execution_function)(i);
                }
            }));
        }
#ifdef BLT_TRACK_ALLOCATIONS
        tracker.await_thread_loading_complete(config.threads);
#endif
    }
}
