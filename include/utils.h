#pragma once

#include <vector>
#include <array>
#include <string>

#include <cstddef>
#include <cstdlib>
#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>

// Note: must include vulkan before GLFW
//#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/string_cast.hpp"

using namespace std;
using namespace glm;

void handle_segfault(int sig_num);

string vec3_str(vec3 v);
string vec4_str(vec4 v);
string ivec4_str(ivec4 v);

