// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#pragma clang diagnostic pop

namespace TTauri {

#ifdef _WIN32
std::string getLastErrorMessage();
#endif

void initializeLogging();

}

#define LOG_TRACE(fmt) BOOST_LOG_TRIVIAL(trace) << boost::format(fmt)
#define LOG_DEBUG(fmt) BOOST_LOG_TRIVIAL(debug) << boost::format(fmt)
#define LOG_INFO(fmt) BOOST_LOG_TRIVIAL(info) << boost::format(fmt)
#define LOG_WARNING(fmt) BOOST_LOG_TRIVIAL(warning) << boost::format(fmt)
#define LOG_ERROR(fmt) BOOST_LOG_TRIVIAL(error) << boost::format(fmt)
#define LOG_FATAL(fmt) BOOST_LOG_TRIVIAL(fatal) << boost::format(fmt)
