/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __LUNARGLASS_DEFINITIONS_HPP__
#define __LUNARGLASS_DEFINITIONS_HPP__

#ifdef UTIL_LUNARGLASS_DLL
	#ifdef __linux__
		#define DLLLUNARGLASS __attribute__((visibility("default")))
	#else
		#define DLLLUNARGLASS  __declspec(dllexport)
	#endif
#else
	#ifdef LUNARGLASS_EXE
		#define DLLLUNARGLASS
	#else
		#ifdef __linux__
			#define DLLLUNARGLASS
		#else
			#define DLLLUNARGLASS  __declspec(dllimport)
		#endif
	#endif
#endif

#endif
