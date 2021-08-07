/**
 * @file mad_config.h
 * @author Phil Schatzmann
 * @brief Detemrine the processor to define the correct FPM implementation
 * @version 0.1
 * @date 2021-08-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once

#if defined(__x86_64__)
//#define FPM_64BIT
#define FPM_INTEL
#endif

#if defined(__i386__)
#define FPM_INTEL
#endif

#if defined(__arm__)
#define FPM_ARM
#endif

