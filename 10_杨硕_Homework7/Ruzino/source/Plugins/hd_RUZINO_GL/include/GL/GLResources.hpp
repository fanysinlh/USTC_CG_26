#pragma once

#include <filesystem>

#include "../../api.h"
#include "GL/shader.hpp"
#include "pxr/base/gf/vec2i.h"
#include "pxr/imaging/garch/glApi.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/imaging/hio/types.h"

RUZINO_NAMESPACE_OPEN_SCOPE
#define RESOURCE_LIST GLTexture, GLShader

////////////////////////////////Shader/////////////////////////////////////////

struct GLShaderResource;
using GLShaderHandle = std::shared_ptr<GLShaderResource>;

struct HD_RUZINO_GL_API GLShaderDesc {
    friend bool operator==(const GLShaderDesc& lhs, const GLShaderDesc& rhs)
    {
        return lhs.vertexPath == rhs.vertexPath &&
               lhs.fragmentPath == rhs.fragmentPath &&
               lhs.lastWriteTime == rhs.lastWriteTime;
    }

    friend bool operator!=(const GLShaderDesc& lhs, const GLShaderDesc& rhs)
    {
        return !(lhs == rhs);
    }

    void set_vertex_path(const std::filesystem::path& vertex_path);

    void set_fragment_path(const std::filesystem::path& fragment_path);

   private:
    void update_last_write_time(const std::filesystem::path& path);

    friend HD_RUZINO_GL_API GLShaderHandle
    createGLShader(const GLShaderDesc& desc);
    std::filesystem::path vertexPath;
    std::filesystem::path fragmentPath;
    std::filesystem::file_time_type lastWriteTime;
};

struct GLShaderResource {
    GLShaderDesc desc;
    Shader shader;

    GLShaderResource(const char* vertexPath, const char* fragmentPath)
        : shader(vertexPath, fragmentPath)
    {
    }

    ~GLShaderResource()
    {
    }
};

using GLShaderHandle = std::shared_ptr<GLShaderResource>;
HD_RUZINO_GL_API GLShaderHandle createGLShader(const GLShaderDesc& desc);

////////////////////////////////GLTexture///////////////////////////////////////

struct GLTextureDesc {
    pxr::GfVec2i size;
    pxr::HdFormat format;

    unsigned array_size = 1;

    friend bool operator==(const GLTextureDesc& lhs, const GLTextureDesc& rhs)
    {
        return lhs.size == rhs.size && lhs.format == rhs.format &&
               lhs.array_size == rhs.array_size;
    }

    friend bool operator!=(const GLTextureDesc& lhs, const GLTextureDesc& rhs)
    {
        return !(lhs == rhs);
    }
};

struct GLTextureResource {
    GLTextureDesc desc;
    GLuint texture_id;

    ~GLTextureResource()
    {
        glDeleteTextures(1, &texture_id);
    }
};

using GLTextureHandle = std::shared_ptr<GLTextureResource>;
HD_RUZINO_GL_API GLTextureHandle createGLTexture(const GLTextureDesc& desc);

#define DESC_HANDLE_TRAIT(RESOURCE)        \
    template<>                             \
    struct ResouceDesc<RESOURCE##Handle> { \
        using Desc = RESOURCE##Desc;       \
    };

#define HANDLE_DESC_TRAIT(RESOURCE)        \
    template<>                             \
    struct DescResouce<RESOURCE##Desc> {   \
        using Resource = RESOURCE##Handle; \
    };

template<typename RESOURCE>
struct ResouceDesc {
    using Desc = void;
};

template<typename DESC>
struct DescResouce {
    using Resource = void;
};

GLenum GetGLFormat(pxr::HdFormat hd_format);
GLenum GetGLType(pxr::HdFormat hd_format);
GLenum GetGLInternalFormat(pxr::HdFormat hd_format);

GLenum GetGLFormat(pxr::HioFormat hd_format);
GLenum GetGLType(pxr::HioFormat hd_format);
GLenum GetGLInternalFormat(pxr::HioFormat hd_format);
RUZINO_NAMESPACE_CLOSE_SCOPE
