/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_egl_backend.h"
// kwin
#include "basiceglsurfacetexture_internal.h"
#include "basiceglsurfacetexture_wayland.h"
#include "virtual_logging.h"
#include "softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_output.h"
// kwin libs
#include <drm_fourcc.h>
#include <kwinglutils.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

namespace KWin
{

VirtualEglLayer::VirtualEglLayer(Output *output, VirtualEglBackend *backend)
    : m_backend(backend)
    , m_output(output)
{
}

VirtualEglLayer::~VirtualEglLayer() = default;

std::shared_ptr<GLTexture> VirtualEglLayer::texture() const
{
    return m_texture;
}

std::optional<OutputLayerBeginFrameInfo> VirtualEglLayer::beginFrame()
{
    m_backend->makeCurrent();

    const QSize nativeSize = m_output->geometry().size() * m_output->scale();
    if (!m_texture || m_texture->size() != nativeSize) {
        m_fbo.reset();
        m_texture = std::make_unique<GLTexture>(GL_RGB8, nativeSize);
        m_fbo = std::make_unique<GLFramebuffer>(m_texture.get());
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_fbo.get()),
        .repaint = infiniteRegion(),
    };
}

bool VirtualEglLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    glFlush(); // flush pending rendering commands.
    Q_EMIT m_output->outputChange(damagedRegion);
    return true;
}

quint32 VirtualEglLayer::format() const
{
    // the texture format is hardcoded in VirtualEglLayer::beginFrame
    return DRM_FORMAT_RGB888;
}

VirtualEglBackend::VirtualEglBackend(VirtualBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
}

VirtualEglBackend::~VirtualEglBackend()
{
    m_outputs.clear();
    cleanup();
}

bool VirtualEglBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_backend->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        // first try surfaceless
        if (hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_surfaceless"))) {
            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        } else {
            qCWarning(KWIN_VIRTUAL) << "Extension EGL_MESA_platform_surfaceless not available";
        }
    }

    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    setEglDisplay(display);
    return initEglAPI();
}

void VirtualEglBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initKWinGL();
    if (checkGLError("Init")) {
        setFailed("Error during init of EglGbmBackend");
        return;
    }

    setSupportsBufferAge(false);
    initWayland();

    const auto outputs = m_backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }

    connect(m_backend, &VirtualBackend::outputAdded, this, &VirtualEglBackend::addOutput);
    connect(m_backend, &VirtualBackend::outputRemoved, this, &VirtualEglBackend::removeOutput);
}

bool VirtualEglBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
        return false;
    }

    return makeCurrent();
}

void VirtualEglBackend::addOutput(Output *output)
{
    makeCurrent();
    m_outputs[output] = std::make_unique<VirtualEglLayer>(output, this);
}

void VirtualEglBackend::removeOutput(Output *output)
{
    makeCurrent();
    m_outputs.erase(output);
}

std::unique_ptr<SurfaceTexture> VirtualEglBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureInternal>(this, pixmap);
}

std::unique_ptr<SurfaceTexture> VirtualEglBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return std::make_unique<BasicEGLSurfaceTextureWayland>(this, pixmap);
}

OutputLayer *VirtualEglBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

void VirtualEglBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();
}

std::shared_ptr<GLTexture> VirtualEglBackend::textureForOutput(Output *output) const
{
    auto it = m_outputs.find(output);
    if (it == m_outputs.end()) {
        return nullptr;
    }
    return it->second->texture();
}

} // namespace
