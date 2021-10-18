#ifndef SPLIT_H
#define SPLIT_H

#include <string>
#include <vector>
#include <list>
#include <cstring>

template <class T, class AGG> void split(const std::string &str, AGG &xsplit, T sep, int count=0) {
	int splitcount = 0;
	size_t size = str.size();
	size_t pos = 0;
	for(pos = 0; (count == 0 || splitcount < count) && pos < size;) {
		int endpos = str.find(sep, pos);
		int nextpos = endpos;
		if(endpos == -1)
			nextpos = endpos = str.size();
		else
			nextpos++;
		xsplit.push_back(str.substr(pos, endpos-pos));
		splitcount++;
		pos = nextpos;
	}
	if(pos < str.size())
		xsplit.push_back(str.substr(pos));

}

template <class T, class AGG> std::string join(const AGG &xsplit, T sep) {
	std::string combined;
	std::string div;
	for(typename AGG::const_iterator i_part = xsplit.begin();
		i_part != xsplit.end(); i_part++) {
		combined += div + *i_part;
		div = sep;
	}
	return combined;
}

#endif // include guard SPLIT_H
