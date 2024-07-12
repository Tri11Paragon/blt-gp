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
    static mutation_t s_mutator;
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
        thread_local static blt::gp::random_t random_engine{seed};
        return random_engine;
    }
    
    void gp_program::create_threads()
    {
        if (config.threads == 0)
            config.set_thread_count(std::thread::hardware_concurrency() - 1);
        for (blt::size_t i = 0; i < config.threads; i++)
        {
            thread_helper.threads.emplace_back(new std::thread([this]() {
                while (!should_thread_terminate())
                {
                    execute_thread();
                }
            }));
        }
    }
    
    void gp_program::execute_thread()
    {
        if (thread_helper.evaluation_left > 0)
        {
            //std::cout << "Thread beginning" << std::endl;
            while (thread_helper.evaluation_left > 0)
            {
                blt::size_t begin = 0;
                blt::size_t end = 0;
                {
                    std::scoped_lock lock(thread_helper.evaluation_control);
                    end = thread_helper.evaluation_left;
                    auto size = std::min(thread_helper.evaluation_left.load(), config.evaluation_size);
                    begin = thread_helper.evaluation_left - size;
                    thread_helper.evaluation_left -= size;
                }
                //std::cout << "Processing " << begin << " to " << end << " with " << thread_helper.evaluation_left << " left" << std::endl;
                for (blt::size_t i = begin; i < end; i++)
                {
                    auto& ind = current_pop.get_individuals()[i];
                    
                    evaluate_fitness_func(ind.tree, ind.fitness, i);
                    
                    auto old_best = current_stats.best_fitness.load();
                    while (ind.fitness.adjusted_fitness > old_best &&
                           !current_stats.best_fitness.compare_exchange_weak(old_best, ind.fitness.adjusted_fitness,
                                                                             std::memory_order_release, std::memory_order_relaxed));
                    
                    auto old_worst = current_stats.worst_fitness.load();
                    while (ind.fitness.adjusted_fitness < old_worst &&
                           !current_stats.worst_fitness.compare_exchange_weak(old_worst, ind.fitness.adjusted_fitness,
                                                                              std::memory_order_release, std::memory_order_relaxed));
                    
                    auto old_overall = current_stats.overall_fitness.load();
                    while (!current_stats.overall_fitness.compare_exchange_weak(old_overall, ind.fitness.adjusted_fitness + old_overall,
                                                                                std::memory_order_release, std::memory_order_relaxed));
                }
            }
            thread_helper.threads_left--;
            //std::cout << "thread finished!" << std::endl;
        }
    }
}