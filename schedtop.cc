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

typedef std::pair<std::string, unsigned long long> Stat;

class Snapshot : public std::list<Stat>
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
	unsigned long long val;

	is >> val;

	Stat stat(name, val);
	this->push_back(stat);
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
		    stype = "idle";
		    break;
		case CPU_NOT_IDLE:
		    stype = "not-idle";
		    break;
		case CPU_NEWLY_IDLE:
		    stype = "newly-idle";
		    break;
	    }

	    Import(is, basename + stype + "/lb_count");
	    Import(is, basename + stype + "/lb_balanced");
	    Import(is, basename + stype + "/lb_failed");
	    Import(is, basename + stype + "/lb_imbalance");
	    Import(is, basename + stype + "/lb_gained");
	    Import(is, basename + stype + "/lb_hot_gained");
	    Import(is, basename + stype + "/lb_nobusyq");
	    Import(is, basename + stype + "/lb_nobusyg");
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

int main(int argc, void **argv)
{
    Snapshot s;
    Snapshot::iterator iter;

    for (iter = s.begin(); iter != s.end(); ++iter)
    {
	std::cout << iter->first << ": " << iter->second << std::endl;
    }

    return 0;
}
