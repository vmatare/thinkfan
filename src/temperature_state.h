#pragma once

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

#include "thinkfan.h"

namespace thinkfan {


class TemperatureState {
public:
	template<typename T>
	using Iter = typename vector<T>::iterator;

	class Ref {
	public:
		Ref();
		void add_temp(int t);
		void skip_temp();
		inline int temp() const;
		float bias() const;
		int biased_temp() const;
		void restart();

	private:
		friend TemperatureState;
		Ref(TemperatureState &ts, unsigned int offset);

		Iter<int> temp0_;
		Iter<float> bias0_;
		Iter<int> biased_temp0_;

		Iter<int> temp_;
		Iter<float> bias_;
		Iter<int> biased_temp_;

		TemperatureState *tstate_;
	};

	TemperatureState(unsigned int num_temps);

	const vector<int> &biased_temps() const;
	const vector<int> &temps() const;
	const vector<float> &biases() const;

	Ref ref(unsigned int num_temps);

	void reset_refd_count();

private:
	vector<int> temps_;
	vector<float> biases_;
	vector<int> biased_temps_;
	unsigned int refd_temps_;

public:
	vector<int>::const_iterator tmax;
};


} // namespace thinkfan
