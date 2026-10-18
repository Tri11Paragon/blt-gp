#pragma once
/*
 *  Copyright (C) 2024  Brett Terpstra
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BLT_GP_DEFINES_H
#define BLT_GP_DEFINES_H

#include <blt/std/defines.h>

#if BLT_DEBUG_LEVEL > 0
	#if defined(__has_include) &&__has_include(<opentelemetry/version.h>)
		#define BLT_DEBUG_OTEL_ENABLED 1
	#endif
#endif

#if BLT_DEBUG_LEVEL > 1
	#define BLT_GP_DEBUG_TRACK_ALLOCATIONS
#endif

#if BLT_DEBUG_LEVEL > 2
	#define BLT_GP_DEBUG_CHECK_TREES
#endif

#ifdef BLT_GP_DEBUG_TRACK_ALLOCATIONS
	#undef BLT_GP_DEBUG_TRACK_ALLOCATIONS
	#define BLT_GP_DEBUG_TRACK_ALLOCATIONS
#endif

#ifdef BLT_GP_DEBUG_CHECK_TREES
	#undef BLT_GP_DEBUG_CHECK_TREES
	#define BLT_GP_DEBUG_CHECK_TREES 1
#endif

#endif //BLT_GP_DEFINES_H
