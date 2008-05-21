// schedtop
//
// Copyright (c) 2008, Novell
//
// Author: Gregory Haskins <ghaskins@novell.com>
//
// schedtop is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License v2 
// as published by the Free Software Foundation.
// 
// schedtop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with schedtop; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


// compile with:
//    g++ schedtop.cc -g -lncurses -lboost_program_options -lboost_regex -o schedtop

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <map>
#include <stdexcept>
#include <curses.h>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/function.hpp>

std::string formindex(const std::string &base, int index)
{
    std::ostringstream os;

    os << base << index;

    return os.str();
}

enum State {
    state_version,
    state_timestamp,
    state_cpu,
    state_domain
};

enum cpu_idle_type {
    CPU_IDLE,
    CPU_NOT_IDLE,
    CPU_NEWLY_IDLE,
    CPU_MAX_IDLE_TYPES
};

typedef unsigned long long StatVal;

class Snapshot : public std::map<std::string, StatVal>
{
public:
    Snapshot() : m_cpu(0), m_domain(0) {
	std::ifstream is("/proc/schedstat");
	State state(state_version);
	
	while(is) {
	    std::string line;
	    std::string type;
	    
	    std::getline(is, line);
	    if (line.empty())
		break;
	    
	    std::istringstream lis(line);
	    
	    lis >> type;
	    
	    switch (state) {
		case state_version: {
		    unsigned int ver;
		    if (type != "version")
			throw std::runtime_error("could not open statistics");
		    
		    lis >> m_version;
		    if (m_version != 14)
			throw std::runtime_error("unsupported version");
		    
		    state = state_timestamp;
		    break;
		}
		case state_timestamp:
		    if (type != "timestamp")
			throw std::runtime_error("error parsing timestamp");
		    
		    state = state_cpu;
		    break;
		case state_domain:
		    if (type == formindex("domain", m_domain)) {
			ImportDomain(lis);
			m_domain++;
			break;
		    } else if (type == formindex("cpu", m_cpu+1)) {
			state = state_cpu;
			m_cpu++;
		    } else
			throw std::runtime_error("error parsing domain");
		    
		    // fall through
		case state_cpu:
		    if (type != formindex("cpu", m_cpu))
			throw std::runtime_error("error parsing cpu");

		    ImportCpu(lis);
		    state = state_domain;
		    m_domain = 0;
		    break;
	    }
	}
    }

private:
    void Import(std::istream &is, const std::string &name) {
	StatVal val;

	is >> val;

	Snapshot::value_type item(name, val);

	this->insert(item);
    }

    void ImportDomain(std::istream &is) {
	std::string basename =
	    "/" + formindex("cpu", m_cpu)
	    + "/" + formindex("domain", m_domain) + "/";
	std::string tmp;

	// skip over the cpumask_t
	is >> tmp;

	for (int itype(CPU_IDLE);
	     itype < CPU_MAX_IDLE_TYPES;
	     ++itype)
	{
	    std::string stype;

	    switch(itype)
	    {
		case CPU_IDLE:
		    stype = "idle/";
		    break;
		case CPU_NOT_IDLE:
		    stype = "not-idle/";
		    break;
		case CPU_NEWLY_IDLE:
		    stype = "newly-idle/";
		    break;
	    }

	    Import(is, basename + stype + "lb_count");
	    Import(is, basename + stype + "lb_balanced");
	    Import(is, basename + stype + "lb_failed");
	    Import(is, basename + stype + "lb_imbalance");
	    Import(is, basename + stype + "lb_gained");
	    Import(is, basename + stype + "lb_hot_gained");
	    Import(is, basename + stype + "lb_nobusyq");
	    Import(is, basename + stype + "lb_nobusyg");
	}

	Import(is, basename + "alb_count");
	Import(is, basename + "alb_failed");
	Import(is, basename + "alb_pushed");
	Import(is, basename + "sbe_count");
	Import(is, basename + "sbe_balanced");
	Import(is, basename + "sbe_pushed");
	Import(is, basename + "sbf_count");
	Import(is, basename + "sbf_balanced");
	Import(is, basename + "sbf_pushed");
	Import(is, basename + "ttwu_wake_remote");
	Import(is, basename + "ttwu_move_affine");
	Import(is, basename + "ttwu_move_balance");
    }

    void ImportCpu(std::istream &is) {
	std::string basename("/" + formindex("cpu", m_cpu) + "/rq/");

	Import(is, basename + "yld_both_empty");
	Import(is, basename + "yld_act_empty");
	Import(is, basename + "yld_exp_empty");
	Import(is, basename + "yld_count");
	Import(is, basename + "sched_switch");
	Import(is, basename + "sched_count");
	Import(is, basename + "sched_goidle");
	Import(is, basename + "ttwu_count");
	Import(is, basename + "ttwu_local");
	Import(is, basename + "rq_sched_info.cpu_time");
	Import(is, basename + "rq_sched_info.run_delay");
	Import(is, basename + "rq_sched_info.pcount");
    }

    int m_version;
    int m_cpu;
    int m_domain;
};

struct ViewData
{
    ViewData(const std::string &name, StatVal val, StatVal delta) :
	m_name(name), m_val(val), m_delta(delta) {}

    std::string m_name;
    StatVal     m_val;
    StatVal     m_delta;
};

typedef std::list<ViewData> ViewList;

bool CompareDelta(const ViewData &lhs, const ViewData &rhs)
{
    return lhs.m_delta > rhs.m_delta;
}

bool CompareValue(const ViewData &lhs, const ViewData &rhs)
{
    return lhs.m_val > rhs.m_val;
}

bool CompareName(const ViewData &lhs, const ViewData &rhs)
{
    return lhs.m_name < rhs.m_name;
}

typedef boost::function<bool (const ViewData &lhs, const ViewData &rhs)> SortBy;


class Engine
{
public:
    Engine(unsigned int period,
	   const std::string &ifilter,
	   const std::string &xfilter,
	   char sortby)
	: m_period(period), m_ifilter(ifilter), m_xfilter(xfilter)
	{
	    initscr();

	    switch (sortby) {
		case 'n':
		    m_sortby = CompareName;
		    break;
		case 'v':
		    m_sortby = CompareValue;
		    break;
		case 'd':
		    m_sortby = CompareDelta;
		    break;
		default:
		    throw std::runtime_error("unknown sort option");
	    }
	}

    ~Engine()
	{
	    endwin();
	}

    void Run()
	{
	    do {
		Render();
		
		sleep(m_period);
	    } while (1);
	}

private:
    void Render()
	{
	    Snapshot now;
	    ViewList view;
	    
	    // Generate the view data
	    {
		Snapshot::iterator curr;
		
		for (curr = now.begin(); curr != now.end(); ++curr)
		{
		    boost::regex e;
		    boost::cmatch what;

		    e.assign(m_ifilter, boost::regex_constants::basic);
            
		    // check for "include" filter matches
		    if (!boost::regex_search(curr->first.c_str(), what, e))
			continue;
		    else {
			// .. and then for "exclude" matches
			e.assign(m_xfilter, boost::regex_constants::basic);
			if (boost::regex_search(curr->first.c_str(), what, e))
			    continue;
		    }

		    Snapshot::iterator prev(m_base.find(curr->first));
		    
		    if (prev == m_base.end())
			throw std::runtime_error("error finding "
						 + curr->first);
		    
		    ViewData data(curr->first, curr->second,
				  curr->second - prev->second);
		    
		    view.push_back(data);
		}
	    }
	    
	    // Sort the data according to the configuration
	    view.sort(m_sortby);
	    
	    // render the view data to the screen
	    {
		int row,col;
		int i(2);
		ViewList::iterator iter;
		
		getmaxyx(stdscr,row,col);
		clear();
		
		// Draw header
		attron(A_BOLD);
		mvprintw(0,0,  "Name");
		mvprintw(0,40, "Value");
		mvprintw(0,60, "Delta");
		move(1,0);
		{
		    for (int j(0); j<col; j++)
			addch('-');
		}
		attroff(A_BOLD);
		
		// Draw data
		for (iter = view.begin();
		     iter != view.end() && i < row;
		     ++iter, ++i)
		{
		    
		    move(i, 0);
		    printw("%s", iter->m_name.c_str());
		    move(i, 40);
		    printw("%llu", iter->m_val);
		    move(i, 60);
		    printw("%llu", iter->m_delta);
		}
	    }
	    
	    refresh();
	    
	    // Update base with new data
	    m_base = now;
	}
    
    unsigned int m_period;
    Snapshot     m_base;
    std::string  m_ifilter;
    std::string  m_xfilter;
    SortBy       m_sortby;
};

namespace po = boost::program_options;

#define IFILTER_DEFAULT ".*"
#define XFILTER_DEFAULT "^$"

int main(int argc, char **argv)
{
    unsigned int period(1);
    std::string ifilter(IFILTER_DEFAULT);
    std::string xfilter(XFILTER_DEFAULT);
    char sortby('d');

    po::options_description desc("Allowed options");
    desc.add_options()
	("help,h", "produces help message")
	("period,p", po::value<unsigned int>(&period),
	 "refresh period (default=1s)")
	("include,i", po::value<std::string>(&ifilter),
	 "reg-ex inclusive filter (default=\"" IFILTER_DEFAULT "\")")
	("exclude,x", po::value<std::string>(&xfilter),
	 "reg-ex exclusive filter (default=\"" XFILTER_DEFAULT "\")")
	("sort,s", po::value<char>(&sortby),
	 "sort-by: n=name, v=value, d=delta (default='d')")
	;
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
	std::cerr << desc << std::endl;
	return -1;
    }

    Engine e(period, ifilter, xfilter, sortby);
    
    e.Run();

    return 0;
}
