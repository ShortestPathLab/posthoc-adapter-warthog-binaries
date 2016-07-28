#include "apex_distance_filter.h"
#include "cfg.h"
#include "ch_expansion_policy.h"
#include "dimacs_parser.h"
#include "down_distance_filter.h"
#include "fixed_graph_contraction.h"
#include "graph.h"
#include "lazy_graph_contraction.h"
#include "planar_graph.h"

#include <iostream>
#include <string>

int verbose=false;
int down_dist_first_id;
int down_dist_last_id;
warthog::util::cfg cfg;

void
help()
{
    std::cerr << 
        "create arc labels for " <<
        "a given (currently, DIMACS-format only) input graph\n";
	std::cerr << "valid parameters:\n"
    << "\t--dimacs [gr file] [co file] (IN THIS ORDER!!)\n"
	<< "\t--order [order-of-contraction file]\n"
    << "\t--arclabels [downdist | arcflags | bbox]\n"
	<< "\t--verbose (optional)\n";
}

void 
compute_down_distance()
{
    std::string grfile = cfg.get_param_value("dimacs");
    std::string cofile = cfg.get_param_value("dimacs");
    std::cerr << "param values " << std::endl;
    std::string orderfile = cfg.get_param_value("order");
    cfg.print_values("dimacs", std::cerr);

    std::cerr << "grfile: "<< grfile << " cofile " << cofile << std::endl;

    // load up (or create) the contraction hierarchy
    warthog::graph::planar_graph g;
    std::vector<uint32_t> order;

    if(orderfile.compare("") != 0)
    {
        g.load_dimacs(grfile.c_str(), cofile.c_str(), false, true);
        warthog::ch::load_node_order(orderfile.c_str(), order, true);

        // compute down_distance
        grfile.append(".ddist.arclabel");
        uint32_t firstid = 0;
        uint32_t lastid = g.get_num_nodes();
        if(cfg.get_num_values("arclabels") == 2)
        {
            std::string first = cfg.get_param_value("arclabels");
            std::string last = cfg.get_param_value("arclabels");

            if(strtol(first.c_str(), 0, 10) != 0)
            {
                firstid = strtol(first.c_str(), 0, 10);
            }
            if(strtol(last.c_str(), 0, 10) != 0)
            {
                lastid = strtol(last.c_str(), 0, 10);
            }
            grfile.append(".");
            grfile.append(first);
            grfile.append(".");
            grfile.append(std::to_string(lastid-1));
        }
        warthog::down_distance_filter ddfilter(&g, &order);
        ddfilter.compute_down_distance(firstid, lastid);
        
        // save the result
        std::cerr << "saving contracted graph to file " << grfile << std::endl;
        std::fstream out(grfile.c_str(), std::ios_base::out | std::ios_base::trunc);
        if(!out.good())
        {
            std::cerr << "\nerror exporting ch to file " << grfile << std::endl;
        }
        ddfilter.print(out);
        out.close();
    }
    else
    {
        std::cerr << "required: node order file. aborting.\n";
        return;
    }
    std::cerr << "all done!\n";
}

void 
compute_apex_distance()
{
    std::string grfile = cfg.get_param_value("dimacs");
    std::string cofile = cfg.get_param_value("dimacs");
    std::cerr << "param values " << std::endl;
    std::string orderfile = cfg.get_param_value("order");
    cfg.print_values("dimacs", std::cerr);

    std::cerr << "grfile: "<< grfile << " cofile " << cofile << std::endl;

    // load up (or create) the contraction hierarchy
    warthog::graph::planar_graph g;
    std::vector<uint32_t> order;

    if(orderfile.compare("") != 0)
    {
        g.load_dimacs(grfile.c_str(), cofile.c_str(), false, true);
        warthog::ch::load_node_order(orderfile.c_str(), order, true);

        // compute down_distance
        grfile.append(".apex.arclabel");
        uint32_t firstid = 0;
        uint32_t lastid = g.get_num_nodes();
        if(cfg.get_num_values("arclabels") == 2)
        {
            std::string first = cfg.get_param_value("arclabels");
            std::string last = cfg.get_param_value("arclabels");

            if(strtol(first.c_str(), 0, 10) != 0)
            {
                firstid = strtol(first.c_str(), 0, 10);
            }
            if(strtol(last.c_str(), 0, 10) != 0)
            {
                lastid = strtol(last.c_str(), 0, 10);
            }
            grfile.append(".");
            grfile.append(first);
            grfile.append(".");
            grfile.append(std::to_string(lastid-1));
        }
        warthog::apex_distance_filter apexfilter(&g, &order);
        apexfilter.compute_apex_distance(firstid, lastid);
        
        // save the result
        std::cerr << "saving contracted graph to file " << grfile << std::endl;
        std::fstream out(grfile.c_str(), std::ios_base::out | std::ios_base::trunc);
        if(!out.good())
        {
            std::cerr << "\nerror exporting ch to file " << grfile << std::endl;
        }
        apexfilter.print(out);
        out.close();
    }
    else
    {
        std::cerr << "required: node order file. aborting.\n";
        return;
    }
    std::cerr << "all done!\n";
}

int main(int argc, char** argv)
{

	// parse arguments
    int print_help=false;
	warthog::util::param valid_args[] = 
	{
		{"help", no_argument, &print_help, 1},
		{"verbose", no_argument, &verbose, 1},
		{"dimacs",  required_argument, 0, 2},
		{"order",  required_argument, 0, 1},
        {"arclabels", required_argument, 0, 1}
	};
	cfg.parse_args(argc, argv, "-hvd:o:a:", valid_args);

    if(argc == 1 || print_help)
    {
		help();
        exit(0);
    }

    if(cfg.get_num_values("dimacs") != 2)
    {
        std::cerr << "insufficient values for param --dimacs (need gr and co files)\n";
        exit(0);
    }

    std::string arclabel = cfg.get_param_value("arclabels");
    if(arclabel.compare("downdist") == 0)
    {
        compute_down_distance();
    }
    if(arclabel.compare("apexdist") == 0)
    {
        compute_apex_distance();
    }
    else
    {
        std::cerr << "invalid option for parameter arclabel: " 
            << arclabel << std::endl;
    }
    return 0;
}
