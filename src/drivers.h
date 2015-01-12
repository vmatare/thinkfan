#ifndef THINKFAN_DRIVERS_H_
#define THINKFAN_DRIVERS_H_

#include <string>
#include <fstream>

namespace thinkfan {

using namespace std;


class FanDriver {
public:
	virtual ~FanDriver() = default;
	virtual void set_speed(int speed) = 0;
};


class TpFanDriver : public FanDriver {
private:
	const string path;
public:
	TpFanDriver(const string path);
	void set_speed(int speed);
};


class HwmonFanDriver : public FanDriver {
private:
	const string path;
public:
	HwmonFanDriver(string path);
	void set_speed(int speed);
};

}

#endif /* THINKFAN_DRIVERS_H_ */
