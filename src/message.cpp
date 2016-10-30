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
		l = TF_NFY;
	else if (l == TF_NFY)
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
		l = TF_NFY;
	else if (l == TF_NFY)
		l = TF_INF;
	else if (l == TF_INF)
		l = TF_DBG;
	return l;
}


Logger::Logger()
: syslog_(false),
  log_lvl_(DEFAULT_LOG_LVL),
  msg_lvl_(DEFAULT_LOG_LVL)
{}


Logger &Logger::instance()
{
	if (!instance_) instance_ = std::unique_ptr<Logger>(new Logger());
	return *instance_;
}


LogLevel &Logger::log_lvl()
{ return log_lvl_; }


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
#ifndef DISABLE_SYSLOG
	openlog("thinkfan", LOG_CONS, LOG_USER);
	syslog_ = true;
#endif //DISABLE_SYSLOG
}


Logger &Logger::flush()
{
	if (msg_pfx_.length() == 0) return *this;
	if (msg_lvl_ <= log_lvl_) {
		if (syslog_) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
			// I think we can safely do this because thinkfan doesn't receive
			// any data from unprivileged processes.
			syslog(msg_lvl_, msg_pfx_.c_str());
#pragma GCC diagnostic pop
		}
		else {
			std::cerr << msg_pfx_ << std::endl;
		}
	}
	msg_pfx_ = "";

	return *this;
}


Logger &Logger::level(const LogLevel &lvl)
{
	flush();
	if (!syslog_ && msg_lvl_ != lvl && lvl >= log_lvl_ && msg_lvl_ >= log_lvl_)
		msg_pfx_ = "\n";
	else
		msg_pfx_ = "";

	if (lvl == TF_WRN)
		msg_pfx_ += "WARNING: ";
	else if (lvl == TF_ERR)
		msg_pfx_ += "ERROR: ";

	this->msg_lvl_ = lvl;
	return *this;
}


Logger &Logger::operator<<( const std::string &msg)
{ msg_pfx_ += msg; return *this; }

Logger &Logger::operator<< (const int i)
{ msg_pfx_ += std::to_string(i); return *this; }

Logger &Logger::operator<< (const unsigned int i)
{ msg_pfx_ += std::to_string(i); return *this; }

Logger &Logger::operator<< (const float &i)
{ msg_pfx_ += std::to_string(i); return *this; }

Logger &Logger::operator<< (const char *msg)
{ msg_pfx_ += msg; return *this; }

Logger &Logger::operator<< (Logger & (*pf_flush)(Logger &))
{ return pf_flush(*this); }


Logger &Logger::operator<< (const TemperatureState &ts)
{
	msg_pfx_ += "Temperatures(bias): ";

	std::vector<float>::const_iterator bias_it;
	std::vector<int>::const_iterator temp_it;

	for (temp_it = ts.temps().cbegin(), bias_it = ts.biases().cbegin();
			temp_it != ts.temps().cend() && bias_it != ts.biases().cend();
			++temp_it, ++bias_it)
		msg_pfx_ += std::to_string(*temp_it) + "(" + std::to_string(int(*bias_it)) + "), ";

	msg_pfx_.pop_back(); msg_pfx_.pop_back();
	return *this;
}


}
