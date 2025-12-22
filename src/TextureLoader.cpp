#include "TextureLoader.hpp"

#include <png.h>

#include <cstdio>
#include <memory>
#include <setjmp.h>
#include <vector>

namespace gfx {
namespace {

struct FileCloser {
    void operator()(FILE* file) const {
        if (file) {
            std::fclose(file);
        }
    }
};

} // namespace

bool LoadTexture2D(const std::filesystem::path& path,
                   GLuint& outTexture,
                   std::string* error) {
    std::unique_ptr<FILE, FileCloser> file(std::fopen(path.string().c_str(), "rb"));
    if (!file) {
        if (error) {
            *error = "Unable to open texture file: " + path.string();
        }
        return false;
    }

    png_byte header[8];
    if (std::fread(header, 1, 8, file.get()) != 8 || png_sig_cmp(header, 0, 8)) {
        if (error) {
            *error = "File is not a valid PNG: " + path.string();
        }
        return false;
    }

    png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!pngPtr) {
        if (error) {
            *error = "Unable to allocate png read struct.";
        }
        return false;
    }

    png_infop infoPtr = png_create_info_struct(pngPtr);
    if (!infoPtr) {
        png_destroy_read_struct(&pngPtr, nullptr, nullptr);
        if (error) {
            *error = "Unable to allocate png info struct.";
        }
        return false;
    }

    if (setjmp(png_jmpbuf(pngPtr))) {
        png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);
        if (error) {
            *error = "Error while reading PNG file: " + path.string();
        }
        return false;
    }

    png_init_io(pngPtr, file.get());
    png_set_sig_bytes(pngPtr, 8);
    png_read_info(pngPtr, infoPtr);

    png_uint_32 width = png_get_image_width(pngPtr, infoPtr);
    png_uint_32 height = png_get_image_height(pngPtr, infoPtr);
    png_byte colorType = png_get_color_type(pngPtr, infoPtr);
    png_byte bitDepth = png_get_bit_depth(pngPtr, infoPtr);

    if (bitDepth == 16) {
        png_set_strip_16(pngPtr);
    }

    if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(pngPtr);
    }

    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(pngPtr);
    }

    if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(pngPtr);
    }

    if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_filler(pngPtr, 0xFF, PNG_FILLER_AFTER);
    }

    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(pngPtr);
    }

    png_read_update_info(pngPtr, infoPtr);

    png_size_t rowBytes = png_get_rowbytes(pngPtr, infoPtr);
    std::vector<png_byte> imageData(rowBytes * height);
    std::vector<png_bytep> rowPointers(height);

    for (png_uint_32 y = 0; y < height; ++y) {
        // Flip vertically so textures appear correctly in OpenGL
        rowPointers[height - 1 - y] = imageData.data() + y * rowBytes;
    }

    png_read_image(pngPtr, rowPointers.data());
    png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);

    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

} // namespace gfx

