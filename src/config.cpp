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
#include <blt/gp/config.h>
#include <blt/parse/argparse_v2.h>
#include "blt/std/string.h"
#include <blt/gp/selection.h>

namespace blt::gp
{
    inline void setup_crossover_parser(argparse::argument_parser_t& parser)
    {
        parser.add_flag("--max_crossover_tries").set_dest("max_crossover_tries").set_default(5u).as_type<u32>().set_help(
            "number of times crossover will try to pick a valid point in the tree.");
        parser.add_flag("--max_crossover_iterations").set_dest("max_crossover_iterations").set_default(10u).as_type<u32>().set_help(
            "how many times the crossover function can fail before we will skip this operation.");
        parser.add_flag("--min_tree_size").set_dest("min_tree_size").set_default(5u).as_type<u32>().set_help("the minimum size of"
            " the tree to be considered for crossover.");
        parser.add_flag("--depth_multiplier").set_dest("depth_multiplier").set_default(0.25f).as_type<float>().set_help(
            "at each depth level, what chance do we have to exit with this as our point?");
        parser.add_flag("--terminal_chance").set_dest("terminal_chance").set_default(0.1f).as_type<float>().set_help(
            "how often should we select terminals over functions. By default, we only allow selection of terminals 10% of the time. "
            "This applies to both types of crossover point functions. Traversal will use the parent if it should not pick a terminal.");
        parser.add_flag("--traverse").set_dest("traverse").make_flag().set_help(" use traversal to select instead of random point selection.");
    }

    inline crossover_t::config_t create_crossover_config(const argparse::argument_storage_t& args)
    {
        crossover_t::config_t config{};
        config.max_crossover_tries = args.get<u32>("max_crossover_tries");
        config.max_crossover_iterations = args.get<u32>("max_crossover_iterations");
        config.min_tree_size = args.get<u32>("min_tree_size");
        config.depth_multiplier = args.get<float>("depth_multiplier");
        config.terminal_chance = args.get<float>("terminal_chance");
        config.traverse = args.get<bool>("traverse");
        return config;
    }

    std::tuple<prog_config_t, selection_t*> create_config_from_args(int argc, const char** argv)
    {
        if (argc == 1)
        {
            static auto arr = std::array{argv[0], "default"};
            argv = arr.data();
            argc = arr.size();
        }
        argparse::argument_parser_t parser;
        parser.with_help();
        parser.add_flag("--initial_tree_min").set_dest("initial_tree_min").set_default(2).as_type<i32>().set_help("The minimum number of nodes in the initial trees");
        parser.add_flag("--initial_tree_max").set_dest("initial_tree_max").set_default(6).as_type<i32>().set_help("The maximum number of nodes in the initial trees");
        parser.add_flag("--elites").set_dest("elites").set_default(2).as_type<i32>().set_help("Number of best fitness individuals to keep each generation");
        parser.add_flag("--max_generations", "-g").set_dest("max_generations").set_default(50u).as_type<u32>().set_help("The maximum number of generations to run");
        parser.add_flag("--population_size", "-p").set_dest("population_size").set_default(500u).as_type<u32>().set_help("The size of the population");
        parser.add_flag("--threads", "-t").set_dest("threads").set_default(0u).as_type<u32>().set_help("The number of threads to use");
        parser.add_flag("--crossover_rate", "-c").set_dest("crossover_rate").set_default(0.8f).as_type<float>().set_help("The rate of crossover");
        parser.add_flag("--mutation_rate", "-m").set_dest("mutation_rate").set_default(0.1f).as_type<float>().set_help("The rate of mutation");
        parser.add_flag("--reproduction_rate", "-r").set_dest("reproduction_rate").set_default(0.1f).as_type<float>().set_help("The rate of reproduction");

        const auto mode = parser.add_subparser("mode")->set_help("Select the mode to run the program in.");
        mode->add_parser("default");
        const auto manual = mode->add_parser("manual");

        const auto selection_parser = manual->add_subparser("selection_type")->set_help("Select the type of selection to use.");
        const auto tournament_selection = selection_parser->add_parser("tournament");
        tournament_selection->add_flag("--tournament_size", "-s").set_dest("tournament_size").set_default(5u).as_type<u32>().set_help("The size of the tournament").set_dest(
            "tournament_size");
        selection_parser->add_parser("best");
        selection_parser->add_parser("worst");
        selection_parser->add_parser("roulette", "fitness");

        const auto crossover_parser = manual->add_subparser("crossover_type")->set_help("Select the type of crossover to use.");
        const auto subtree_crossover = crossover_parser->add_parser("subtree_crossover");
        setup_crossover_parser(*subtree_crossover);
        const auto one_point_crossover = crossover_parser->add_parser("one_point_crossover");
        setup_crossover_parser(*one_point_crossover);
        const auto advanced_crossover = crossover_parser->add_parser("advanced_crossover");
        setup_crossover_parser(*advanced_crossover);

        const auto mutation_parser = manual->add_subparser("mutation_type")->set_help("Select the type of mutation to use.");
        const auto single_point_mutation = mutation_parser->add_parser("single_point_mutation");
        single_point_mutation->add_flag("--replacement_min_depth").set_dest("replacement_min_depth").set_default(2u).as_type<u32>().set_help(
            "Minimum depth of the generated replacement tree");
        single_point_mutation->add_flag("--replacement_max_depth").set_dest("replacement_max_depth").set_default(2u).as_type<u32>().set_help(
            "Maximum depth of the generated replacement tree");
        single_point_mutation->add_positional("generator").set_choices("grow", "full").set_default("grow");
        const auto advanced_mutation = mutation_parser->add_parser("advanced_mutation");
        advanced_mutation->add_flag("--replacement_min_depth").set_dest("replacement_min_depth").set_default(2u).as_type<u32>().set_help(
            "Minimum depth of the generated replacement tree");
        advanced_mutation->add_flag("--replacement_max_depth").set_dest("replacement_max_depth").set_default(2u).as_type<u32>().set_help(
            "Maximum depth of the generated replacement tree");
        advanced_mutation->add_flag("--per_node_mutation_chance").set_dest("per_node_mutation_chance").set_default(5.0).as_type<double>().set_help(
            "this value is adjusted inversely to the size of the tree.");
        advanced_mutation->add_flag("--expression_chance").set_dest("expression_chance").set_default(0.25).as_type<double>().set_help(
            "Chance that we will use the expression operator, which generates a new random expression (uses the single point mutation function internally).");
        advanced_mutation->add_flag("--adjust_chance").set_dest("adjust_chance").set_default(0.2).as_type<double>().set_help(
            "Chance that we will use the adjust operator, which adjusts the value of the type. (if it is a function it will mutate it to a different one - geneating or removing trees as needed)");
        advanced_mutation->add_flag("--sub_chance").set_dest("sub_chance").set_default(0.05).as_type<double>().set_help(
            "Chance that we will use the sub-to-sub operator, where subexpression becomes argument to new random function. Other args are generated.");
        advanced_mutation->add_flag("--jump_func").set_dest("jump_func").set_default(0.15).as_type<double>().set_help(
            "Chance that we will use the sub-jump operator, where subexpression becomes this new node. Other arguments discarded.");
        advanced_mutation->add_flag("--copy").set_dest("copy").set_default(0.1).as_type<double>().set_help(
            "Chance that we will use the copy operator, where subtree is copied to the current node. Effectively running subtree crossover on itself");

        advanced_mutation->add_positional("generator").set_choices("grow", "full").set_default("grow");

        const auto args = parser.parse(argc, argv);
        auto config = prog_config_t()
                      .set_initial_min_tree_size(args.get<i32>("initial_tree_min"))
                      .set_initial_max_tree_size(args.get<i32>("initial_tree_max"))
                      .set_elite_count(args.get<i32>("elites"))
                      .set_crossover_chance(args.get<float>("crossover_rate"))
                      .set_mutation_chance(args.get<float>("mutation_rate"))
                      .set_reproduction_chance(args.get<float>("reproduction_rate"))
                      .set_max_generations(args.get<u32>("max_generations"))
                      .set_pop_size(args.get<u32>("population_size"))
                      .set_thread_count(args.get<u32>("threads"));

        thread_local select_tournament_t s_tournament_selection;
        thread_local select_best_t s_best_selection;
        thread_local select_worst_t s_worst_selection;
        thread_local select_fitness_proportionate_t s_roulette_selection;

        thread_local subtree_crossover_t s_subtree_crossover;
        thread_local one_point_crossover_t s_one_point_crossover;
        thread_local advanced_crossover_t s_advanced_crossover;

        thread_local mutation_t s_single_point_mutation;
        thread_local advanced_mutation_t s_advanced_mutation;

        thread_local grow_generator_t grow_generator;
        thread_local full_generator_t full_generator;

        config.set_crossover(s_subtree_crossover);
        config.set_mutation(s_advanced_mutation);

        if (args.get("mode") == "default")
            return {config, &s_tournament_selection};
        if (args.get("mode") == "manual")
        {
            selection_t* selection_op = &s_tournament_selection;
            crossover_t* crossover_op = &s_subtree_crossover;
            mutation_t* mutation_op = &s_single_point_mutation;
            const auto selection_arg = string::toLowerCase(args.get("selection_type"));
            if (selection_arg == "tournament")
            {
                s_tournament_selection = select_tournament_t{args.get<u32>("tournament_size")};
                selection_op = &s_tournament_selection;
            }
            else if (selection_arg == "best")
                selection_op = &s_best_selection;
            else if (selection_arg == "worst")
                selection_op = &s_worst_selection;
            else if (selection_arg == "roulette" || selection_arg == "fitness")
                selection_op = &s_roulette_selection;

            const auto crossover_arg = string::toLowerCase(args.get("crossover_type"));
            if (crossover_arg == "subtree_crossover")
            {
                s_subtree_crossover = subtree_crossover_t{create_crossover_config(args)};
                crossover_op = &s_subtree_crossover;
            }
            else if (crossover_arg == "one_point_crossover")
            {
                s_one_point_crossover = one_point_crossover_t{create_crossover_config(args)};
                crossover_op = &s_one_point_crossover;
            }
            else if (crossover_arg == "advanced_crossover")
            {
                s_advanced_crossover = advanced_crossover_t{create_crossover_config(args)};
                crossover_op = &s_advanced_crossover;
            }

            const auto mutation_arg = string::toLowerCase(args.get("mutation_type"));
            if (mutation_arg == "single_point_mutation")
            {
                mutation_t::config_t mutation_config{
                    *(args.get("generator") == "full"
                          ? static_cast<tree_generator_t*>(&full_generator)
                          : static_cast<tree_generator_t*>(&grow_generator))
                };
                mutation_config.replacement_max_depth = args.get<u32>("replacement_max_depth");
                mutation_config.replacement_min_depth = args.get<u32>("replacement_min_depth");
                s_single_point_mutation = mutation_t{mutation_config};
                mutation_op = &s_single_point_mutation;
            }
            else if (mutation_arg == "advanced_mutation")
            {
                mutation_t::config_t mutation_config{
                    *(args.get("generator") == "full"
                          ? static_cast<tree_generator_t*>(&full_generator)
                          : static_cast<tree_generator_t*>(&grow_generator))
                };
                mutation_config.replacement_max_depth = args.get<u32>("replacement_max_depth");
                mutation_config.replacement_min_depth = args.get<u32>("replacement_min_depth");
                s_advanced_mutation = advanced_mutation_t{mutation_config};
                s_advanced_mutation.set_per_node_mutation_chance(args.get<double>("per_node_mutation_chance"));
                const std::array chances{
                    args.get<double>("expression_chance"), args.get<double>("adjust_chance"), args.get<double>("sub_chance"),
                    args.get<double>("jump_func"), args.get<double>("copy")
                };
                s_advanced_mutation.set_mutation_operator_chances(chances);
                mutation_op = &s_advanced_mutation;
            }

            config.set_crossover(*crossover_op);
            config.set_mutation(*mutation_op);
            return {config, selection_op};
        }
        BLT_ABORT("Broken arguments!");
    }
}