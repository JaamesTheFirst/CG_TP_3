#pragma once

#include <GL/glew.h>
#include <filesystem>
#include <string>

namespace gfx {

// Loads a PNG texture into GPU memory using libpng.
bool LoadTexture2D(const std::filesystem::path& path,
                   GLuint& outTexture,
                   std::string* error = nullptr);

} // namespace gfx

