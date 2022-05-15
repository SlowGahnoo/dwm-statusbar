#ifndef __MODULE_HPP__
#define __MODULE_HPP__
#include <iostream>

class Module {
private:
	virtual std::ostream& update(std::ostream& out) = 0;
public:
	virtual ~Module() {} ;
	friend std::ostream& operator << (std::ostream& out, Module& m) {
		return m.update(out);
	}
};

#endif
