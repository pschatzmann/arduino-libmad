/**
 * @file mad_pgmspace.h
 * @author Phil Schatzmann
 * @brief Ignore <pgmspace.h> for non relevant processors
 * @version 0.1
 * @date 2021-08-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once
#if __has_include(<pgmspace.h>)
#include <pgmspace.h>
#else
#define PROGMEM
#define memcpy_P memcpy
#define PSTR(str) (str)
#endif