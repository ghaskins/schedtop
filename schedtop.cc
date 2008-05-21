#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <map>
#include <stdexcept>

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
		    
		    lis >> ver;
		    if (ver != 14)
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

class Engine
{
public:
    Engine() : m_period(1), m_filter("*") {}

    void Run() {
	do {
	    Render();

	    sleep(m_period);
	} while (1);
    }

private:
    void Render() {
	Snapshot now;
	ViewList view;

	{
	    Snapshot::iterator curr;
	    
	    // Generate the view data
	    for (curr = now.begin(); curr != now.end(); ++curr)
	    {
		Snapshot::iterator prev(m_base.find(curr->first));
		
		if (prev == m_base.end())
		    throw std::runtime_error("error finding " + curr->first);
		
		ViewData data(curr->first, curr->second,
			      curr->second - prev->second);
		
		view.push_back(data);
	    }
	}

	// Now sort by delta
	view.sort(CompareDelta);

	// render the view data to the screen
	{
	    ViewList::iterator iter;

	    for (iter = view.begin(); iter != view.end(); ++iter)
	    {
		std::cout << iter->m_name << "\t"
			  << iter->m_val << "\t"
			  << iter->m_delta << std::endl;
	    }
	}

	// Update base with new data
	m_base = now;
    }

    unsigned int m_period;
    Snapshot     m_base;
    std::string  m_filter;
};

int main(int argc, void **argv)
{
    Engine e;

    e.Run();

    return 0;
}
