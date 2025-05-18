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
#include <blt/gp/transformers.h>
#include <blt/gp/program.h>
#include <blt/std/ranges.h>
#include <blt/std/utility.h>
#include <algorithm>
#include <blt/std/memory.h>
#include <blt/profiling/profiler_v2.h>
#include <random>

namespace blt::gp
{
#if BLT_DEBUG_LEVEL >= 2 || defined(BLT_TRACK_ALLOCATIONS)
    std::atomic_uint64_t mutate_point_counter = 0;
    std::atomic_uint64_t mutate_expression_counter = 0;
    std::atomic_uint64_t mutate_adjust_counter = 0;
    std::atomic_uint64_t mutate_sub_func_counter = 0;
    std::atomic_uint64_t mutate_jump_counter = 0;
    std::atomic_uint64_t mutate_copy_counter = 0;

    inline void print_mutate_stats()
    {
        std::cerr << "Mutation statistics (Total: " << (mutate_point_counter + mutate_expression_counter + mutate_adjust_counter +
            mutate_sub_func_counter + mutate_jump_counter + mutate_copy_counter) << "):" << std::endl;
        std::cerr << "\tSuccessful Point Mutations: " << mutate_point_counter << std::endl;
        std::cerr << "\tSuccessful Expression Mutations: " << mutate_expression_counter << std::endl;
        std::cerr << "\tSuccessful Adjust Mutations: " << mutate_adjust_counter << std::endl;
        std::cerr << "\tSuccessful Sub Func Mutations: " << mutate_sub_func_counter << std::endl;
        std::cerr << "\tSuccessful Jump Mutations: " << mutate_jump_counter << std::endl;
        std::cerr << "\tSuccessful Copy Mutations: " << mutate_copy_counter << std::endl;
    }
#ifdef BLT_TRACK_ALLOCATIONS

    struct run_me_baby
    {
        ~run_me_baby()
        {
            print_mutate_stats();
        }
    };

    run_me_baby this_will_run_when_program_exits;
#endif
#endif

    grow_generator_t grow_generator;

    mutation_t::config_t::config_t(): generator(grow_generator)
    {
    }

    bool subtree_crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2) // NOLINT
    {
        if (p1.size() < config.min_tree_size || p2.size() < config.min_tree_size)
            return false;

        std::optional<crossover_point_t> point;

        if (config.traverse)
            point = get_crossover_point_traverse(p1, p2);
        else
            point = get_crossover_point(p1, p2);

        if (!point)
            return false;

        c1.manipulate().easy_manipulator().swap_subtrees(point->p1_crossover_point, c2, point->p2_crossover_point);

#if BLT_DEBUG_LEVEL >= 2
        if (!c1.check(detail::debug::context_ptr) || !c2.check(detail::debug::context_ptr))
            throw std::runtime_error("Tree check failed");
#endif

        return true;
    }

    std::optional<subtree_crossover_t::crossover_point_t> subtree_crossover_t::get_crossover_point(const tree_t& c1,
                                                                                   const tree_t& c2) const
    {
        const auto first = c1.select_subtree(config.terminal_chance);
        const auto second = c2.select_subtree(first.get_type(), config.max_crossover_tries, config.terminal_chance);

        if (!second)
            return {};

        return {{first, *second}};
    }

    std::optional<subtree_crossover_t::crossover_point_t> subtree_crossover_t::get_crossover_point_traverse(const tree_t& c1,
                                                                                            const tree_t& c2) const
    {
        auto c1_point_o = get_point_traverse_retry(c1, {});
        if (!c1_point_o)
            return {};
        const auto c2_point_o = get_point_traverse_retry(c2, c1_point_o->get_type());
        if (!c2_point_o)
            return {};
        return {{*c1_point_o, *c2_point_o}};
    }

    std::optional<subtree_point_t> subtree_crossover_t::get_point_traverse_retry(const tree_t& t, const std::optional<type_id> type) const
    {
        if (type)
            return t.select_subtree_traverse(*type, config.max_crossover_tries, config.terminal_chance, config.depth_multiplier);
        return t.select_subtree_traverse(config.terminal_chance, config.depth_multiplier);
    }

    bool one_point_crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2)
    {
        // if (p1.size() < config.min_tree_size || p2.size() < config.min_tree_size)
            // return false;

        subtree_point_t point1, point2; // NOLINT
        if (config.traverse)
        {
            point1 = p1.select_subtree_traverse(config.terminal_chance, config.depth_multiplier);
            if (const auto val = p2.select_subtree_traverse(point1.get_type(), config.max_crossover_tries, config.terminal_chance, config.depth_multiplier))
                point2 = *val;
            else
                return false;
        } else
        {
            point1 = p1.select_subtree(config.terminal_chance);
            if (const auto val = p2.select_subtree(point1.get_type(), config.max_crossover_tries, config.terminal_chance))
                point2 = *val;
            else
                return false;
        }

        const auto& p1_operator = p1.get_operator(point1.get_point());
        const auto& p2_operator = p2.get_operator(point2.get_point());

        const auto& p1_info = program.get_operator_info(p1_operator.id());
        const auto& p2_info = program.get_operator_info(p2_operator.id());

        struct reorder_index_t
        {
            size_t index1;
            size_t index2;
        };

        struct swap_index_t
        {
            size_t p1_index;
            size_t p2_index;
        };

        thread_local struct type_resolver_t
        {
            tracked_vector<child_t> children_data_p1;
            tracked_vector<child_t> children_data_p2;
            hashmap_t<type_id, std::vector<size_t>> missing_p1_types;
            hashmap_t<type_id, std::vector<size_t>> missing_p2_types;
            hashset_t<size_t> correct_types;
            hashset_t<size_t> p1_correct_types;
            hashset_t<size_t> p2_correct_types;
            std::vector<reorder_index_t> p1_reorder_types;
            std::vector<reorder_index_t> p2_reorder_types;
            std::vector<swap_index_t> swap_types;
            std::vector<tree_t> temp_trees;

            void print_missing_types()
            {
                for (const auto& [id, v] : missing_p1_types)
                {
                    if (!v.empty())
                    {
                        BLT_INFO("(P1) For type {} missing indexes:", id);
                        for (const auto idx : v)
                            BLT_INFO("\t{}", idx);
                        BLT_INFO("----");
                    }
                }
                for (const auto& [id, v] : missing_p2_types)
                {
                    if (!v.empty())
                    {
                        BLT_INFO("(P2) For type {} missing indexes:", id);
                        for (const auto idx : v)
                            BLT_INFO("\t{}", idx);
                        BLT_INFO("----");
                    }
                }
            }

            std::optional<size_t> get_p1_index(const type_id& id)
            {
                if (!missing_p1_types.contains(id))
                    return {};
                if (missing_p1_types[id].empty())
                    return {};
                auto idx = missing_p1_types[id].back();
                missing_p1_types[id].pop_back();
                return idx;
            }

            std::optional<size_t> get_p2_index(const type_id& id)
            {
                if (!missing_p2_types.contains(id))
                    return {};
                if (missing_p2_types[id].empty())
                    return {};
                auto idx = missing_p2_types[id].back();
                missing_p2_types[id].pop_back();
                return idx;
            }

            [[nodiscard]] bool handled_p1(const size_t index) const
            {
                return correct_types.contains(index) || p1_correct_types.contains(index);
            }

            [[nodiscard]] bool handled_p2(const size_t index) const
            {
                return correct_types.contains(index) || p2_correct_types.contains(index);
            }

            void clear(gp_program& program)
            {
                children_data_p1.clear();
                children_data_p2.clear();
                correct_types.clear();
                p1_correct_types.clear();
                p2_correct_types.clear();
                p1_reorder_types.clear();
                p2_reorder_types.clear();
                swap_types.clear();
                for (auto& tree : temp_trees)
                    tree.clear(program);
                for (auto& [id, v] : missing_p1_types)
                    v.clear();
                for (auto& [id, v] : missing_p2_types)
                    v.clear();
            }
        } resolver;
        resolver.clear(program);

        auto min_size = std::min(p1_info.argument_types.size(), p2_info.argument_types.size());

        // resolve type information
        for (size_t i = 0; i < min_size; i++)
        {
            if (p1_info.argument_types[i] != p2_info.argument_types[i])
            {
                resolver.missing_p1_types[p1_info.argument_types[i].id].push_back(i);
                resolver.missing_p2_types[p2_info.argument_types[i].id].push_back(i);
            } else
                resolver.correct_types.insert(i);
        }

        for (size_t i = min_size; i < p1_info.argument_types.size(); i++)
            resolver.missing_p1_types[p1_info.argument_types[i].id].push_back(i);

        for (size_t i = min_size; i < p2_info.argument_types.size(); i++)
            resolver.missing_p2_types[p2_info.argument_types[i].id].push_back(i);

        // if swaping p1 -> p2 and p2 -> p1, we may already have the types we need just in a different order

        // first, make a list of types which can simply be reordered
        for (size_t i = 0; i < p1_info.argument_types.size(); i++)
        {
            if (resolver.correct_types.contains(i))
                continue;
            if (auto index = resolver.get_p2_index(p1_info.argument_types[i].id))
            {
                resolver.p2_reorder_types.push_back({i, *index});
                resolver.p2_correct_types.insert(i);
            }
        }

        BLT_DEBUG("Operator C1 {} expects types: ", p1_operator.id());
        for (const auto [i, type] : enumerate(p1_info.argument_types))
            BLT_TRACE("{} -> {}", i, type);
        BLT_DEBUG("Operator C2 {} expects types: ", p2_operator.id());
        for (const auto [i, type] : enumerate(p2_info.argument_types))
            BLT_TRACE("{} -> {}", i, type);
        resolver.print_missing_types();

        for (size_t i = 0; i < p2_info.argument_types.size(); i++)
        {
            if (resolver.correct_types.contains(i))
                continue;
            if (auto index = resolver.get_p1_index(p2_info.argument_types[i].id))
            {
                resolver.p1_reorder_types.push_back({i, *index});
                resolver.p1_correct_types.insert(i);
            }
        }

        // next we need to figure out which types need to be swapped
        for (size_t i = 0; i < p1_info.argument_types.size(); i++)
        {
            if (resolver.handled_p2(i))
                continue;
            if (auto index = resolver.get_p1_index(p1_info.argument_types[i].id))
                resolver.swap_types.push_back({*index, i});
        }

        for (size_t i = 0; i < p2_info.argument_types.size(); i++)
        {
            if (resolver.handled_p1(i))
                continue;
            if (auto index = resolver.get_p2_index(p2_info.argument_types[i].id))
                resolver.swap_types.push_back({i, *index});
        }

        // now we do the swap
        p1.find_child_extends(resolver.children_data_p1, point1.get_point(), p1_info.argument_types.size());
        p2.find_child_extends(resolver.children_data_p2, point2.get_point(), p2_info.argument_types.size());

        for (const auto& [index1, index2] : resolver.p1_reorder_types)
        {
            BLT_DEBUG("Reordering in C1: {} -> {}", index1, index2);
            c1.manipulate().easy_manipulator().swap_subtrees(resolver.children_data_p1[index1], c1, resolver.children_data_p1[index2]);
        }

        for (const auto& [index1, index2] : resolver.p2_reorder_types)
        {
            BLT_DEBUG("Reordering in C2: {} -> {}", index1, index2);
            c2.manipulate().easy_manipulator().swap_subtrees(resolver.children_data_p2[index1], c2, resolver.children_data_p2[index2]);
        }

        auto c1_insert = resolver.children_data_p1.back().end;
        auto c2_insert = resolver.children_data_p2.back().end;

        for (const auto& [p1_index, p2_index] : resolver.swap_types)
        {
            if (p1_index < p1_info.argument_types.size() && p2_index < p2_info.argument_types.size())
                c1.manipulate().easy_manipulator().swap_subtrees(resolver.children_data_p1[p1_index], c2, resolver.children_data_p2[p2_index]);
            else if (p1_index < p1_info.argument_types.size() && p2_index >= p2_info.argument_types.size())
            {
                BLT_TRACE("(P1 IS UNDER!) Trying to swap P1 {} for P2 {} (Sizes: P1: {} P2: {})", p1_index, p2_index, p1_info.argument_types.size(), p2_info.argument_types.size());
                BLT_TRACE("Inserting into P2 from P1!");
                c1.manipulate().easy_manipulator().copy_subtree(resolver.children_data_p1[p1_index], resolver.temp_trees[0]);
                c1.manipulate().easy_manipulator().delete_subtree(resolver.children_data_p1[p1_index]);
                c2_insert = c2.manipulate().easy_manipulator().insert_subtree(subtree_point_t{c1_insert}, resolver.temp_trees[0]);
            } else if (p2_index < p2_info.argument_types.size() && p1_index >= p1_info.argument_types.size())
            {
                BLT_TRACE("(P2 IS UNDER!) Trying to swap P1 {} for P2 {} (Sizes: P1: {} P2: {})", p1_index, p2_index, p1_info.argument_types.size(), p2_info.argument_types.size());
            } else
            {
                BLT_WARN("This should be an impossible state!");
            }
        }


        c1.manipulate().easy_manipulator().modify_operator(point1.get_point(), p2_operator.id(), p2_info.return_type);
        c2.manipulate().easy_manipulator().modify_operator(point2.get_point(), p1_operator.id(), p1_info.return_type);

#if BLT_DEBUG_LEVEL >= 2
        if (!c1.check(detail::debug::context_ptr) || !c2.check(detail::debug::context_ptr))
            throw std::runtime_error("Tree check failed");
#endif
        return true;
    }

    bool advanced_crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2)
    {
        if (p1.size() < config.min_tree_size || p2.size() < config.min_tree_size)
            return false;

        // TODO: more crossover!
        switch (program.get_random().get_u32(0, 2))
        {
            // single point crossover (only if operators at this point are "compatible")
        case 0:
            {

                // check if can work
                // otherwise goto case2
            }
            // Mating crossover analogs to same species breeding. Only works if tree is mostly similar
        case 1:
            {
            }
            // Subtree crossover, select random points inside trees and swap their subtrees
        case 2:
            return subtree_crossover_t{}.apply(program, p1, p2, c1, c2);
        default:
#if BLT_DEBUG_LEVEL > 0
            BLT_ABORT("This place should be unreachable!");
#else
            BLT_UNREACHABLE;
#endif
        }

#if BLT_DEBUG_LEVEL >= 2
        if (!c1.check(detail::debug::context_ptr) || !c2.check(detail::debug::context_ptr))
            throw std::runtime_error("Tree check failed");
#endif
    }

    bool mutation_t::apply(gp_program& program, const tree_t&, tree_t& c)
    {
        // TODO: options for this?
        mutate_point(program, c, c.select_subtree());
        return true;
    }

    size_t mutation_t::mutate_point(gp_program& program, tree_t& c, const subtree_point_t node) const
    {
        auto& new_tree = tree_t::get_thread_local(program);
#if BLT_DEBUG_LEVEL >= 2
        auto previous_size = new_tree.size();
        auto previous_bytes = new_tree.total_value_bytes();
#endif
        config.generator.get().generate(new_tree, {program, node.get_type(), config.replacement_min_depth, config.replacement_max_depth});

#if BLT_DEBUG_LEVEL >= 2
        const auto old_op = c.get_operator(node.get_point());
        if (!new_tree.check(detail::debug::context_ptr))
        {
            BLT_ERROR("Mutate point new tree check failed!");
            BLT_ERROR("Old Op: {} got replaced with New Op: {}", program.get_name(old_op.id()).value_or("Unknown"),
                      program.get_name(new_tree.get_operator(0).id()).value_or("Unknown"));
            BLT_ERROR("Tree started with size: {} and bytes: {}", previous_size, previous_bytes);
            throw std::runtime_error("Mutate Point tree check failed");
        }
#endif

        c.manipulate().easy_manipulator().replace_subtree(node, new_tree);

        // this will check to make sure that the tree is in a correct and executable state. it requires that the evaluation is context free!
#if BLT_DEBUG_LEVEL >= 2
        const auto new_op = c.get_operator(node.get_point());
        if (!c.check(detail::debug::context_ptr))
        {
            print_mutate_stats();
            BLT_ERROR("Old Op: {} got replaced with New Op: {}", program.get_name(old_op.id()).value_or("Unknown"),
                      program.get_name(new_op.id()).value_or("Unknown"));
            throw std::runtime_error("Mutate Point tree check failed");
        }
#endif
#if defined(BLT_TRACK_ALLOCATIONS) || BLT_DEBUG_LEVEL >= 2
        ++mutate_point_counter;
#endif
        return node.get_point() + new_tree.size();
    }

    bool advanced_mutation_t::apply(gp_program& program, [[maybe_unused]] const tree_t& p, tree_t& c)
    {
        for (size_t c_node = 0; c_node < c.size(); c_node++)
        {
#if BLT_DEBUG_LEVEL >= 2
            auto c_copy = c;
#endif
            if (!program.get_random().choice(per_node_mutation_chance / static_cast<double>(c.size())))
                continue;

            // select an operator to apply
            auto selected_point = static_cast<i32>(mutation_operator::COPY);
            auto choice = program.get_random().get_double();

            for (const auto& [index, value] : enumerate(mutation_operator_chances))
            {
                if (choice <= value)
                {
                    selected_point = static_cast<i32>(index);
                    break;
                }
            }

            switch (static_cast<mutation_operator>(selected_point))
            {
            case mutation_operator::EXPRESSION:
                c_node += mutate_point(program, c, c.subtree_from_point(static_cast<ptrdiff_t>(c_node)));
#if BLT_TRACK_ALLOCATIONS || BLT_DEBUG_LEVEL >= 2
                ++mutate_expression_counter;
#endif
                break;
            case mutation_operator::ADJUST:
                {
                    // this is going to be evil >:3
                    const auto& node = c.get_operator(c_node);
                    if (!node.is_value())
                    {
                        auto& current_func_info = program.get_operator_info(node.id());
                        operator_id random_replacement = program.get_random().select(
                            program.get_type_non_terminals(current_func_info.return_type.id));
                        auto& replacement_func_info = program.get_operator_info(random_replacement);

                        // cache memory used for offset data.
                        thread_local tracked_vector<child_t> children_data;
                        children_data.clear();

                        c.find_child_extends(children_data, c_node, current_func_info.argument_types.size());

                        for (const auto& [index, val] : blt::enumerate(replacement_func_info.argument_types))
                        {
                            // need to generate replacement.
                            if (index < current_func_info.argument_types.size() && val.id != current_func_info.argument_types[index].id)
                            {
                                // TODO: new config?
                                auto& tree = tree_t::get_thread_local(program);
                                config.generator.get().generate(tree,
                                                                {program, val.id, config.replacement_min_depth, config.replacement_max_depth});

                                auto& [child_start, child_end] = children_data[children_data.size() - 1 - index];
                                c.manipulate().easy_manipulator().replace_subtree(c.subtree_from_point(child_start), child_end, tree);

                                // shift over everybody after.
                                if (index > 0)
                                {
                                    // don't need to update if the index is the last
                                    for (auto& new_child : iterate(children_data.end() - static_cast<ptrdiff_t>(index),
                                                                   children_data.end()))
                                    {
                                        // remove the old tree size, then add the new tree size to get the correct positions.
                                        new_child.start =
                                            new_child.start - (child_end - child_start) +
                                            static_cast<ptrdiff_t>(tree.size());
                                        new_child.end =
                                            new_child.end - (child_end - child_start) + static_cast<ptrdiff_t>(tree.size());
                                    }
                                }
                                child_end = static_cast<ptrdiff_t>(child_start + tree.size());
                            }
                        }

                        if (current_func_info.argc.argc > replacement_func_info.argc.argc)
                        {
                            auto end_index = children_data[(current_func_info.argc.argc - replacement_func_info.argc.argc) - 1].end;
                            auto start_index = children_data.begin()->start;
                            c.manipulate().easy_manipulator().delete_subtree(subtree_point_t(start_index), end_index);
                        }
                        else if (current_func_info.argc.argc == replacement_func_info.argc.argc)
                        {
                            // exactly enough args
                            // return types should have been replaced if needed. this part should do nothing?
                        }
                        else
                        {
                            // not enough args
                            size_t start_index = c_node + 1;
                            // size_t total_bytes_after = c.total_value_bytes(start_index);
                            // TODO: transactions?
                            // auto move = c.temporary_move(total_bytes_after);

                            for (ptrdiff_t i = static_cast<ptrdiff_t>(replacement_func_info.argc.argc) - 1;
                                 i >= current_func_info.argc.argc; i--)
                            {
                                auto& tree = tree_t::get_thread_local(program);
                                config.generator.get().generate(tree,
                                                                {
                                                                    program, replacement_func_info.argument_types[i].id, config.replacement_min_depth,
                                                                    config.replacement_max_depth
                                                                });
                                start_index = c.manipulate().easy_manipulator().insert_subtree(subtree_point_t(static_cast<ptrdiff_t>(start_index)), tree);
                            }
                        }
                        // now finally update the type.
                        c.manipulate().easy_manipulator().modify_operator(c_node, random_replacement, replacement_func_info.return_type);
                    }
#if BLT_DEBUG_LEVEL >= 2
                    if (!c.check(detail::debug::context_ptr))
                    {
                        std::cout << "Parent: " << std::endl;
                        c_copy.print(std::cout, false, false);
                        std::cout << "Child Values:" << std::endl;
                        c.print(std::cout, false, false);
                        std::cout << std::endl;
                        print_mutate_stats();
                        BLT_ABORT("Adjust Tree Check Failed.");
                    }
#endif
#if defined(BLT_TRACK_ALLOCATIONS) || BLT_DEBUG_LEVEL >= 2
                    ++mutate_adjust_counter;
#endif
                }
                break;
            case mutation_operator::SUB_FUNC:
                {
                    auto& current_func_info = program.get_operator_info(c.get_operator(c_node).id());

                    // need to:
                    // mutate the current function.
                    // current function is moved to one of the arguments.
                    // other arguments are generated.

                    // get a replacement which returns the same type.
                    auto& non_terminals = program.get_type_non_terminals(current_func_info.return_type.id);
                    if (non_terminals.empty())
                        continue;
                    operator_id random_replacement = program.get_random().select(non_terminals);
                    size_t arg_position = 0;
                    do
                    {
                        auto& replacement_func_info = program.get_operator_info(random_replacement);
                        for (const auto& [index, v] : enumerate(replacement_func_info.argument_types))
                        {
                            if (v.id == current_func_info.return_type.id)
                            {
                                arg_position = index;
                                goto exit;
                            }
                        }
                        random_replacement = program.get_random().select(program.get_type_non_terminals(current_func_info.return_type.id));
                    }
                    while (true);
                exit:
                    auto& replacement_func_info = program.get_operator_info(random_replacement);
                    auto new_argc = replacement_func_info.argc.argc;
                    // replacement function should be valid. let's make a copy of us.
                    auto current_end = c.find_endpoint(static_cast<ptrdiff_t>(c_node));
                    // size_t for_bytes = c.total_value_bytes(c_node, current_end);
                    // size_t after_bytes = c.total_value_bytes(current_end);
                    auto size = current_end - c_node;

                    // auto combined_ptr = get_thread_pointer_for_size<struct SUB_FUNC_FOR>(for_bytes + after_bytes);

                    // vals.copy_to(combined_ptr, for_bytes + after_bytes);
                    // vals.pop_bytes(static_cast<ptrdiff_t>(for_bytes + after_bytes));

                    size_t start_index = c_node;
                    for (ptrdiff_t i = new_argc - 1; i > static_cast<ptrdiff_t>(arg_position); i--)
                    {
                        auto& tree = tree_t::get_thread_local(program);
                        config.generator.get().generate(tree,
                                                        {
                                                            program, replacement_func_info.argument_types[i].id, config.replacement_min_depth,
                                                            config.replacement_max_depth
                                                        });
                        start_index = c.manipulate().easy_manipulator().insert_subtree(subtree_point_t(static_cast<ptrdiff_t>(start_index)), tree);
                    }
                    start_index += size;
                    // vals.copy_from(combined_ptr, for_bytes);
                    for (blt::ptrdiff_t i = static_cast<blt::ptrdiff_t>(arg_position) - 1; i >= 0; i--)
                    {
                        auto& tree = tree_t::get_thread_local(program);
                        config.generator.get().generate(tree,
                                                        {
                                                            program, replacement_func_info.argument_types[i].id, config.replacement_min_depth,
                                                            config.replacement_max_depth
                                                        });
                        start_index = c.manipulate().easy_manipulator().insert_subtree(subtree_point_t(static_cast<ptrdiff_t>(start_index)), tree);
                    }
                    // vals.copy_from(combined_ptr + for_bytes, after_bytes);

                    c.insert_operator(c_node, {
                                          program.get_typesystem().get_type(replacement_func_info.return_type).size(),
                                          random_replacement,
                                          program.is_operator_ephemeral(random_replacement),
                                          program.get_operator_flags(random_replacement)
                                      });
#if BLT_DEBUG_LEVEL >= 2
                    if (!c.check(detail::debug::context_ptr))
                    {
                        std::cout << "Parent: " << std::endl;
                        p.print(std::cout, false, false);
                        std::cout << "Child:" << std::endl;
                        c.print(std::cout, false, false);
                        std::cout << std::endl;
                        print_mutate_stats();
                        BLT_ABORT("SUB_FUNC Tree Check Failed.");
                    }
#endif
#if defined(BLT_TRACK_ALLOCATIONS) || BLT_DEBUG_LEVEL >= 2
                    ++mutate_sub_func_counter;
#endif
                }
                break;
            case mutation_operator::JUMP_FUNC:
                {
                    auto& info = program.get_operator_info(c.get_operator(c_node).id());
                    size_t argument_index = -1ul;
                    for (const auto& [index, v] : enumerate(info.argument_types))
                    {
                        if (v.id == info.return_type.id)
                        {
                            argument_index = index;
                            break;
                        }
                    }
                    if (argument_index == -1ul)
                        continue;

                    thread_local tracked_vector<child_t> child_data;
                    child_data.clear();

                    c.find_child_extends(child_data, c_node, info.argument_types.size());

                    auto child_index = child_data.size() - 1 - argument_index;
                    const auto child = child_data[child_index];

                    thread_local tree_t child_tree{program};

                    c.manipulate().easy_manipulator().copy_subtree(subtree_point_t(child.start), child.end, child_tree);
                    c.manipulate().easy_manipulator().delete_subtree(subtree_point_t(static_cast<ptrdiff_t>(c_node)));
                    c.manipulate().easy_manipulator().insert_subtree(subtree_point_t(static_cast<ptrdiff_t>(c_node)), child_tree);
                    child_tree.clear(program);

                    // auto for_bytes = c.total_value_bytes(child.start, child.end);
                    // auto after_bytes = c.total_value_bytes(child_data.back().end);

                    // auto storage_ptr = get_thread_pointer_for_size<struct jump_func>(for_bytes + after_bytes);
                    // vals.copy_to(storage_ptr + for_bytes, after_bytes);
                    // vals.pop_bytes(static_cast<blt::ptrdiff_t>(after_bytes));
                    //
                    // for (auto i = static_cast<blt::ptrdiff_t>(child_data.size() - 1); i > static_cast<blt::ptrdiff_t>(child_index); i--)
                    // {
                    //     auto& cc = child_data[i];
                    //     auto bytes = c.total_value_bytes(cc.start, cc.end);
                    //     vals.pop_bytes(static_cast<blt::ptrdiff_t>(bytes));
                    //     ops.erase(ops.begin() + cc.start, ops.begin() + cc.end);
                    // }
                    // vals.copy_to(storage_ptr, for_bytes);
                    // vals.pop_bytes(static_cast<blt::ptrdiff_t>(for_bytes));
                    // for (auto i = static_cast<blt::ptrdiff_t>(child_index - 1); i >= 0; i--)
                    // {
                    //     auto& cc = child_data[i];
                    //     auto bytes = c.total_value_bytes(cc.start, cc.end);
                    //     vals.pop_bytes(static_cast<blt::ptrdiff_t>(bytes));
                    //     ops.erase(ops.begin() + cc.start, ops.begin() + cc.end);
                    // }
                    // ops.erase(ops.begin() + static_cast<blt::ptrdiff_t>(c_node));
                    // vals.copy_from(storage_ptr, for_bytes + after_bytes);

#if BLT_DEBUG_LEVEL >= 2
                    if (!c.check(detail::debug::context_ptr))
                    {
                        std::cout << "Parent: " << std::endl;
                        p.print(std::cout, false, false, false, static_cast<ptrdiff_t>(c_node));
                        std::cout << "Child Values:" << std::endl;
                        c.print(std::cout, false, false);
                        std::cout << std::endl;
                        BLT_ERROR("Failed at mutation index %lu/%lu", c_node, c.size());
                        print_mutate_stats();
                        BLT_ABORT("JUMP_FUNC Tree Check Failed.");
                    }
#endif
#if defined(BLT_TRACK_ALLOCATIONS) || BLT_DEBUG_LEVEL >= 2
                    ++mutate_jump_counter;
#endif
                }
                break;
            case mutation_operator::COPY:
                {
                    auto& info = program.get_operator_info(c.get_operator(c_node).id());
                    if (c.get_operator(c_node).is_value())
                        continue;
                    thread_local tracked_vector<size_t> potential_indexes;
                    potential_indexes.clear();

                    const auto from_index = program.get_random().get_u64(0, info.argument_types.size());
                    for (const auto [index, type] : enumerate(info.argument_types))
                    {
                        if (index == from_index)
                            continue;
                        if (info.argument_types[from_index] == type)
                            potential_indexes.push_back(index);
                    }
                    if (potential_indexes.empty())
                        continue;
                    const auto to_index = program.get_random().select(potential_indexes);

                    thread_local tracked_vector<child_t> child_data;
                    child_data.clear();

                    c.find_child_extends(child_data, c_node, info.argument_types.size());

                    const auto child_from_index = child_data.size() - 1 - from_index;
                    const auto child_to_index = child_data.size() - 1 - to_index;
                    const auto& [from_start, from_end] = child_data[child_from_index];
                    const auto& [to_start, to_end] = child_data[child_to_index];

                    thread_local tree_t copy_tree{program};
                    c.manipulate().easy_manipulator().copy_subtree(subtree_point_t{from_start}, from_end, copy_tree);
                    c.manipulate().easy_manipulator().replace_subtree(subtree_point_t{to_start}, to_end, copy_tree);
                    copy_tree.clear(program);

#if BLT_DEBUG_LEVEL >= 2
                    if (!c.check(detail::debug::context_ptr))
                    {
                        std::cout << "Parent: " << std::endl;
                        p.print(std::cout, false, false);
                        std::cout << "Child Values:" << std::endl;
                        c.print(std::cout, false, false);
                        std::cout << std::endl;
                        print_mutate_stats();
                        BLT_ABORT("COPY Tree Check Failed.");
                    }
#endif
#if defined(BLT_TRACK_ALLOCATIONS) || BLT_DEBUG_LEVEL >= 2
                    ++mutate_copy_counter;
#endif

                    // size_t from_bytes = c.total_value_bytes(from_child.start, from_child.end);
                    // size_t after_from_bytes = c.total_value_bytes(from_child.end);
                    // size_t to_bytes = c.total_value_bytes(to_child.start, to_child.end);
                    // size_t after_to_bytes = c.total_value_bytes(to_child.end);
                    //
                    // auto after_bytes = std::max(after_from_bytes, after_to_bytes);
                    //
                    // auto from_ptr = get_thread_pointer_for_size<struct copy>(from_bytes);
                    // auto after_ptr = get_thread_pointer_for_size<struct copy_after>(after_bytes);
                    //
                    // vals.copy_to(after_ptr, after_from_bytes);
                    // vals.pop_bytes(static_cast<blt::ptrdiff_t>(after_from_bytes));
                    // vals.copy_to(from_ptr, from_bytes);
                    // vals.copy_from(after_ptr, after_from_bytes);
                    //
                    // vals.copy_to(after_ptr, after_to_bytes);
                    // vals.pop_bytes(static_cast<blt::ptrdiff_t>(after_to_bytes + to_bytes));
                    //
                    // vals.copy_from(from_ptr, from_bytes);
                    // vals.copy_from(after_ptr, after_to_bytes);
                    //
                    // static thread_local tracked_vector<op_container_t> op_copy;
                    // op_copy.clear();
                    // op_copy.insert(op_copy.begin(), ops.begin() + from_child.start, ops.begin() + from_child.end);
                    //
                    // ops.erase(ops.begin() + to_child.start, ops.begin() + to_child.end);
                    // ops.insert(ops.begin() + to_child.start, op_copy.begin(), op_copy.end());
                }
                break;
            case mutation_operator::END:
            default:
#if BLT_DEBUG_LEVEL > 1
                BLT_ABORT("You shouldn't be able to get here!");
#else
                BLT_UNREACHABLE;
#endif
            }
        }

#if BLT_DEBUG_LEVEL >= 2
        if (!c.check(detail::debug::context_ptr))
        {
            std::cout << "Parent: " << std::endl;
            p.print(std::cout, false, false);
            std::cout << "Child Values:" << std::endl;
            c.print(std::cout, false, false);
            std::cout << std::endl;
            BLT_ABORT("Advanced Mutation Tree Check Failed.");
        }
#endif

        return true;
    }
}
