#ifndef WARTHOG_BIDIRECTIONAL_SEARCH_H
#define WARTHOG_BIDIRECTIONAL_SEARCH_H

// bidirectional_search.h
//
// A customisable variant of bidirectional best-first search.
// Users can pass in any heuristic and any (domain-specific) expansion policy.
//
// @author: dharabor
// @created: 2016-02-14
//

#include "constants.h"
#include "graph_expansion_policy.h"
#include "planar_graph.h"
#include "pqueue.h"
#include "search.h"
#include "search_node.h"
#include "timer.h"
#include "zero_heuristic.h"

#include "constants.h"
#include <cstdlib>
#include <stack>
#include <stdint.h>

namespace warthog
{

namespace graph
{
    class planar_graph;
}

class expansion_policy;
class pqueue;
class search_node;

typedef double (* heuristicFn)
(uint32_t nodeid, uint32_t targetid);

template<class H>
class bidirectional_search : public warthog::search
{
    public:
        bidirectional_search(
                warthog::expansion_policy* fexp,
                warthog::expansion_policy* bexp,
                H* heuristic) 
            : fexpander_(fexp), bexpander_(bexp), heuristic_(heuristic)
        {
            verbose_ = false;
            fopen_ = new pqueue(512, true);
            bopen_ = new pqueue(512, true);
            
            dijkstra_ = false;
            if(typeid(*heuristic_) == typeid(warthog::zero_heuristic))
            {
                dijkstra_ = true;
            }
        }

        ~bidirectional_search()
        {
            delete fopen_;
            delete bopen_;
        }

        double 
        get_length(uint32_t startid, uint32_t goalid)
        {
            this->search(startid, goalid);

#ifndef NDEBUG

#endif
            cleanup();
            return best_cost_;
        }
            
		inline bool
		get_verbose() { return verbose_; }

		inline void
		set_verbose(bool verbose) { verbose_ = verbose; } 

        size_t
        mem()
        {
            return sizeof(*this) + 
                fopen_->mem() +
                bopen_->mem() +
                fexpander_->mem();
                bexpander_->mem();
        }

    private:
        warthog::pqueue* fopen_;
        warthog::pqueue* bopen_;
        warthog::expansion_policy* fexpander_;
        warthog::expansion_policy* bexpander_;
        H* heuristic_;
        bool dijkstra_;
        bool forward_;

        // v is the section of the path in the forward
        // direction and w is the section of the path
        // in the backward direction. need parent pointers
        // of both to extract the actual path
        warthog::search_node* v_;
        warthog::search_node* w_;
        double best_cost_;
        warthog::problem_instance instance_;

        // modify this function to balance the search
        // by default the search expands one node in
        // each direction then switches to the other direction
        bool
        expand_forward()
        {
            forward_ = !forward_;
            return forward_;
        }

        void 
        search(uint32_t startid, uint32_t goalid)
        {
            warthog::timer mytimer;
            mytimer.start();

            // init
            this->reset_metrics();
            best_cost_ = warthog::INF;
            v_ = w_ = 0;

            #ifndef NDEBUG
            if(verbose_)
            {
                std::cerr << "bidirectional_search: startid="
                    << startid<<" goalid=" <<goalid
                    << std::endl;
            }
            #endif

            instance_.set_goal(goalid);
            instance_.set_start(startid);
            instance_.set_searchid(++warthog::search::searchid_);

            warthog::search_node* start = fexpander_->generate(startid);
            start->init(instance_.get_searchid(), 0, 0, 
                    heuristic_->h(startid, goalid));
            fopen_->push(start);

            warthog::search_node* goal = bexpander_->generate(goalid);
            goal->init(instance_.get_searchid(), 0, 0, 
                    heuristic_->h(startid, goalid));
            bopen_->push(goal);

            // interleave search 
            forward_ = false;
            while(fopen_->size() > 0 && bopen_->size() > 0)
            {
                // the way we calculate the lower-bound on solution cost 
                // differs depending on the search algorithm at hand. 
                uint32_t best_bound_ = dijkstra_ ? 
                    (fopen_->peek()->get_g() + bopen_->peek()->get_g()) : 
                    (std::min( // alternative termination for bi-A* and bi-CH
                        fopen_->peek()->get_f(), 
                        bopen_->peek()->get_f()));

                // terminate if we cannot improve the best solution found so far
                if(best_bound_ > best_cost_)
                {
#ifndef NDEBUG
                    if(verbose_)
                    {
                        std::cerr << "provably-best solution found; cost=" << 
                            best_cost_ << std::endl;
                    }
#endif
                    break;
                }

                // ok, we still have hope. let's keep expanding. 
                // NB: if we can't improve in any one direction we expand a 
                // node in the other direction instead (regardless of 
                // interleave policy)
                if(this->expand_forward())
                {
                    if(fopen_->peek()->get_f() < best_cost_)
                    {
                        warthog::search_node* current = fopen_->pop();
                        expand(current, fopen_, fexpander_, bexpander_, goalid);
                    }
                    else
                    {
                        warthog::search_node* current = bopen_->pop();
                        expand(current, bopen_, bexpander_, fexpander_, startid);
                    }
                }
                else 
                {
                    if(bopen_->peek()->get_f() < best_cost_)
                    {
                        warthog::search_node* current = bopen_->pop();
                        expand(current, bopen_, bexpander_, fexpander_, startid);
                    }
                    else
                    {
                        warthog::search_node* current = fopen_->pop();
                        expand(current, fopen_, fexpander_, bexpander_, goalid);
                    }
                }


            }

            assert(best_cost_ != warthog::INF ||
                    (v_->get_g() != warthog::INF && w_->get_g() != warthog::INF));

			mytimer.stop();
			search_time_ = mytimer.elapsed_time_micro();
        }

        void
        expand( warthog::search_node* current,
                warthog::pqueue* open,
                warthog::expansion_policy* expander,
                warthog::expansion_policy* reverse_expander, 
                uint32_t tmp_goalid)
        {
            // goal test
            if(current->get_id() == tmp_goalid) 
            {
                best_cost_ = current->get_g();
                v_ = current;
                w_ = 0;
                return;
            }

            // goal not found yet; expand as normal
            current->set_expanded(true);
            expander->expand(current, &instance_);
            nodes_expanded_++;

            #ifndef NDEBUG
            if(verbose_)
            {
                int32_t x, y;
                expander->get_xy(current, x, y);
                std::cerr << this->nodes_expanded_ << ". "
                    "expanding " << (instance_.get_goal() == tmp_goalid ? "(f)" : "(b)")
                    << " ("<<x<<", "<<y<<")...";
                current->print(std::cerr);
                std::cerr << std::endl;
            }
            #endif
            
            // generate all neighbours
            warthog::search_node* n = 0;
            double cost_to_n = warthog::INF;
            for(expander->first(n, cost_to_n); n != 0; expander->next(n, cost_to_n))
            {
                nodes_touched_++;
                if(n->get_expanded())
                {
                    // skip neighbours already expanded
                    #ifndef NDEBUG
                    if(verbose_)
                    {
                        int32_t x, y;
                        expander->get_xy(n, x, y);
                        std::cerr << "  closed; (edgecost=" << cost_to_n << ") "
                            << "("<<x<<", "<<y<<")...";
                        n->print(std::cerr);
                        std::cerr << std::endl;

                    }
                    #endif
                    continue;
                }

                // relax (or generate) each neighbour
                double gval = current->get_g() + cost_to_n;
                if(open->contains(n))
                {
                    // update a node from the fringe
                    if(gval < n->get_g())
                    {
                        n->relax(gval, current);
                        open->decrease_key(n);
                        #ifndef NDEBUG
                        if(verbose_)
                        {
                            int32_t x, y;
                            expander->get_xy(n, x, y);
                            std::cerr << "  open; updating "
                                << "(edgecost="<< cost_to_n<<") "
                                << "("<<x<<", "<<y<<")...";
                            n->print(std::cerr);
                            std::cerr << std::endl;
                        }
                        #endif
                    }
                    else
                    {
                        #ifndef NDEBUG
                        if(verbose_)
                        {
                            int32_t x, y;
                            expander->get_xy(n, x, y);
                            std::cerr << "  open; not updating "
                                << "(edgecost=" << cost_to_n<< ") "
                                << "("<<x<<", "<<y<<")...";
                            n->print(std::cerr);
                            std::cerr << std::endl;
                        }
                        #endif
                    }
                }
                else
                {
                    // add a new node to the fringe
                    n->init(current->get_searchid(), 
                            current, 
                            gval,
                            gval + heuristic_->h(n->get_id(), tmp_goalid));
                    open->push(n);
                    #ifndef NDEBUG
                    if(verbose_)
                    {
                        int32_t x, y;
                        expander->get_xy(n, x, y);
                        std::cerr << "  generating "
                            << "(edgecost=" << cost_to_n<<") " 
                            << "("<<x<<", "<<y<<")...";
                        n->print(std::cerr);
                        std::cerr << std::endl;
                    }
                    #endif
                    nodes_generated_++;
                }

                // update the best solution if possible
                warthog::search_node* reverse_n = 
                    reverse_expander->generate(n->get_id());
                if(reverse_n->get_searchid() == n->get_searchid())
//                        && reverse_n->get_expanded())
                {
                    if((current->get_g() + cost_to_n + reverse_n->get_g()) < best_cost_)
                    {
                        v_ = current;
                        w_ = reverse_n;
                        best_cost_ = current->get_g() + cost_to_n + reverse_n->get_g();

                        #ifndef NDEBUG
                        if(verbose_)
                        {
                            int32_t x, y;
                            expander->get_xy(current, x, y);
                            std::cerr <<"new best solution!  cost=" << best_cost_<<std::endl;
                        }
                        #endif
                    }
                }
            }

            #ifndef NDEBUG
            if(verbose_)
            {
                int32_t x, y;
                expander->get_xy(current, x, y);
                std::cerr <<"closing ("<<x<<", "<<y<<")...";
                current->print(std::cerr);
                std::cerr << std::endl;
            }
            #endif
        }

        void
        cleanup()
        {
            fopen_->clear();
            bopen_->clear();
            fexpander_->clear();
            bexpander_->clear();
        }

};

}

#endif

