/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/*
 * con_busy.c: Console Busy Mode
 *
 * Draws the screen while the main engine thread is working a long 
 * operation. The busy mode can be configured to be displaying a progress 
 * bar, the console output, or a more generic "please wait" message.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     Con_BusyLoop(void);
static void     Con_BusyDrawer(void);
static void     Con_BusyLoadTextures(void);
static void     Con_BusyDeleteTextures(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/*
boolean         startupScreen = false;
int             startupLogo;
*/

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean  busyInited;
static int      busyMode;
static thread_t busyThread;
static timespan_t busyTime;
static volatile boolean busyDone;
static volatile const char* busyError = NULL;
static float    busyProgress = 0;

static DGLuint texLoading[2];

static char *titleText = "";
static char secondaryTitleText[256];
static char statusText[256];
static int fontHgt = 8;         // Height of the font.

// CODE --------------------------------------------------------------------

/**
 * Busy mode.
 *
 * @param flags  Busy mode flags (see BUSYF_PROGRESS_BAR and others).
 * @param worker  Worker thread that does processing while in busy mode.
 * @param workerData  Data context for the worker thread.
 *
 * @return  Return value of the worker.
 */
int Con_Busy(int flags, busyworkerfunc_t worker, void *workerData)
{
    int result = 0;
    
    if(busyInited)
    {
        Con_Error("Con_Busy: Already busy.\n");
    }

    busyMode = flags;
    busyInited = true;
    busyDone = false;

    // Load any textures needed in this mode.
    Con_BusyLoadTextures();

    // Start the busy worker thread, which will proces things in the 
    // background while we keep the user occupied with nice animations.
    busyThread = Sys_StartThread(worker, workerData);
    
    // Wait for the busy thread to stop.
    Con_BusyLoop();

    // Free resources.
    Con_BusyDeleteTextures();

    if(busyError)
    {
        Con_AbnormalShutdown((const char*) busyError);
    }
    
    // Make sure the worker finishes before we continue.
    result = Sys_WaitThread(busyThread);   
    busyThread = NULL;
    busyInited = false;
    
    // Make sure that any remaining deferred content gets uploaded.
    if(!(busyMode & BUSYF_NO_UPLOADS))
    {
        GL_UploadDeferredContent(0);
    }
    
    return result;
}

/**
 * Called by the busy worker to shutdown the engine immediately.
 * 
 * @param message  Message, expected to exist until the engine closes.
 */
void Con_BusyWorkerError(const char* message)
{
    busyError = message;
    Con_BusyWorkerEnd();
}

/**
 * Called by the busy worker thread when it has finished processing, 
 * to end busy mode.
 */
void Con_BusyWorkerEnd(void)
{
    if(!busyInited)
        return;
    
    busyDone = true;
}

boolean Con_IsBusy(void)
{
    return busyInited;
}

static void Con_BusyLoadTextures(void)
{
    image_t image;

    // Need to load the progress indicator?
    if(busyMode & (BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR))
    {
        // These must be real files in the base dir because virtual files haven't
        // been loaded yet when engine startup is done.
        if(GL_LoadImage(&image, "}data/graphics/loading1.png", false))
        {
            texLoading[0] = GL_NewTextureWithParams(DGL_RGBA, image.width, image.height, 
                                                    image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }

        if(GL_LoadImage(&image, "}data/graphics/loading2.png", false))
        {
            texLoading[1] = GL_NewTextureWithParams(DGL_RGBA, image.width, image.height, 
                                                    image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }        
    }    
}

static void Con_BusyDeleteTextures(void)
{
    gl.DeleteTextures(2, texLoading);
    texLoading[0] = texLoading[1] = 0;
}

/**
 * The busy thread main function. Runs until busyDone set to true.
 */
static void Con_BusyLoop(void)
{
    timespan_t startTime = Sys_GetRealSeconds();

    // TODO: Grab a copy of the current frame buffer contents.
    
    // Need any textures?
    Con_BusyLoadTextures();
    
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);
    
    while(!busyDone || (!(busyMode & BUSYF_NO_UPLOADS) && GL_GetDeferredCount() > 0))
    {
        Sys_Sleep(20);
        
        // Make sure that any deferred content gets uploaded.
        if(!(busyMode & BUSYF_NO_UPLOADS))
        {
            GL_UploadDeferredContent(15);
        }
        
        // Update the time.
        busyTime = Sys_GetRealSeconds() - startTime;
        
        // Time for an update?
        Con_BusyDrawer();
    }

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();    
    
    if(verbose)
    {
        Con_Message("Con_Busy: Was busy for %.2lf seconds.\n", busyTime);
    }
}

/**
 * @param 0...1, to indicate how far things have progressed.
 */
static void Con_BusyDrawIndicator(float pos)
{
    int x = glScreenWidth/2;
    int y = glScreenHeight/2;
    int radius = glScreenHeight/16;
    float col[4] = {
         .333f, .333f, .333f, 1
    };
    int i = 0;
    int edgeCount = 0;
    
    pos = MINMAX_OF(0, pos, 1);
    edgeCount = MAX_OF(1, pos * 30);
    
    // Draw the frame.
    gl.Enable(DGL_TEXTURING);
    gl.Enable(DGL_BLENDING);
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

    gl.Bind(texLoading[0]);
    GL_DrawRect(x - radius, y - radius, radius*2, radius*2, col[0], col[1], col[2], col[3]);
    
    // Rotate around center.
    gl.MatrixMode(DGL_TEXTURE);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Translatef(.5f, .5f, 0.f);
    gl.Rotatef(-busyTime * 20, 0.f, 0.f, 1.f);
    gl.Translatef(-.5f, -.5f, 0.f);

    // Draw a fan.
    gl.Bind(texLoading[1]);
    gl.Begin(DGL_TRIANGLE_FAN);
    // Center.
    gl.TexCoord2f(.5f, .5f);
    gl.Vertex2f(x, y);
    // Vertices along the edge.
    for(i = 0; i <= edgeCount; ++i)
    {
        float angle = 2 * PI * pos * (i / (float)edgeCount) + PI/2;
        gl.TexCoord2f(.5f + cos(angle)*.5f, .5f + sin(angle)*.5f);
        gl.Vertex2f(x + cos(angle)*radius, y + sin(angle)*radius);
    }
    gl.End();
    
    gl.MatrixMode(DGL_TEXTURE);
    gl.PopMatrix();
}

/**
 * Busy drawer function. The entire frame is drawn here.
 */
static void Con_BusyDrawer(void)
{
    DGLuint oldBinding = 0;
    char buf[100];
    
    gl.Clear(DGL_COLOR_BUFFER_BIT);

    // Indefinite activity?
    if(busyMode & BUSYF_ACTIVITY)
    {
        Con_BusyDrawIndicator(1);
    }
    else if(busyMode & BUSYF_PROGRESS_BAR)
    {
        // The progress is animated elsewhere.
        Con_BusyDrawIndicator(Con_GetProgress());
    }
    
    sprintf(buf, "Busy %04x : %.2lf", busyMode, busyTime);
    gl.Color4f(1, 1, 1, 1);
    FR_TextOut(buf, 20, 20);
    
    // TODO: When accessing console output, lock the console buffer first!
    
    // Swap buffers.
    gl.Show();
}

#if 0
/*
 * The startup screen mode is used during engine startup.  In startup
 * mode, the whole screen is used for console output.
 */
void Con_StartupInit(void)
{
    static boolean firstTime = true;

    if(novideo)
        return;

    GL_InitVarFont();
    fontHgt = FR_SingleLineHeight("Doomsday!");

    startupScreen = true;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);

    if(firstTime)
    {
        titleText = "Doomsday " DOOMSDAY_VERSION_TEXT " Startup";
        firstTime = false;
    }
    else
    {
        titleText = "Doomsday " DOOMSDAY_VERSION_TEXT;
    }

    // Load graphics.
    startupLogo = GL_LoadGraphics("Background", LGM_GRAYSCALE);
}

void Con_StartupDone(void)
{
    if(isDedicated)
        return;
    titleText = "Doomsday " DOOMSDAY_VERSION_TEXT;
    startupScreen = false;
    gl.DeleteTextures(1, (DGLuint*) &startupLogo);
    startupLogo = 0;
    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
    GL_ShutdownVarFont();

    // Update the secondary title and the game status.
    strncpy(secondaryTitleText, (char *) gx.GetVariable(DD_GAME_ID),
            sizeof(secondaryTitleText) - 1);
    strncpy(statusText, (char *) gx.GetVariable(DD_GAME_MODE),
            sizeof(statusText) - 1);
}

/*
 * Background with the "The Doomsday Engine" text superimposed.
 *
 * @param alpha  Alpha level to use when drawing the background.
 */
void Con_DrawStartupBackground(float alpha)
{
    float   mul = (startupLogo ? 1.5f : 1.0f);
    ui_color_t *dark = UI_COL(UIC_BG_DARK), *light = UI_COL(UIC_BG_LIGHT);

    // Background gradient picture.
    gl.Bind(startupLogo);
    if(alpha < 1.0)
    {
        gl.Enable(DGL_BLENDING);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        gl.Disable(DGL_BLENDING);
    }
    gl.Begin(DGL_QUADS);
    // Top color.
    gl.Color4f(dark->red * mul, dark->green * mul, dark->blue * mul, alpha);
    gl.TexCoord2f(0, 0);
    gl.Vertex2f(0, 0);
    gl.TexCoord2f(1, 0);
    gl.Vertex2f(glScreenWidth, 0);
    // Bottom color.
    gl.Color4f(light->red * mul, light->green * mul, light->blue * mul, alpha);
    gl.TexCoord2f(1, 1);
    gl.Vertex2f(glScreenWidth, glScreenHeight);
    gl.TexCoord2f(0, 1);
    gl.Vertex2f(0, glScreenHeight);
    gl.End();
    gl.Enable(DGL_BLENDING);
}

/*
 * Draws the title bar of the console.
 *
 * @return  Title bar height.
 */
int Con_DrawTitle(float alpha)
{
    int width = 0;
    int height = UI_TITLE_HGT;

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.LoadIdentity();

    FR_SetFont(glFontVariable[GLFS_BOLD]);
    height = FR_TextHeight("W") + UI_BORDER;
    UI_DrawTitleEx(titleText, height, alpha);
    if(secondaryTitleText[0])
    {
        width = FR_TextWidth(titleText) + FR_TextWidth("  ");
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(secondaryTitleText, UI_BORDER + width, height / 2,
                     false, true, UI_COL(UIC_TEXT), .75f * alpha);
    }
    if(statusText[0])
    {
        width = FR_TextWidth(statusText);
        FR_SetFont(glFontVariable[GLFS_LIGHT]);
        UI_TextOutEx(statusText, glScreenWidth - UI_BORDER - width, height / 2,
                     false, true, UI_COL(UIC_TEXT), .75f * alpha);
    }

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();

    FR_SetFont(glFontFixed);
    return height;
}

/*
 * Draw the background and the current console output.
 */
void Con_DrawStartupScreen(int show)
{
    int         vislines, y, x;
    int         topy;
    uint        i, count;
    cbuffer_t  *buffer;
    const cbline_t   **lines, *line;

    // Print the messages in the console.
    if(!startupScreen || ui_active)
        return;

    //gl.MatrixMode(DGL_PROJECTION);
    //gl.LoadIdentity();
    //gl.Ortho(0, 0, glScreenWidth, glScreenHeight, -1, 1);

    Con_DrawStartupBackground(1.0);

    // Draw the title.
    topy = Con_DrawTitle(1.0);

    topy += UI_BORDER;
    vislines = (glScreenHeight - topy + fontHgt / 2) / fontHgt;
    y = topy;

    buffer = Con_GetConsoleBuffer();
    if(vislines > 0)
    {
        lines = Con_BufferGetLines(buffer, vislines, -vislines, &count);
        if(lines)
        {
            for(i = 0; i < count - 1; ++i)
            {
                line = lines[i];

                if(!line)
                    break;
                if(line->flags & CBLF_RULER)
                {
                    Con_DrawRuler(y, fontHgt, 1);
                }
                else
                {
                    if(!line->text)
                        continue;

                    x = (line->flags & CBLF_CENTER ?
                            (glScreenWidth - FR_TextWidth(line->text)) / 2 : 3);
                    //gl.Color3f(0, 0, 0);
                    //FR_TextOut(line->text, x + 1, y + 1);
                    gl.Color3f(1, 1, 1);
                    FR_CustomShadowTextOut(line->text, x, y, 1, 1, 1);
                }
                y += fontHgt;
            }
            Z_Free((cbline_t **) lines);
        }
    }
    if(show)
    {
        // Update the progress bar, if one is active.
        //Con_Progress(0, PBARF_NOBACKGROUND | PBARF_NOBLIT);
        gl.Show();
    }
}
#endif
