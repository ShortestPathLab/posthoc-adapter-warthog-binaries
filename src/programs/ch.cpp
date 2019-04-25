#include "cfg.h"
#include "bch_expansion_policy.h"
#include "dimacs_parser.h"
#include "fixed_graph_contraction.h"
#include "graph.h"
#include "lazy_graph_contraction.h"
#include "xy_graph.h"

#include <iostream>
#include <string>

int verbose=false;
int verify=false;
warthog::util::cfg cfg;

void
help()
{
    std::cerr << 
        "create a contraction hierarchy from " <<
        "a given (currently, DIMACS-format only) input graph\n";
	std::cerr << "valid parameters:\n"
	<< "\t--order [ fixed | lazy ]\n"
    << "\t--partial [1-100] (optional; percentage of nodes to contract)\n"
    << "\t--input [gr file] [co file] (IN THIS ORDER!!)\n"
	<< "\t--verbose (optional)\n"
	<< "\t--verify (verify lazy node priorities before contraction)\n";
}

void 
contract_graph()
{
    std::string grfile = cfg.get_param_value("input");
    std::string cofile = cfg.get_param_value("input");

    if(grfile == "" || cofile == "")
    {
        std::cerr << "err; insufficient input parameters."
                  << " required, in order:\n"
                  << " --input [gr file] [co file]\n";
        return;
    }

    // contract a graph and create a hierarchy
    warthog::graph::xy_graph g;

    std::string partial = cfg.get_param_value("partial");
    int32_t pct_nodes_to_contract = 100;
    if(partial != "")
    {
        pct_nodes_to_contract = atoi(partial.c_str());
    }

    // use an existing node order for contraction operations
    std::string order_type = cfg.get_param_value("order");
    if(order_type == "fixed")
    {
        std::string orderfile = cfg.get_param_value("order");
        if(orderfile == "")
        {
            std::cerr 
                  << "err; fixed order requires input file."
                  << " syntax: --order fixed [node order file]\n";
            return;
        }

        if(!g.load_from_dimacs(grfile.c_str(), cofile.c_str(), false, true))
        {
            std::cerr 
                << "err; could not load gr or co input files (one or both)\n";
            return;
        }

        std::vector<uint32_t> order;
        if(!warthog::ch::load_node_order(orderfile.c_str(), order))
        {
            std::cerr << "err; could not load node order input file\n";
            return;
        }
        warthog::ch::fixed_graph_contraction contractor(&g, &order);
        contractor.set_verbose(verbose);
        contractor.set_partial_contraction_percentage((uint32_t)pct_nodes_to_contract);
        contractor.contract();

        // save the result
        if(pct_nodes_to_contract < 100)
        {
            grfile.append(".pch");
        }
        else
        {
            grfile.append(".ch");
        }
        std::cerr << "saving contracted graph to file " << grfile << std::endl;
        std::fstream ch_out(grfile.c_str(), std::ios_base::out | std::ios_base::trunc);
        if(!ch_out.good())
        {
            std::cerr << "\nerror exporting ch to file " << grfile << std::endl;
        }
        g.print_dimacs_gr(ch_out, 0, (uint32_t)g.get_num_nodes());
        ch_out.close();
    }
    else if(order_type == "lazy")
    {
        // create a new contraction hierarchy with dynamic node ordering
        if(!g.load_from_dimacs(grfile.c_str(), cofile.c_str(), false, true))
        {
            std::cerr 
                << "err; could not load gr or co input files (one or both)\n";
            return;
        }

        warthog::ch::lazy_graph_contraction contractor;
        contractor.set_verbose(verbose);
        warthog::ch::ch_data* chd = 
            contractor.contract(&g, verify, (uint32_t)pct_nodes_to_contract);

        std::cerr << "saving...\n";

        // save the result
        if(pct_nodes_to_contract < 100)
        {
            grfile.append(".chd_p");
        }
        else
        {
            grfile.append(".chd");
        }
        std::cerr << "saving contracted graph to file " << grfile << std::endl;
        if(!warthog::ch::save(grfile.c_str(), chd))
        {
            std::cerr << "\nerror exporting ch to file " << grfile << std::endl;
        }
    }
    else
    {
        std::cerr << "unknown parameter for --order\n";
        return;
    }

    std::cerr << "all done!\n";
}

int main(int argc, char** argv)
{

	// parse arguments
	warthog::util::param valid_args[] = 
	{
		{"verbose", no_argument, &verbose, 1},
		{"verify", no_argument, &verify, 1},
		{"input",  required_argument, 0, 2},
		{"order",  required_argument, 0, 3},
		{"partial",  required_argument, 0, 4}
	};
	cfg.parse_args(argc, argv, "-hvd:o:", valid_args);

    if(argc == 1)
    {
		help();
        exit(0);
    }

    contract_graph();
}
