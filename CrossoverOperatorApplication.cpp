bool type_aware_crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2)
{
    if (p1.size() < config.min_tree_size || p2.size() < config.min_tree_size)
        return false;

    tree_t::subtree_point_t point1, point2;
    if (config.traverse)
    {
        point1 = p1.select_subtree_traverse(config.terminal_chance, config.depth_multiplier);
        if (const auto val = p2.select_subtree_traverse(point1.type, config.max_crossover_tries, config.terminal_chance, config.depth_multiplier))
            point2 = *val;
        else
            return false;
    } else
    {
        point1 = p1.select_subtree(config.terminal_chance);
        if (const auto val = p2.select_subtree(point1.type, config.max_crossover_tries, config.terminal_chance))
            point2 = *val;
        else
            return false;
    }

    const auto& p1_operator = p1.get_operator(point1.pos);
    const auto& p2_operator = p2.get_operator(point2.pos);

    // If either is a terminal (value), just do normal subtree crossover
    if (p1_operator.is_value() || p2_operator.is_value())
    {
        c1.swap_subtrees(point1, c2, point2);
        return true;
    }

    const auto& p1_info = program.get_operator_info(p1_operator.id());
    const auto& p2_info = program.get_operator_info(p2_operator.id());

    // Find the child subtrees of both operators
    thread_local tracked_vector<tree_t::child_t> children_data_p1;
    thread_local tracked_vector<tree_t::child_t> children_data_p2;
    children_data_p1.clear();
    children_data_p2.clear();
    
    p1.find_child_extends(children_data_p1, point1.pos, p1_info.argument_types.size());
    p2.find_child_extends(children_data_p2, point2.pos, p2_info.argument_types.size());

    // Check if all types are identical but possibly in different order
    bool same_types_different_order = p1_info.argument_types.size() == p2_info.argument_types.size();
    
    if (same_types_different_order)
    {
        // Create frequency counts of types in both operators
        std::unordered_map<type_id, size_t> type_counts_p1;
        std::unordered_map<type_id, size_t> type_counts_p2;
        
        for (const auto& type : p1_info.argument_types)
            type_counts_p1[type.id]++;
            
        for (const auto& type : p2_info.argument_types)
            type_counts_p2[type.id]++;
            
        // Check if the type counts match
        for (const auto& [type, count] : type_counts_p1)
        {
            if (type_counts_p2[type] != count)
            {
                same_types_different_order = false;
                break;
            }
        }
    }

    if (same_types_different_order)
    {
        // Create a mapping from p1's argument positions to p2's positions
        std::vector<size_t> arg_mapping(p1_info.argument_types.size(), (size_t)-1);
        std::vector<bool> p2_used(p2_info.argument_types.size(), false);
        
        // First pass: match exact types in order
        for (size_t i = 0; i < p1_info.argument_types.size(); i++)
        {
            for (size_t j = 0; j < p2_info.argument_types.size(); j++)
            {
                if (!p2_used[j] && p1_info.argument_types[i].id == p2_info.argument_types[j].id)
                {
                    arg_mapping[i] = j;
                    p2_used[j] = true;
                    break;
                }
            }
        }
        
        // Copy operators first
        auto& c1_temp = tree_t::get_thread_local(program);
        auto& c2_temp = tree_t::get_thread_local(program);
        c1_temp.clear(program);
        c2_temp.clear(program);
        
        // Create new operators with the same return types
        c1_temp.insert_operator({
            program.get_typesystem().get_type(p2_info.return_type).size(),
            p2_operator.id(),
            program.is_operator_ephemeral(p2_operator.id()),
            program.get_operator_flags(p2_operator.id())
        });
        
        c2_temp.insert_operator({
            program.get_typesystem().get_type(p1_info.return_type).size(),
            p1_operator.id(),
            program.is_operator_ephemeral(p1_operator.id()),
            program.get_operator_flags(p1_operator.id())
        });
        
        // Copy child subtrees according to the mapping
        for (size_t i = 0; i < p1_info.argument_types.size(); i++)
        {
            auto& p1_child = children_data_p1[i];
            auto& p2_child = children_data_p2[arg_mapping[i]];
            
            tree_t p1_subtree(program);
            tree_t p2_subtree(program);
            
            p1.copy_subtree(tree_t::subtree_point_t(p1_child.start), p1_child.end, p1_subtree);
            p2.copy_subtree(tree_t::subtree_point_t(p2_child.start), p2_child.end, p2_subtree);
            
            c1_temp.insert_subtree(tree_t::subtree_point_t(c1_temp.size()), p2_subtree);
            c2_temp.insert_subtree(tree_t::subtree_point_t(c2_temp.size()), p1_subtree);
        }
        
        // Replace the original subtrees with our new reordered ones
        c1.replace_subtree(point1, c1_temp);
        c2.replace_subtree(point2, c2_temp);
    }
    else
    {
        // If types don't match exactly, fall back to simple operator swap
        // but we need to ensure the children are compatible
        
        // Create new operators with swapped operators but appropriate children
        auto& c1_temp = tree_t::get_thread_local(program);
        auto& c2_temp = tree_t::get_thread_local(program);
        c1_temp.clear(program);
        c2_temp.clear(program);
        
        c1_temp.insert_operator({
            program.get_typesystem().get_type(p2_info.return_type).size(),
            p2_operator.id(),
            program.is_operator_ephemeral(p2_operator.id()),
            program.get_operator_flags(p2_operator.id())
        });
        
        c2_temp.insert_operator({
            program.get_typesystem().get_type(p1_info.return_type).size(),
            p1_operator.id(),
            program.is_operator_ephemeral(p1_operator.id()),
            program.get_operator_flags(p1_operator.id())
        });
        
        // Create a mapping of which children we can reuse and which need to be regenerated
        for (size_t i = 0; i < p2_info.argument_types.size(); i++)
        {
            const auto& needed_type = p2_info.argument_types[i];
            bool found_match = false;
            
            // Try to find a matching child from p1
            for (size_t j = 0; j < p1_info.argument_types.size(); j++)
            {
                if (needed_type.id == p1_info.argument_types[j].id)
                {
                    // Copy this child subtree from p1
                    auto& p1_child = children_data_p1[j];
                    tree_t p1_subtree(program);
                    p1.copy_subtree(tree_t::subtree_point_t(p1_child.start), p1_child.end, p1_subtree);
                    c1_temp.insert_subtree(tree_t::subtree_point_t(c1_temp.size()), p1_subtree);
                    found_match = true;
                    break;
                }
            }
            
            if (!found_match)
            {
                // If no matching child, we need to generate a new subtree of the correct type
                auto& tree = tree_t::get_thread_local(program);
                tree.clear(program);
                config.generator.get().generate(tree, {program, needed_type.id, config.replacement_min_depth, config.replacement_max_depth});
                c1_temp.insert_subtree(tree_t::subtree_point_t(c1_temp.size()), tree);
            }
        }
        
        // Do the same for the other direction (c2)
        for (size_t i = 0; i < p1_info.argument_types.size(); i++)
        {
            const auto& needed_type = p1_info.argument_types[i];
            bool found_match = false;
            
            // Try to find a matching child from p2
            for (size_t j = 0; j < p2_info.argument_types.size(); j++)
            {
                if (needed_type.id == p2_info.argument_types[j].id)
                {
                    // Copy this child subtree from p2
                    auto& p2_child = children_data_p2[j];
                    tree_t p2_subtree(program);
                    p2.copy_subtree(tree_t::subtree_point_t(p2_child.start), p2_child.end, p2_subtree);
                    c2_temp.insert_subtree(tree_t::subtree_point_t(c2_temp.size()), p2_subtree);
                    found_match = true;
                    break;
                }
            }
            
            if (!found_match)
            {
                // If no matching child, we need to generate a new subtree of the correct type
                auto& tree = tree_t::get_thread_local(program);
                tree.clear(program);
                config.generator.get().generate(tree, {program, needed_type.id, config.replacement_min_depth, config.replacement_max_depth});
                c2_temp.insert_subtree(tree_t::subtree_point_t(c2_temp.size()), tree);
            }
        }
        
        // Replace the original subtrees with our new ones
        c1.replace_subtree(point1, c1_temp);
        c2.replace_subtree(point2, c2_temp);
    }

#if BLT_DEBUG_LEVEL >= 2
    if (!c1.check(detail::debug::context_ptr) || !c2.check(detail::debug::context_ptr))
        throw std::runtime_error("Tree check failed");
#endif

    return true;
}