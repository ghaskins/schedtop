#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
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

int main(int argc, void **argv)
{
    std::ifstream is("/proc/schedstat");
    int cpu(0);
    int domain(0);
    State state;

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
		if (type == formindex("domain", domain)) {
		    std::cout << "d:" << type << std::endl;
		    domain++;
		    break;
		} else if ( type == formindex("cpu", cpu+1)) {
		    state = state_cpu;
		    cpu++;
		} else
		    throw std::runtime_error("error parsing domain");
			    
		// fall through
	    case state_cpu:
		std::cout << "c:" << type << std::endl;
		state = state_domain;
		domain = 0;
		break;
	}
    }

    return 0;
}
