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

namespace blt::gp
{
    // default static references for mutation, crossover, and initializer
    // this is largely to not break the tests :3
    // it's also to allow for quick setup of a gp program if you don't care how crossover or mutation is handled
    static advanced_mutation_t s_mutator;
    static crossover_t s_crossover;
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

    void gp_program::load_generation(fs::reader_t& reader)
    {
        size_t individuals;
        reader.read(&individuals, sizeof(individuals));
        if (current_pop.get_individuals().size() != individuals)
        {
            for (size_t i = current_pop.get_individuals().size(); i < individuals; i++)
                current_pop.get_individuals().emplace_back(tree_t{*this});
        }
        for (auto& individual : current_pop.get_individuals())
        {
            reader.read(&individual.fitness, sizeof(individual.fitness));
            individual.tree.clear(*this);
            individual.tree.from_file(reader);
        }
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
        save_generation(writer);
    }

    void gp_program::load_state(fs::reader_t& reader)
    {
        size_t operator_count;
        reader.read(&operator_count, sizeof(operator_count));
        if (operator_count != storage.operators.size())
            throw std::runtime_error(
                "Invalid number of operators. Expected " + std::to_string(storage.operators.size()) + " found " + std::to_string(operator_count));
        for (size_t i = 0; i < operator_count; i++)
        {
            size_t expected_i;
            reader.read(&expected_i, sizeof(expected_i));
            if (expected_i != i)
                throw std::runtime_error("Loaded invalid operator ID. Expected " + std::to_string(i) + " found " + std::to_string(expected_i));
            bool has_name;
            reader.read(&has_name, sizeof(has_name));
            if (has_name)
            {
                size_t size;
                reader.read(&size, sizeof(size));
                std::string name;
                name.resize(size);
                reader.read(name.data(), size);
                if (!storage.names[i].has_value())
                    throw std::runtime_error("Expected operator ID " + std::to_string(i) + " to have name " + name);
                if (name != *storage.names[i])
                    throw std::runtime_error(
                        "Operator ID " + std::to_string(i) + " expected to be named " + name + " found " + std::string(*storage.names[i]));
                auto& op = storage.operators[i];
                auto& op_meta = storage.operator_metadata[i];

                decltype(std::declval<decltype(storage.operator_metadata)::value_type>().arg_size_bytes) arg_size_bytes;
                decltype(std::declval<decltype(storage.operator_metadata)::value_type>().return_size_bytes) return_size_bytes;
                reader.read(&arg_size_bytes, sizeof(arg_size_bytes));
                reader.read(&return_size_bytes, sizeof(return_size_bytes));

                if (op_meta.arg_size_bytes != arg_size_bytes)
                    throw std::runtime_error(
                        "Operator ID " + std::to_string(i) + " expected operator to take " + std::to_string(op_meta.arg_size_bytes) + " but got " +
                        std::to_string(arg_size_bytes));

                if (op_meta.return_size_bytes != return_size_bytes)
                    throw std::runtime_error(
                        "Operator ID " + std::to_string(i) + " expected operator to return " + std::to_string(op_meta.return_size_bytes) + " but got " +
                        std::to_string(return_size_bytes));

                argc_t argc;
                reader.read(&argc, sizeof(argc));
                type_id return_type;
                reader.read(&return_type, sizeof(return_type));
                size_t arg_type_count;
                reader.read(&arg_type_count, sizeof(arg_type_count));
                for (size_t j = 0; j < arg_type_count; j++)
                {
                    type_id type;
                    reader.read(&type, sizeof(type));
                }
            }
        }
        load_generation(reader);
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
