/********************************************************************
 * message.cpp: Logging and error management.
 * (C) 2015, Victor Mataré
 *
 * this file is part of thinkfan. See thinkfan.c for further information.
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

#include "error.h"
#include "thinkfan.h"
#include "message.h"
#include <syslog.h>
#include <iostream>


namespace thinkfan {

std::unique_ptr<Logger> Logger::instance_(nullptr);


LogLevel &operator--(LogLevel &l)
{
	if (l == TF_DBG)
		l = TF_INF;
	else if (l == TF_INF)
		l = TF_WRN;
	else
		l = TF_ERR;
	return l;
}


LogLevel &operator++(LogLevel &l)
{
	if (l == TF_ERR)
		l = TF_WRN;
	else if (l == TF_WRN)
		l = TF_INF;
	else if (l == TF_INF)
		l = TF_DBG;
	return l;
}


Logger::Logger()
: syslog_(false),
  min_lvl_(TF_INF),
  log_lvl_(TF_INF)
{}


Logger &Logger::instance()
{
	if (!instance_) instance_ = std::unique_ptr<Logger>(new Logger());
	return *instance_;
}


const LogLevel Logger::set_min_lvl(const LogLevel &min)
{
	LogLevel rv = min_lvl_;
	min_lvl_ = min;
	return rv;
}


LogLevel &Logger::min_lvl()
{ return min_lvl_; }


Logger &flush(Logger &l) { return l.flush(); }


Logger &log(LogLevel lvl)
{
	Logger::instance().level(lvl);
	return Logger::instance();
}


Logger::~Logger()
{
	flush();
	if (syslog_) closelog();
}


void Logger::enable_syslog()
{
	openlog("thinkfan", LOG_CONS, LOG_USER);
	syslog_ = true;
}


Logger &Logger::flush()
{
	if (log_str_.length() == 0) return *this;
	if (!min_lvl_ || log_lvl_ <= min_lvl_) {
		if (syslog_) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
			syslog(log_lvl_, log_str_.c_str());
#pragma GCC diagnostic pop
		}
		else {
			std::cerr << log_str_ << std::endl;
		}
	}
	log_str_ = "";

	return *this;
}


Logger &Logger::level(const LogLevel &lvl)
{
	flush();
	if (this->log_lvl_ != lvl)
		if (!syslog_)
			std::cerr << std::endl;

	if (lvl == TF_WRN)
		log_str_ = "WARNING: ";
	else if (lvl == TF_ERR)
		log_str_ = "ERROR: ";

	this->log_lvl_ = lvl;
	return *this;
}


Logger &Logger::operator<<( const std::string &msg)
{ log_str_ += msg; return *this; }

Logger &Logger::operator<< (const int i)
{ log_str_ += std::to_string(i); return *this; }

Logger &Logger::operator<< (const unsigned int i)
{ log_str_ += std::to_string(i); return *this; }

Logger &Logger::operator<< (const float &i)
{ log_str_ += std::to_string(i); return *this; }

Logger &Logger::operator<< (const char *msg)
{ log_str_ += msg; return *this; }

Logger &Logger::operator<< (Logger & (*pf_flush)(Logger &))
{ return pf_flush(*this); }


Logger &Logger::operator<< (const TemperatureState &ts)
{
	log_str_ += "Temperatures(bias): ";

	std::vector<float>::const_iterator bias_it;
	std::vector<int>::const_iterator temp_it;

	for (temp_it = ts.get().cbegin(), bias_it = ts.biases().cbegin();
			temp_it != ts.get().cend() && bias_it != ts.biases().cend();
			++temp_it, ++bias_it)
		log_str_ += std::to_string(*temp_it) + "(" + std::to_string(int(*bias_it)) + "), ";

	log_str_.pop_back(); log_str_.pop_back();
	return *this;
}


}
