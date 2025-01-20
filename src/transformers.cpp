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
#if BLT_DEBUG_LEVEL >= 2
    std::atomic_uint64_t mutate_point_counter = 0;
    std::atomic_uint64_t mutate_expression_counter = 0;
    std::atomic_uint64_t mutate_adjust_counter = 0;
    std::atomic_uint64_t mutate_sub_func_counter = 0;
    std::atomic_uint64_t mutate_jump_counter = 0;
    std::atomic_uint64_t mutate_copy_counter = 0;

    inline void print_mutate_stats()
    {
        std::cerr << "Mutation statistics:" << std::endl;
        std::cerr << "\tSuccessful Point Mutations: " << mutate_point_counter << std::endl;
        std::cerr << "\tSuccessful Expression Mutations: " << mutate_expression_counter << std::endl;
        std::cerr << "\tSuccessful Adjust Mutations: " << mutate_adjust_counter << std::endl;
        std::cerr << "\tSuccessful Sub Func Mutations: " << mutate_sub_func_counter << std::endl;
        std::cerr << "\tSuccessful Func Jump Mutations: " << mutate_jump_counter << std::endl;
        std::cerr << "\tSuccessful Copy Mutations: " << mutate_copy_counter << std::endl;
    }
#endif

    grow_generator_t grow_generator;

    inline tree_t& get_static_tree_tl(gp_program& program)
    {
        thread_local tree_t new_tree{program};
        new_tree.clear(program);
        return new_tree;
    }

    // TODO: consolidate the two copies of this. other is in tree.cpp
    template <typename>
    static u8* get_thread_pointer_for_size(const size_t bytes)
    {
        thread_local expanding_buffer<u8> buffer;
        if (bytes > buffer.size())
            buffer.resize(bytes);
        return buffer.data();
    }

    mutation_t::config_t::config_t(): generator(grow_generator)
    {
    }

    bool crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2) // NOLINT
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

        // TODO: more crossover!
        switch (program.get_random().get_u32(0, 2))
        {
        case 0:
        case 1:
            c1.swap_subtrees(point->p1_crossover_point, c2, point->p2_crossover_point);
            break;
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

        return true;
    }

    std::optional<crossover_t::crossover_point_t> crossover_t::get_crossover_point(const tree_t& c1,
                                                                                   const tree_t& c2) const
    {
        auto first = c1.select_subtree(config.terminal_chance);
        auto second = c2.select_subtree(first.type, config.max_crossover_tries, config.terminal_chance);

        if (!second)
            return {};

        return {{first, *second}};
    }

    std::optional<crossover_t::crossover_point_t> crossover_t::get_crossover_point_traverse(const tree_t& c1,
                                                                                            const tree_t& c2) const
    {
        auto c1_point_o = get_point_traverse_retry(c1, {});
        if (!c1_point_o)
            return {};
        auto c2_point_o = get_point_traverse_retry(c2, c1_point_o->type);
        if (!c2_point_o)
            return {};
        return {{*c1_point_o, *c2_point_o}};
    }

    std::optional<tree_t::subtree_point_t> crossover_t::get_point_traverse_retry(const tree_t& t, const std::optional<type_id> type) const
    {
        if (type)
            return t.select_subtree_traverse(*type, config.max_crossover_tries, config.terminal_chance, config.depth_multiplier);
        return t.select_subtree_traverse(config.terminal_chance, config.depth_multiplier);
    }

    bool mutation_t::apply(gp_program& program, const tree_t&, tree_t& c)
    {
        // TODO: options for this?
        mutate_point(program, c, c.select_subtree());
        return true;
    }

    size_t mutation_t::mutate_point(gp_program& program, tree_t& c, const tree_t::subtree_point_t node) const
    {
        auto& new_tree = get_static_tree_tl(program);
        config.generator.get().generate(new_tree, {program, node.type, config.replacement_min_depth, config.replacement_max_depth});

        c.replace_subtree(node, new_tree);

        // this will check to make sure that the tree is in a correct and executable state. it requires that the evaluation is context free!
#if BLT_DEBUG_LEVEL >= 2
        if (!c.check(detail::debug::context_ptr))
        {
            print_mutate_stats();
            throw std::runtime_error("Mutate Point tree check failed");
        }
        ++mutate_point_counter;
#endif
        return node.pos + new_tree.size();
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
            for (const auto& [index, value] : blt::enumerate(mutation_operator_chances))
            {
                if (index == 0)
                {
                    if (choice <= value)
                    {
                        selected_point = static_cast<blt::i32>(index);
                        break;
                    }
                }
                else
                {
                    if (choice > mutation_operator_chances[index - 1] && choice <= value)
                    {
                        selected_point = static_cast<blt::i32>(index);
                        break;
                    }
                }
            }

            switch (static_cast<mutation_operator>(selected_point))
            {
            case mutation_operator::EXPRESSION:
                c_node += mutate_point(program, c, c.subtree_from_point(static_cast<ptrdiff_t>(c_node)));
#if BLT_DEBUG_LEVEL >= 2
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
                        thread_local tracked_vector<tree_t::child_t> children_data;
                        children_data.clear();

                        c.find_child_extends(children_data, c_node, current_func_info.argument_types.size());

                        for (const auto& [index, val] : blt::enumerate(replacement_func_info.argument_types))
                        {
                            // need to generate replacement.
                            if (index < current_func_info.argument_types.size() && val.id != current_func_info.argument_types[index].id)
                            {
                                // TODO: new config?
                                auto& tree = get_static_tree_tl(program);
                                config.generator.get().generate(tree,
                                                                {program, val.id, config.replacement_min_depth, config.replacement_max_depth});

                                auto& [child_start, child_end] = children_data[children_data.size() - 1 - index];
                                c.replace_subtree(c.subtree_from_point(child_start), child_end, tree);

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

#if BLT_DEBUG_LEVEL >= 2
                                if (!c.check(detail::debug::context_ptr))
                                {
                                    print_mutate_stats();
                                    throw std::runtime_error("Adjust Tree check failed");
                                }
                                ++mutate_adjust_counter;
#endif
                            }
                        }

                        if (current_func_info.argc.argc > replacement_func_info.argc.argc)
                        {
                            auto end_index = children_data[(current_func_info.argc.argc - replacement_func_info.argc.argc) - 1].end;
                            auto start_index = children_data.begin()->start;
                            c.delete_subtree(tree_t::subtree_point_t(start_index), end_index);
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
                                auto& tree = get_static_tree_tl(program);
                                config.generator.get().generate(tree,
                                                                {
                                                                    program, replacement_func_info.argument_types[i].id, config.replacement_min_depth,
                                                                    config.replacement_max_depth
                                                                });
                                start_index = c.insert_subtree(tree_t::subtree_point_t(static_cast<ptrdiff_t>(start_index)), tree);
                            }
                        }
                        // now finally update the type.
                        c.modify_operator(c_node, random_replacement, replacement_func_info.return_type);
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
                        auto& tree = get_static_tree_tl(program);
                        config.generator.get().generate(tree,
                                                        {
                                                            program, replacement_func_info.argument_types[i].id, config.replacement_min_depth,
                                                            config.replacement_max_depth
                                                        });
                        start_index = c.insert_subtree(tree_t::subtree_point_t(static_cast<ptrdiff_t>(start_index)), tree);
                    }
                    start_index += size;
                    // vals.copy_from(combined_ptr, for_bytes);
                    for (blt::ptrdiff_t i = static_cast<blt::ptrdiff_t>(arg_position) - 1; i >= 0; i--)
                    {
                        auto& tree = get_static_tree_tl(program);
                        config.generator.get().generate(tree,
                                                        {
                                                            program, replacement_func_info.argument_types[i].id, config.replacement_min_depth,
                                                            config.replacement_max_depth
                                                        });
                        start_index = c.insert_subtree(tree_t::subtree_point_t(static_cast<ptrdiff_t>(start_index)), tree);
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

                    thread_local tracked_vector<tree_t::child_t> child_data;
                    child_data.clear();

                    c.find_child_extends(child_data, c_node, info.argument_types.size());

                    auto child_index = child_data.size() - 1 - argument_index;
                    const auto child = child_data[child_index];

                    thread_local tree_t child_tree{program};

                    c.copy_subtree(tree_t::subtree_point_t(child.start), child.end, child_tree);
                    c.delete_subtree(tree_t::subtree_point_t(static_cast<ptrdiff_t>(c_node)));
                    c.insert_subtree(tree_t::subtree_point_t(static_cast<ptrdiff_t>(c_node)), child_tree);
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
                    ++mutate_jump_counter;
#endif
                }
                break;
            case mutation_operator::COPY:
                {
                    auto& info = program.get_operator_info(c.get_operator(c_node).id());
                    size_t pt = -1ul;
                    size_t pf = -1ul;
                    for (const auto& [index, v] : blt::enumerate(info.argument_types))
                    {
                        for (size_t i = index + 1; i < info.argument_types.size(); i++)
                        {
                            auto& v1 = info.argument_types[i];
                            if (v == v1)
                            {
                                if (pt == -1ul)
                                    pt = index;
                                else
                                    pf = index;
                                break;
                            }
                        }
                        if (pt != -1ul && pf != -1ul)
                            break;
                    }
                    if (pt == -1ul || pf == -1ul)
                        continue;

                    size_t from = 0;
                    size_t to = 0;

                    if (program.get_random().choice())
                    {
                        from = pt;
                        to = pf;
                    }
                    else
                    {
                        from = pf;
                        to = pt;
                    }

                    thread_local tracked_vector<tree_t::child_t> child_data;
                    child_data.clear();

                    c.find_child_extends(child_data, c_node, info.argument_types.size());

                    auto from_index = child_data.size() - 1 - from;
                    auto to_index = child_data.size() - 1 - to;
                    auto& from_child = child_data[from_index];
                    auto& to_child = child_data[to_index];

                    thread_local tree_t copy_tree{program};
                    c.copy_subtree(tree_t::subtree_point_t{from_child.start}, from_child.end, copy_tree);
                    c.replace_subtree(tree_t::subtree_point_t{to_child.start}, to_child.end, copy_tree);
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
