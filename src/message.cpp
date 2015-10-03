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

#include "message.h"
#include "thinkfan.h"
#include "error.h"
#include <syslog.h>
#include <iostream>


namespace thinkfan {

std::unique_ptr<Logger> Logger::instance_(nullptr);

Logger::Logger()
: syslog_(false),
  log_lvl_(TF_ERR)
{}


Logger &Logger::instance()
{
	if (!instance_) instance_ = std::unique_ptr<Logger>(new Logger());
	return *instance_;
}


Logger &flush(Logger &l) { return l.flush(); }


Logger &fail(LogLevel lvl_insane)
{ return log(TF_ERR, lvl_insane); }


Logger &log(LogLevel lvl_sane, LogLevel lvl_insane)
{
	Logger::instance().level(chk_sanity ? lvl_sane : lvl_insane);
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
	if (syslog_) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
		syslog(log_lvl_, log_str_.c_str());
#pragma GCC diagnostic pop
	}
	else {
		std::cerr << log_str_ << std::endl;
	}
	log_str_ = "";

	if (log_lvl_ == TF_ERR && exception_) {
		std::rethrow_exception(exception_);
	}
	exception_ = nullptr;
	return *this;
}


Logger &Logger::level(const LogLevel &lvl)
{
	flush();
	this->log_lvl_ = lvl;
	switch (lvl) {
	case TF_WRN:
		log_str_ = "WARNING: ";
		break;
	case TF_ERR:
		log_str_ = "ERROR: ";
		break;
	default:
		;
	}
	return *this;
}


Logger &Logger::operator <<(const std::string &msg)
{ log_str_ += msg; return *this; }

Logger &Logger::operator <<(const int i)
{ log_str_ += std::to_string(i); return *this; }

Logger &Logger::operator <<(const unsigned int i)
{ log_str_ += std::to_string(i); return *this; }

Logger &Logger::operator <<(const float &i)
{ log_str_ += std::to_string(i); return *this; }

Logger &Logger::operator <<(Logger & (*pf_flush)(Logger &))
{ return pf_flush(*this); }


Logger &Logger::operator <<(Error e)
{
	log_str_ += e.what();
	exception_ = std::make_exception_ptr(e);
	return *this;
}



}
