/**
 * @file mad_log.h
 * @author Phil Schatzmann
 * @brief Simple Logging
 * @version 0.1
 * @date 2021-08-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once

// User Settings: Activate/Deactivate logging
#ifndef MAD_LOGGING_ACTIVE
#define MAD_LOGGING_ACTIVE false
#endif
#ifndef MAD_LOG_LEVEL
#define MAD_LOG_LEVEL LogLevelMAD::Warning
#endif

// Logging Implementation
#if MAD_LOGGING_ACTIVE == true
static char log_buffer_mad[512];
enum class LogLevelMAD {Debug, Info, Warning, Error};
static LogLevelMAD minLogLevelMAD = MAD_LOG_LEVEL;
// We print the log based on the log level
#define LOG(level,...) { if(level>=minLogLevelMAD) { int l = snprintf(log_buffer_mad,512, __VA_ARGS__);  Serial.write(log_buffer_mad,l); Serial.println(); } }
#else
// Remove all log statments from the code
#define LOG(Debug, ...) 
#endif
