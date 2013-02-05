/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HWUI_LAYER_H
#define ANDROID_HWUI_LAYER_H

#include <sys/types.h>

#include <GLES2/gl2.h>

#include <ui/Region.h>

#include <SkXfermode.h>

#include "Rect.h"
#include "SkiaColorFilter.h"
#include "Texture.h"
#include "Vertex.h"

namespace android {
namespace uirenderer {

///////////////////////////////////////////////////////////////////////////////
// Layers
///////////////////////////////////////////////////////////////////////////////

// Forward declarations
class OpenGLRenderer;
class DisplayList;

/**
 * A layer has dimensions and is backed by an OpenGL texture or FBO.
 */
struct Layer {
    Layer(const uint32_t layerWidth, const uint32_t layerHeight);
    ~Layer();

    static uint32_t computeIdealWidth(uint32_t layerWidth);
    static uint32_t computeIdealHeight(uint32_t layerHeight);

    /**
     * Calling this method will remove (either by recycling or
     * destroying) the associated FBO, if present, and any render
     * buffer (stencil for instance.)
     */
    void removeFbo(bool flush = true);

    /**
     * Sets this layer's region to a rectangle. Computes the appropriate
     * texture coordinates.
     */
    void setRegionAsRect() {
        const android::Rect& bounds = region.getBounds();
        regionRect.set(bounds.leftTop().x, bounds.leftTop().y,
               bounds.rightBottom().x, bounds.rightBottom().y);

        const float texX = 1.0f / float(texture.width);
        const float texY = 1.0f / float(texture.height);
        const float height = layer.getHeight();
        texCoords.set(
               regionRect.left * texX, (height - regionRect.top) * texY,
               regionRect.right * texX, (height - regionRect.bottom) * texY);

        regionRect.translate(layer.left, layer.top);
    }

    void updateDeferred(OpenGLRenderer* renderer, DisplayList* displayList,
            int left, int top, int right, int bottom) {
        this->renderer = renderer;
        this->displayList = displayList;
        const Rect r(left, top, right, bottom);
        dirtyRect.unionWith(r);
        deferredUpdateScheduled = true;
    }

    inline uint32_t getWidth() {
        return texture.width;
    }

    inline uint32_t getHeight() {
        return texture.height;
    }

    /**
     * Resize the layer and its texture if needed.
     *
     * @param width The new width of the layer
     * @param height The new height of the layer
     *
     * @return True if the layer was resized or nothing happened, false if
     *         a failure occurred during the resizing operation
     */
    bool resize(const uint32_t width, const uint32_t height);

    void setSize(uint32_t width, uint32_t height) {
        texture.width = width;
        texture.height = height;
    }

    ANDROID_API void setPaint(SkPaint* paint);

    inline void setBlend(bool blend) {
        texture.blend = blend;
    }

    inline bool isBlend() {
        return texture.blend;
    }

    inline void setAlpha(int alpha) {
        this->alpha = alpha;
    }

    inline void setAlpha(int alpha, SkXfermode::Mode mode) {
        this->alpha = alpha;
        this->mode = mode;
    }

    inline int getAlpha() {
        return alpha;
    }

    inline SkXfermode::Mode getMode() {
        return mode;
    }

    inline void setEmpty(bool empty) {
        this->empty = empty;
    }

    inline bool isEmpty() {
        return empty;
    }

    inline void setFbo(GLuint fbo) {
        this->fbo = fbo;
    }

    inline GLuint getFbo() {
        return fbo;
    }

    inline void setStencilRenderBuffer(GLuint renderBuffer) {
        this->stencil = renderBuffer;
    }

    inline GLuint getStencilRenderBuffer() {
        return stencil;
    }

    inline GLuint getTexture() {
        return texture.id;
    }

    inline GLenum getRenderTarget() {
        return renderTarget;
    }

    inline void setRenderTarget(GLenum renderTarget) {
        this->renderTarget = renderTarget;
    }

    void setWrap(GLenum wrap, bool bindTexture = false, bool force = false) {
        texture.setWrap(wrap, bindTexture, force, renderTarget);
    }

    void setFilter(GLenum filter, bool bindTexture = false, bool force = false) {
        texture.setFilter(filter, bindTexture, force, renderTarget);
    }

    inline bool isCacheable() {
        return cacheable;
    }

    inline void setCacheable(bool cacheable) {
        this->cacheable = cacheable;
    }

    inline bool isDirty() {
        return dirty;
    }

    inline void setDirty(bool dirty) {
        this->dirty = dirty;
    }

    inline bool isTextureLayer() {
        return textureLayer;
    }

    inline void setTextureLayer(bool textureLayer) {
        this->textureLayer = textureLayer;
    }

    inline SkiaColorFilter* getColorFilter() {
        return colorFilter;
    }

    ANDROID_API void setColorFilter(SkiaColorFilter* filter);

    inline void bindTexture() {
        if (texture.id) {
            glBindTexture(renderTarget, texture.id);
        }
    }

    inline void bindStencilRenderBuffer() {
        if (stencil) {
            glBindRenderbuffer(GL_RENDERBUFFER, stencil);
        }
    }

    inline void generateTexture() {
        if (!texture.id) {
            glGenTextures(1, &texture.id);
        }
    }

    inline void deleteTexture() {
        if (texture.id) {
            glDeleteTextures(1, &texture.id);
            texture.id = 0;
        }
    }

    /**
     * When the caller frees the texture itself, the caller
     * must call this method to tell this layer that it lost
     * the texture.
     */
    void clearTexture() {
        texture.id = 0;
    }

    inline void allocateTexture(GLenum format, GLenum storage) {
#if DEBUG_LAYERS
        ALOGD("  Allocate layer: %dx%d", getWidth(), getHeight());
#endif
        if (texture.id) {
            glTexImage2D(renderTarget, 0, format, getWidth(), getHeight(), 0,
                    format, storage, NULL);
        }
    }

    inline void allocateStencilRenderBuffer() {
        if (stencil) {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, getWidth(), getHeight());
        }
    }

    inline mat4& getTexTransform() {
        return texTransform;
    }

    inline mat4& getTransform() {
        return transform;
    }

    /**
     * Bounds of the layer.
     */
    Rect layer;
    /**
     * Texture coordinates of the layer.
     */
    Rect texCoords;
    /**
     * Clipping rectangle.
     */
    Rect clipRect;

    /**
     * Dirty region indicating what parts of the layer
     * have been drawn.
     */
    Region region;
    /**
     * If the region is a rectangle, coordinates of the
     * region are stored here.
     */
    Rect regionRect;

    /**
     * If the layer can be rendered as a mesh, this is non-null.
     */
    TextureVertex* mesh;
    uint16_t* meshIndices;
    GLsizei meshElementCount;

    /**
     * Used for deferred updates.
     */
    bool deferredUpdateScheduled;
    OpenGLRenderer* renderer;
    DisplayList* displayList;
    Rect dirtyRect;
    bool debugDrawUpdate;

private:
    /**
     * Name of the FBO used to render the layer. If the name is 0
     * this layer is not backed by an FBO, but a simple texture.
     */
    GLuint fbo;

    /**
     * Name of the render buffer used as the stencil buffer. If the
     * name is 0, this layer does not have a stencil buffer.
     */
    GLuint stencil;

    /**
     * Indicates whether this layer has been used already.
     */
    bool empty;

    /**
     * The texture backing this layer.
     */
    Texture texture;

    /**
     * If set to true (by default), the layer can be reused.
     */
    bool cacheable;

    /**
     * When set to true, this layer must be treated as a texture
     * layer.
     */
    bool textureLayer;

    /**
     * When set to true, this layer is dirty and should be cleared
     * before any rendering occurs.
     */
    bool dirty;

    /**
     * Indicates the render target.
     */
    GLenum renderTarget;

    /**
     * Color filter used to draw this layer. Optional.
     */
    SkiaColorFilter* colorFilter;

    /**
     * Opacity of the layer.
     */
    int alpha;
    /**
     * Blending mode of the layer.
     */
    SkXfermode::Mode mode;

    /**
     * Optional texture coordinates transform.
     */
    mat4 texTransform;

    /**
     * Optional transform.
     */
    mat4 transform;

}; // struct Layer

}; // namespace uirenderer
}; // namespace android

#endif // ANDROID_HWUI_LAYER_H