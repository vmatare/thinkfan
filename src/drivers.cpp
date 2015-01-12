#include "drivers.h"

namespace thinkfan {


TpFanDriver::TpFanDriver(string path) :
		path(path)
{}


void TpFanDriver::set_speed(int speed)
{}


HwmonFanDriver::HwmonFanDriver(string path) :
		path(path)
{}


void HwmonFanDriver::set_speed(int speed)
{}

}
