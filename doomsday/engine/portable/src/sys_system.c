/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  include <windows.h>
#  include <process.h>
#endif

#include <signal.h>
#include <SDL.h>
#include <SDL_thread.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#ifdef WIN32
extern HWND hWndMain;
extern HINSTANCE hInstApp;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//int       systics = 0;    // System tics (every game tic).
boolean novideo;                // if true, stay in text mode for debugging

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#ifdef WIN32
/**
 * Borrowed from Lee Killough.
 */
static void C_DECL handler(int s)
{
    signal(s, SIG_IGN);  // Ignore future instances of this signal.

    Con_Error(s==SIGSEGV ? "Segmentation Violation\n" :
        s==SIGINT  ? "Interrupted by User\n" :
        s==SIGILL  ? "Illegal Instruction\n" :
        s==SIGFPE  ? "Floating Point Exception\n" :
        s==SIGTERM ? "Killed\n" : "Terminated by signal\n");
}
#endif

/**
 * Initialize machine state.
 */
void Sys_Init(void)
{
#ifdef WIN32
    // Initialize COM.
    CoInitialize(NULL);
#endif

    Con_Message("Sys_Init: Initializing keyboard, mouse and joystick.\n");
    if(!isDedicated)
    {
        if(!I_Init())
            Con_Error("Sys_Init: failed to initialize DirectInput.\n");

        I_InitInputDevices();
    }
    Sys_InitTimer();
    Sys_InitMixer();
    S_Init();
    Huff_Init();
    N_Init();

#if defined(WIN32) && !defined(_DEBUG)
    // Register handler for abnormal situations (in release build).
    signal(SIGSEGV, handler);
    signal(SIGTERM, handler);
    signal(SIGILL, handler);
    signal(SIGFPE, handler);
    signal(SIGILL, handler);
    signal(SIGABRT, handler);
#endif

#ifndef WIN32
    // We are not worried about broken pipes. When a TCP connection closes,
    // we prefer to receive an error code instead of a signal.
    signal(SIGPIPE, SIG_IGN);
#endif
}

/**
 * Return to default system state.
 */
void Sys_Shutdown(void)
{
    Sys_ShutdownTimer();

    if(gx.Shutdown)
        gx.Shutdown();

    Net_Shutdown();
    Huff_Shutdown();
    // Let's shut down sound first, so Windows' HD-hogging doesn't jam
    // the MUS player (would produce horrible bursts of notes).
    S_Shutdown();
    Sys_ShutdownMixer();
    GL_Shutdown();
    I_ShutdownInputDevices();
    I_Shutdown();

#ifdef WIN32
    CoUninitialize();
#endif
}

int Sys_CriticalMessage(char *msg)
{
#ifdef WIN32
    char    buf[256];
    int     ret;

    ShowCursor(TRUE);
    ShowCursor(TRUE);
    GetWindowText(hWndMain, buf, 255);
    ret =
        (MessageBox(hWndMain, msg, buf, MB_YESNO | MB_ICONEXCLAMATION) ==
         IDYES);
    ShowCursor(FALSE);
    ShowCursor(FALSE);
    return ret;
#else
    fprintf(stderr, "--- %s\n", msg);
    return 0;
#endif
}

void Sys_Sleep(int millisecs)
{
#ifdef WIN32
    Sleep(millisecs);
#endif
#ifdef UNIX
    // Not guaranteed to be very accurate...
    SDL_Delay(millisecs);
#endif
}

void Sys_ShowCursor(boolean show)
{
#ifdef WIN32
    ShowCursor(show);
#endif
#ifdef UNIX
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
#endif
}

void Sys_HideMouse(void)
{
    //  if(!I_MousePresent()) return;

#ifdef WIN32
    if(novideo || nofullscreen)
        return;
    ShowCursor(FALSE);
    ShowCursor(FALSE);
#endif
#ifdef UNIX
    //if(novideo) return;
    Sys_ShowCursor(false);
#endif
}

void Sys_ShowWindow(boolean show)
{
    // Showing does not work in dedicated mode.
    if(isDedicated && show)
        return;

#ifdef WIN32
    SetWindowPos(hWndMain, HWND_TOP, 0, 0, 0, 0,
                 (show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOSIZE |
                 SWP_NOMOVE);
    if(show)
        SetActiveWindow(hWndMain);
#endif
}

/**
 * Shut everything down and quit the program.
 */
void Sys_Quit(void)
{
    // Quit netgame if one is in progress.
    if(netgame)
    {
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect",
                    true, false);
    }

    Demo_StopPlayback();
    Con_SaveDefaults();
    Sys_Shutdown();
    B_Shutdown();
    Con_Shutdown();
    DD_Shutdown();

    // Stop the execution of the program.
    exit(0);
}

void Sys_MessageBox(const char *msg, boolean iserror)
{
#ifdef WIN32
    char    title[300];

    GetWindowText(hWndMain, title, 300);
    MessageBox(hWndMain, msg, title,
               MB_OK | (iserror ? MB_ICONERROR : MB_ICONINFORMATION));
#endif
#ifdef UNIX
    fprintf(stderr, "%s %s\n", iserror ? "**ERROR**" : "---", msg);
#endif
}

/**
 * Opens the given file in a suitable text editor.
 */
void Sys_OpenTextEditor(const char *filename)
{
#ifdef WIN32
    // Everybody is bound to have Notepad.
    spawnlp(P_NOWAIT, "notepad.exe", "notepad.exe", filename, 0);
#endif
}

/**
 * Utilises SDL Threads on ALL systems.
 * Returns the SDL_thread structure as handle to the started thread.
 */
thread_t Sys_StartThread(systhreadfunc_t startpos, void *parm)
{
    SDL_Thread* thread = SDL_CreateThread(startpos, parm);

    if(thread == NULL)
    {
        Con_Message("Sys_StartThread: Failed to start new thread (%s).\n",
                    SDL_GetError());
        return 0;
    }

    return thread;
}

/**
 * Suspends or resumes the execution of a thread.
 */
void Sys_SuspendThread(thread_t handle, boolean dopause)
{
    Con_Error("Sys_SuspendThread: Not implemented.\n");
}

/**
 * Returns the return value of the thread.
 */
int Sys_WaitThread(thread_t thread)
{
    int result = 0;
    SDL_WaitThread(thread, &result);
    return result;
}

/**
 * Returns the identifier of the current thread.
 */
uint Sys_ThreadID(void)
{
    return SDL_ThreadID();
}

mutex_t Sys_CreateMutex(const char *name)
{
    return (mutex_t) SDL_CreateMutex();
}

void Sys_DestroyMutex(mutex_t handle)
{
    if(!handle)
        return;
    SDL_DestroyMutex((SDL_mutex *) handle);
}

void Sys_Lock(mutex_t handle)
{
    if(!handle)
        return;
    SDL_mutexP((SDL_mutex *) handle);
}

void Sys_Unlock(mutex_t handle)
{
    if(!handle)
        return;
    SDL_mutexV((SDL_mutex *) handle);
}

/**
 * Create a new semaphore. Returns a handle.
 */
long Sem_Create(uint32_t initialValue)
{
    return (long) SDL_CreateSemaphore(initialValue);
}

void Sem_Destroy(long semaphore)
{
    if(semaphore)
    {
        SDL_DestroySemaphore((SDL_sem *) semaphore);
    }
}

/**
 * "Proberen" a semaphore. Blocks until the successful.
 */
void Sem_P(long semaphore)
{
    if(semaphore)
    {
        SDL_SemWait((SDL_sem *) semaphore);
    }
}

/**
 * "Verhogen" a semaphore. Returns immediately.
 */
void Sem_V(long semaphore)
{
    if(semaphore)
    {
        SDL_SemPost((SDL_sem *) semaphore);
    }
}
