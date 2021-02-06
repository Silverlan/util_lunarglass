/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright (c) 2020 Florian Weischer
*/

#ifndef __UNIRENDER_CYCLES_SCENE_HPP__
#define __UNIRENDER_CYCLES_SCENE_HPP__

#include "util_lunarglass/lunarglass_definitions.hpp"
#include <optional>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

namespace lunarglass
{
	enum class ShaderStage : uint8_t
	{
		Compute = 0,
		Fragment = 1,
		Geometry = 2,
		TessellationControl = 3,
		TessellationEvaluation = 4,
		Vertex = 5,

		Count
	};
	DLLLUNARGLASS std::optional<std::unordered_map<ShaderStage,std::string>> optimize_glsl(const std::unordered_map<ShaderStage,std::string> &shaderStages,std::string &outInfoLog);
};

#endif
