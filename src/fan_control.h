/*
 * fan_control.h
 *
 *  Created on: 05.10.2015
 *      Author: ich
 */

#ifndef THINKFAN_FAN_CONTROL_H_
#define THINKFAN_FAN_CONTROL_H_

class FanControl {
public:
	FanControl();
	virtual ~FanControl();

	void run();
};

#endif /* THINKFAN_FAN_CONTROL_H_ */
