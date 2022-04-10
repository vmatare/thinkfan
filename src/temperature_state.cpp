/********************************************************************
 * temperature_state.cpp: Storage & math for current temperatures
 * (C) 2022, Victor Mataré
 *
 * this file is part of thinkfan.
 *
 * thinkfan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * thinkfan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with thinkfan.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ******************************************************************/

#include "temperature_state.h"
#include <cmath>

namespace thinkfan {


TemperatureState::TemperatureState(unsigned int num_temps)
: temps_(num_temps, 0),
  biases_(num_temps, 0),
  biased_temps_(num_temps, 0),
  refd_temps_(0),
  tmax(biased_temps_.begin())
{}

TemperatureState::Ref::Ref(TemperatureState &ts, unsigned int offset)
: temp0_(ts.temps_.begin() + offset),
  bias0_(ts.biases_.begin() + offset),
  biased_temp0_(ts.biased_temps_.begin() + offset),
  temp_(temp0_),
  bias_(bias0_),
  biased_temp_(biased_temp0_),
  tstate_(&ts)
{}



TemperatureState::Ref::Ref()
{}

void TemperatureState::Ref::restart()
{
	temp_ = temp0_;
	bias_ = bias0_;
	biased_temp_ = biased_temp0_;
}


void TemperatureState::Ref::add_temp(int t)
{
	int diff = *temp_ > 0 ?
		t - *temp_
		: 0;
	*temp_ = t;

	if (unlikely(diff > 2)) {
		// Apply bias_ if temperature changed quickly
		*bias_ = int(float(diff) * bias_level);

		if (tmp_sleeptime > seconds(2))
			tmp_sleeptime = seconds(2);
	}
	else {
		// Slowly return to normal sleeptime
		if (unlikely(tmp_sleeptime < sleeptime))
			tmp_sleeptime++;
		// slowly reduce the bias_
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal" // bias is set to 0 explicitly
		if (unlikely(*bias_ != 0)) {
#pragma GCC diagnostic pop
			if (std::abs(*bias_) < 0.5f)
				*bias_ = 0;
			else
				*bias_ -= std::copysign(1 + std::abs(*bias_)/5, *bias_);
		}
	}

	*biased_temp_ = *temp_ + int(*bias_);

	if (*biased_temp_ > *tstate_->tmax)
		tstate_->tmax = biased_temp_;

	skip_temp();
}

void TemperatureState::Ref::skip_temp()
{
	++temp_;
	++bias_;
	++biased_temp_;
}



const vector<int> & TemperatureState::biased_temps() const
{ return biased_temps_; }

const vector<int> & TemperatureState::temps() const
{ return temps_; }

const vector<float> & TemperatureState::biases() const
{ return biases_; }

void TemperatureState::reset_refd_count()
{ refd_temps_ = 0; }


TemperatureState::Ref TemperatureState::ref(unsigned int num_temps)
{
	auto ref = TemperatureState::Ref(*this, refd_temps_);
	refd_temps_ += num_temps;
	return ref;
}


} // namespace thinkfan
