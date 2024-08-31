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
    {}
    
    prog_config_t::prog_config_t(size_t populationSize, const std::reference_wrapper<population_initializer_t>& popInitializer):
            population_size(populationSize), mutator(s_mutator), crossover(s_crossover), pop_initializer(popInitializer)
    {}
    
    prog_config_t::prog_config_t(size_t populationSize):
            population_size(populationSize), mutator(s_mutator), crossover(s_crossover), pop_initializer(s_init)
    {}
    
    random_t& gp_program::get_random() const
    {
        thread_local static blt::gp::random_t random_engine{seed_func()};
        return random_engine;
    }
    
    stack_allocator::Allocator& stack_allocator::get_allocator()
    {
        thread_local static Allocator allocator;
        return allocator;
    }
    
    void gp_program::create_threads()
    {
        if (config.threads == 0)
            config.set_thread_count(std::thread::hardware_concurrency());
        // main thread is thread0
        for (blt::size_t i = 1; i < config.threads; i++)
        {
            thread_helper.threads.emplace_back(new std::thread([i, this]() {
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
    }
}