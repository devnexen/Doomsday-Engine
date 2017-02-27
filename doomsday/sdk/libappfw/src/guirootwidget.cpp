/** @file guirootwidget.cpp  Graphical root widget.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/GuiRootWidget"
#include "de/BaseGuiApp"
#include "de/BaseWindow"
#include "de/FocusWidget"
#include "de/GuiWidget"
#include "de/Painter"
#include "de/Style"

#include <de/GLFramebuffer>
#include <de/GLState>
#include <de/GLUniform>
#include <de/GLWindow>
#include <de/TextureBank>

#include <QImage>
#include <QPainter>

namespace de {

// Identifiers for images generated by GuiRootWidget.
static DotPath const ID_SOLID_WHITE         = "GuiRootWidget.solid.white";
static DotPath const ID_SOLID_ROUND_CORNERS = "GuiRootWidget.solid.roundCorners";
static DotPath const ID_THIN_ROUND_CORNERS  = "GuiRootWidget.frame.thin";
static DotPath const ID_BOLD_ROUND_CORNERS  = "GuiRootWidget.frame.bold";
static DotPath const ID_DOT                 = "GuiRootWidget.dot";

#ifdef DENG2_QT_5_0_OR_NEWER
#  define DPI_SCALED(x)       ((x) * DENG2_BASE_GUI_APP->dpiFactor())
#else
#  define DPI_SCALED(x)       (x)
#endif
#define DPI_SCALED_INT(x)     (int(DPI_SCALED(x)))

DENG2_PIMPL(GuiRootWidget)
, DENG2_OBSERVES(Widget, ChildAddition)
, DENG2_OBSERVES(RootWidget, FocusChange)
{
    /*
     * Built-in runtime-generated images:
     */
    struct SolidWhiteImage : public TextureBank::ImageSource {
        Image load() const {
            return Image::solidColor(Image::Color(255, 255, 255, 255),
                                     Image::Size(1, 1));
        }
    };
    struct ThinCornersImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED_INT(15), DPI_SCALED_INT(15)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::white, DPI_SCALED(1)));
            painter.drawEllipse(DPI_SCALED(QPointF(8, 8)), DPI_SCALED(6), DPI_SCALED(6));
            return img;
        }
    };
    struct BoldCornersImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED_INT(12), DPI_SCALED_INT(12)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(Qt::white, DPI_SCALED(2)));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(DPI_SCALED(QPointF(6, 6)), DPI_SCALED(4), DPI_SCALED(4));
            return img;
        }
    };
    struct SolidRoundedImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED_INT(12), DPI_SCALED_INT(12)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawEllipse(DPI_SCALED(QPointF(6, 6)), DPI_SCALED(6), DPI_SCALED(6));
            return img;
        }
    };
    struct TinyDotImage : public TextureBank::ImageSource {
        Image load() const {
            QImage img(QSize(DPI_SCALED_INT(5), DPI_SCALED_INT(5)), QImage::Format_ARGB32);
            img.fill(QColor(255, 255, 255, 0).rgba());
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawEllipse(DPI_SCALED(QPointF(2.5, 2.5)), DPI_SCALED(2), DPI_SCALED(2));
            return img;
        }
    };
    struct StyleImage : public TextureBank::ImageSource {
        StyleImage(DotPath const &sourcePath) : ImageSource(sourcePath) {}
        Image load() const {
            return Style::get().images().image(sourcePath());
        }
    };

    GLWindow *window;
    QScopedPointer<AtlasTexture> atlas; ///< Shared atlas for most UI graphics/text.
    GLUniform uTexAtlas;
    TextureBank texBank; ///< Bank for the atlas contents.
    Painter painter;
    FocusWidget *focusIndicator;
    bool noFramesDrawnYet;
    QList<SafeWidgetPtr<Widget> *> focusStack;

    Impl(Public *i, GLWindow *win)
        : Base(i)
        , window(win)
        , atlas(0)
        , uTexAtlas("uTex", GLUniform::Sampler2D)
        , noFramesDrawnYet(true)
    {
        self().audienceForChildAddition() += this;
        self().audienceForFocusChange() += this;

        // The focus indicator exists outside the widget tree.
        focusIndicator = new FocusWidget;
        focusIndicator->setRoot(thisPublic);
    }

    ~Impl()
    {
        if (window) window->glActivate();

        qDeleteAll(focusStack);

        GuiWidget::recycleTrashedWidgets();

        // Tell all widgets to release their resource allocations. The base
        // class destructor will destroy all widgets, but this class governs
        // shared GL resources, so we'll ask the widgets to do this now.
        focusIndicator->deinitialize();
        self().notifyTree(&Widget::deinitialize);

        // Destroy GUI widgets while the shared resources are still available.
        GuiWidget::destroy(focusIndicator);
        self().clearTree();
    }

    void initAtlas()
    {
        if (atlas.isNull())
        {
            atlas.reset(AtlasTexture::newWithKdTreeAllocator(
                            Atlas::BackingStore | Atlas::AllowDefragment,
                            GLTexture::maximumSize().min(GLTexture::Size(4096, 4096))));
            uTexAtlas = *atlas;
            texBank.setAtlas(atlas.data());

            // Load a set of general purpose textures (derived classes may extend this).
            self().loadCommonTextures();
        }
    }

    void initBankContents()
    {
        // Built-in images.
        texBank.add(ID_SOLID_WHITE,         new SolidWhiteImage);
        texBank.add(ID_SOLID_ROUND_CORNERS, new SolidRoundedImage);
        texBank.add(ID_THIN_ROUND_CORNERS,  new ThinCornersImage);
        texBank.add(ID_BOLD_ROUND_CORNERS,  new BoldCornersImage);
        texBank.add(ID_DOT,                 new TinyDotImage);

        // All style images.
        Style const &st = Style::get();
        ImageBank::Names imageNames;
        st.images().allItems(imageNames);
        foreach (String const &name, imageNames)
        {
            texBank.add("Style." + name, new StyleImage(name));
        }
    }

    void widgetChildAdded(Widget &child)
    {
        // Make sure newly added children know the view size.
        child.viewResized();
        child.notifyTree(&Widget::viewResized);
    }

    void focusedWidgetChanged(Widget *focused)
    {
        if (GuiWidget const *w = focused->maybeAs<GuiWidget>())
        {
            focusIndicator->rule().setRect(w->hitRule());
            if (!w->attributes().testFlag(GuiWidget::FocusHidden))
            {
                focusIndicator->startFlashing(w);
            }
            else
            {
                focusIndicator->stopFlashing();
            }
        }
        else
        {
            focusIndicator->stopFlashing();
        }
    }
};

GuiRootWidget::GuiRootWidget(GLWindow *window)
    : d(new Impl(this, window))
{}

void GuiRootWidget::setWindow(GLWindow *window)
{
    d->window = window;
}

GLWindow &GuiRootWidget::window()
{
    DENG2_ASSERT(d->window != 0);
    return *d->window;
}

void GuiRootWidget::addOnTop(GuiWidget *widget)
{
    add(widget);
}

void GuiRootWidget::moveToTop(GuiWidget &widget)
{
    if (widget.parentWidget())
    {
        widget.parentWidget()->remove(widget);
    }
    addOnTop(&widget);
}

AtlasTexture &GuiRootWidget::atlas()
{
    d->initAtlas();
    return *d->atlas;
}

GLUniform &GuiRootWidget::uAtlas()
{
    d->initAtlas();
    return d->uTexAtlas;
}

Id GuiRootWidget::solidWhitePixel() const
{
    d->initAtlas();
    return d->texBank.texture(ID_SOLID_WHITE);
}

Id GuiRootWidget::solidRoundCorners() const
{
    d->initAtlas();
    return d->texBank.texture(ID_SOLID_ROUND_CORNERS);
}

Id GuiRootWidget::roundCorners() const
{
    d->initAtlas();
    return d->texBank.texture(ID_THIN_ROUND_CORNERS);
}

Id GuiRootWidget::boldRoundCorners() const
{
    d->initAtlas();
    return d->texBank.texture(ID_BOLD_ROUND_CORNERS);
}

Id GuiRootWidget::borderGlow() const
{
    d->initAtlas();
    return d->texBank.texture(QStringLiteral("Style.window.borderglow"));
}

Id GuiRootWidget::tinyDot() const
{
    d->initAtlas();
    return d->texBank.texture(ID_DOT);
}

Id GuiRootWidget::styleTexture(DotPath const &styleImagePath) const
{
    d->initAtlas();
    return d->texBank.texture(QStringLiteral("Style.") + styleImagePath);
}

GLShaderBank &GuiRootWidget::shaders()
{
    return BaseGuiApp::shaders();
}

Painter &GuiRootWidget::painter()
{
    return d->painter;
}

Matrix4f GuiRootWidget::projMatrix2D() const
{
    RootWidget::Size const size = viewSize();
    return Matrix4f::ortho(0, size.x, 0, size.y);
}

void GuiRootWidget::routeMouse(Widget *routeTo)
{
    setEventRouting(QList<int>({ Event::MouseButton,   Event::MouseMotion,
                                 Event::MousePosition, Event::MouseWheel }),
                    routeTo);
}

void GuiRootWidget::dispatchLatestMousePosition()
{}

bool GuiRootWidget::processEvent(Event const &event)
{
    window().glActivate();

    if (event.type() == Event::MouseButton &&
        event.as<MouseEvent>().state() != MouseEvent::Released)
    {
        d->focusIndicator->fadeOut();
    }

    bool const wasProcessed = RootWidget::processEvent(event);
    return wasProcessed;
}

void GuiRootWidget::handleEventAsFallback(Event const &)
{}

void GuiRootWidget::loadCommonTextures()
{
    d->initBankContents();
}

GuiWidget const *GuiRootWidget::globalHitTest(Vector2i const &pos) const
{
    Widget::Children const childs = children();
    for (int i = childs.size() - 1; i >= 0; --i)
    {
        if (GuiWidget const *w = childs.at(i)->maybeAs<GuiWidget>())
        {
            if (GuiWidget const *hit = w->treeHitTest(pos))
            {
                return hit;
            }
        }
    }
    return 0;
}

GuiWidget const *GuiRootWidget::guiFind(String const &name) const
{
    return find(name)->maybeAs<GuiWidget>();
}

FocusWidget &GuiRootWidget::focusIndicator()
{
    return *d->focusIndicator;
}

GuiWidget *GuiRootWidget::focus() const
{
    return static_cast<GuiWidget *>(RootWidget::focus());
}

void GuiRootWidget::pushFocus()
{
    if (!focus()) return;

    d->focusStack.append(new SafeWidgetPtr<Widget>(focus()));
}

void GuiRootWidget::popFocus()
{
    while (!d->focusStack.isEmpty())
    {
        std::unique_ptr<SafeWidgetPtr<Widget>> ptr(d->focusStack.takeLast());
        if (*ptr)
        {
            setFocus(*ptr);
            return;
        }
    }
    setFocus(nullptr);
}

void GuiRootWidget::clearFocusStack()
{
    qDeleteAll(d->focusStack);
    d->focusStack.clear();
}

void GuiRootWidget::update()
{
    if (window().isGLReady())
    {
        // Allow GL operations.
        window().glActivate();

        RootWidget::update();
        d->focusIndicator->update();
    }

    // Request a window draw so that the updated content becomes visible.
    window().as<BaseWindow>().requestDraw();
}

void GuiRootWidget::draw()
{
    d->focusIndicator->initialize();

    if (d->noFramesDrawnYet)
    {
        d->noFramesDrawnYet = false;
    }

    // Clear the framebuffer for the frame.
    window().framebuffer().clear(GLFramebuffer::ColorDepthStencil);

#ifdef DENG2_DEBUG
    // Detect mistakes in GLState stack usage.
    dsize const depthBeforeDrawing = GLState::stackDepth();
#endif

    d->painter.init();
    d->painter.setModelViewProjection(projMatrix2D());
    d->painter.setTexture(uAtlas());
    d->painter.setNormalizedScissor();

    RootWidget::draw();

    d->painter.flush();

    DENG2_ASSERT(GLState::stackDepth() == depthBeforeDrawing);
}

void GuiRootWidget::drawUntil(Widget &until)
{
    d->painter.setNormalizedScissor();

    NotifyArgs args = notifyArgsForDraw();
    args.until = &until;
    notifyTree(args);

    d->painter.flush();
}

} // namespace de
