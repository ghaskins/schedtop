// schedtop
//
// Copyright (c) 2008, Novell
//
// Author: Gregory Haskins <ghaskins@novell.com>
//
//   Special thanks to David Bahi <dbahi@novell.com> for helping to get the
//   reg-ex code working.
//
// schedtop is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License v2 as published
// by the Free Software Foundation.
// 
// schedtop is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with schedtop; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

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
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

std::string FormIndex(const std::string &base, int index)
{
    std::ostringstream os;

    os << base << index;

    return os.str();
}

const char *IdleType[] = {
    "idle/",
    "not-idle/",
    "newly-idle/"
};

typedef unsigned long long StatVal;
typedef std::map<std::string, StatVal> StatMap;

class Importer
{
public:
    Importer(StatMap &smap,
	     std::istream &is,
	     const std::string &basename) :
	m_smap(smap), m_is(is), m_basename(basename) {}

    void operator+=(const std::string &name)
	{
	    StatVal val;
	    
	    m_is >> val;
	    
	    StatMap::value_type item(m_basename + name, val);
	    
	    m_smap.insert(item);
	}
    
private:
    StatMap           &m_smap;
    std::istream      &m_is;
    const std::string  m_basename;
};

class GlobalSnapshot
{
public:
    enum State {
	state_version,
	state_timestamp,
	state_cpu,
	state_domain
    };
    
    GlobalSnapshot(StatMap &smap) : m_smap(smap), m_cpu(0), m_domain(0)
	{
	    std::ifstream is("/proc/schedstat");
	    State state(state_version);
	    
	    if (!is.is_open())
		throw std::runtime_error("could not open /proc/schedstats (is CONFIG_SCHEDSTATS enabled?)");
	    
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
			    throw std::runtime_error("error parsing version");
			
			lis >> m_version;
			if (!IsSupportedVersion())
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
			if (type == FormIndex("domain", m_domain)) {
			    ImportDomain(lis);
			    m_domain++;
			    break;
			} else if (type == FormIndex("cpu", m_cpu+1)) {
			    state = state_cpu;
			    m_cpu++;
			} else
			    throw std::runtime_error("error parsing domain");
			
			// fall through
		    case state_cpu:
			if (type != FormIndex("cpu", m_cpu))
			    throw std::runtime_error("error parsing cpu");
			
			ImportCpu(lis);
			state = state_domain;
			m_domain = 0;
			break;
		}
	    }
	}
    
private:
    bool IsSupportedVersion()
	{
	    if ((m_version < 14) || (m_version > 15))
		return false;
	    return true;
	}
    
    bool VersionSupportsSchedYield()
	{
	    if (m_version >= 15)
		return false;
	    return true;
	}
    
    void ImportUnknown(std::istream &is, const std::string &basename)
	{
	    int unknown(0);
	    
	    while (is) {
		std::string s;
		
		is >> s;
		if (s.empty())
		    break;
		
		std::istringstream sis(s);
		Importer importer(m_smap, sis, basename);

		importer += FormIndex("unknown", unknown);
		unknown++;
	    }
	}
    
    void ImportDomain(std::istream &is)
	{
	    std::string basename =
		"/" + FormIndex("cpu", m_cpu)
		+ "/" + FormIndex("domain", m_domain) + "/";
	    std::string tmp;
	    
	    // skip over the cpumask_t
	    is >> tmp;
	    
	    for (int itype(0);
		 itype < sizeof(IdleType)/sizeof(IdleType[0]);
		 ++itype)
	    {
		Importer importer(m_smap, is, basename + IdleType[itype]);

		importer += "lb_count";
		importer += "lb_balanced";
		importer += "lb_failed";
		importer += "lb_imbalance";
		importer += "lb_gained";
		importer += "lb_hot_gained";
		importer += "lb_nobusyq";
		importer += "lb_nobusyg";
	    }

	    Importer importer(m_smap, is, basename);

	    importer += "alb_count";
	    importer += "alb_failed";
	    importer += "alb_pushed";
	    importer += "sbe_count";
	    importer += "sbe_balanced";
	    importer += "sbe_pushed";
	    importer += "sbf_count";
	    importer += "sbf_balanced";
	    importer += "sbf_pushed";
	    importer += "ttwu_wake_remote";
	    importer += "ttwu_move_affine";
	    importer += "ttwu_move_balance";
	    
	    ImportUnknown(is, basename);
	}
    
    void ImportCpu(std::istream &is)
	{
	    std::string basename("/" + FormIndex("cpu", m_cpu) + "/rq/");
	    Importer importer(m_smap, is, basename);

	    if (VersionSupportsSchedYield()){
		    importer += "yld_both_empty";
		    importer += "yld_act_empty";
		    importer += "yld_exp_empty";
	    }
	    importer += "yld_count";
	    importer += "sched_switch";
	    importer += "sched_count";
	    importer += "sched_goidle";
	    importer += "ttwu_count";
	    importer += "ttwu_local";
	    importer += "rq_sched_info.cpu_time";
	    importer += "rq_sched_info.run_delay";
	    importer += "rq_sched_info.pcount";
	    
	    ImportUnknown(is, basename);
	}

    StatMap &m_smap;
    int m_version;
    int m_cpu;
    int m_domain;
};

namespace fs = boost::filesystem;

void ProcSnapshot(StatMap &smap)
{
    if (!fs::exists("/proc"))
	return;
    
    fs::directory_iterator end;
    for (fs::directory_iterator iter("/proc"); iter != end; ++iter) {
	
	std::string path(iter->path().string() + "/schedstat");
	if (fs::exists(path)) {
	    std::ifstream is(path.c_str());
	    
	    if (!is.is_open())
		throw std::runtime_error("could not open " + path);
	    
	    Importer importer(smap, is, iter->path().string() + "/");
	    
	    importer += "sched_info.cpu_time";
	    importer += "sched_info.run_delay";
	    importer += "sched_info.pcount";
	}

	path = iter->path().string() + "/sched";
	if (fs::exists(path)) {
	    std::ifstream is(path.c_str());
	    
	    if (!is.is_open())
		throw std::runtime_error("could not open " + path);

	    while(is) {
		std::string line;
		std::string type;
		
		std::getline(is, line);
		if (line.empty())
		    break;
		
		std::istringstream lis(line);
		
		lis >> type;

		boost::regex e;
		boost::cmatch what;
		
		e.assign("nr_", boost::regex_constants::basic);
            
		if (boost::regex_search(type.c_str(), what, e)) {
		    std::string tmp;

		    // discard the uneeded ":" 
		    lis >> tmp;

		    Importer importer(smap, lis,
				      iter->path().string() + "/");

		    importer += type;
		}
	    }
	}
	
    }
}

class Snapshot : public StatMap
{
public:
    Snapshot()
	{
	    GlobalSnapshot(*this);
	    ProcSnapshot(*this);
	}
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

		    e.assign(m_ifilter, boost::regex_constants::perl);
            
		    // check for "include" filter matches
		    if (!boost::regex_search(curr->first.c_str(), what, e))
			continue;
		    else {
			// .. and then for "exclude" matches
			e.assign(m_xfilter, boost::regex_constants::perl);
			if (boost::regex_search(curr->first.c_str(), what, e))
			    continue;
		    }

		    Snapshot::iterator prev(m_base.find(curr->first));
		    StatVal delta(0);

		    if (prev != m_base.end())
			delta = curr->second - prev->second;
		    
		    ViewData data(curr->first, curr->second, delta);
		    
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
		
		// Draw separator
		move(1,0);
		for (int j(0); j<col; j++)
		    addch('-');

		attroff(A_BOLD);
		
		// Draw data
		for (iter = view.begin();
		     iter != view.end() && i < row;
		     ++iter, ++i)
		{
		    
		    mvprintw(i,0, "%s", iter->m_name.c_str());
		    mvprintw(i,40, "%llu", iter->m_val);
		    mvprintw(i,60, "%llu", iter->m_delta);
		}
	    }
	    
	    refresh();
	    
	    // Update base with new data
	    m_base = now;
	}
    
    Snapshot     m_base;
    unsigned int m_period;
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

    try {
	Engine e(period, ifilter, xfilter, sortby);
	
	e.Run();
    } catch (std::exception &e) {
	std::cerr << "Exception: " << e.what() << std::endl;
	return -1;
    }

    return 0;
}
