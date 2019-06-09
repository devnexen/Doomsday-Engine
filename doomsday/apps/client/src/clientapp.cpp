/** @file clientapp.cpp  The client application.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"

//#include <QAction>
//#include <QDebug>
//#include <QDesktopServices>
//#include <QFontDatabase>
//#include <QMenuBar>
//#include <QNetworkProxyFactory>
//#include <QSplashScreen>

#include "audio/audiosystem.h"
#include "busyrunner.h"
#include "clientapp.h"
#include "clientplayer.h"
#include "con_config.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "dd_main.h"
#include "def_main.h"
#include "gl/gl_defer.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/svg.h"
#include "network/net_demo.h"
#include "network/net_main.h"
#include "network/serverlink.h"
#include "render/r_draw.h"
#include "render/rend_main.h"
#include "render/rend_particle.h"
#include "render/rendersystem.h"
#include "sys_system.h"
#include "ui/alertmask.h"
#include "ui/b_main.h"
#include "ui/clientwindow.h"
#include "ui/clientwindowsystem.h"
#include "ui/dialogs/alertdialog.h"
#include "ui/dialogs/packagecompatibilitydialog.h"
#include "ui/inputsystem.h"
#include "ui/nativemenu.h"
#include "ui/progress.h"
#include "ui/styledlogsinkformatter.h"
#include "ui/sys_input.h"
#include "ui/viewcompositor.h"
#include "ui/widgets/taskbarwidget.h"
#include "updater.h"
#include "updater/updatedownloaddialog.h"
#include "world/contact.h"
#include "world/map.h"
#include "world/p_players.h"

#if WIN32
#  include "dd_winit.h"
#elif UNIX
#  include "dd_uinit.h"
#endif

#include <doomsday/console/exec.h>
#include <doomsday/AbstractSession>
#include <doomsday/GameStateFolder>

#include <de/legacy/timer.h>
#include <de/c_wrapper.h>
#include <de/ArrayValue>
#include <de/ByteArrayFile>
#include <de/CallbackAction>
#include <de/CommandLine>
#include <de/Config>
#include <de/DictionaryValue>
#include <de/Error>
#include <de/Extension>
#include <de/FileSystem>
#include <de/Garbage>
#include <de/Info>
#include <de/Log>
#include <de/LogSink>
#include <de/NativeFont>
#include <de/ScriptSystem>
#include <de/TextValue>
#include <de/VRConfig>

#include <SDL_events.h>
#include <SDL_surface.h>
#include <SDL_timer.h>
#include <SDL_video.h>

#include <cstdlib>

#include "ui/splash.xpm"

using namespace de;

DE_EXTENSION(importsave);

#if defined (DE_HAVE_AUDIO_FMOD)
DE_EXTENSION(fmod);
#endif
#if defined (DE_HAVE_AUDIO_FLUIDSYNTH)
DE_EXTENSION(fluidsynth);
#endif
#if defined (DE_HAVE_AUDIO_OPENAL)
DE_EXTENSION(openal);
#endif
#if defined (DE_HAVE_AUDIO_DIRECTSOUND)
DE_EXTENSION(directsound);
#endif
#if defined (DE_HAVE_AUDIO_WINMM)
DE_EXTENSION(winmm);
#endif

static ClientApp *clientAppSingleton = 0;

DE_NORETURN static void handleLegacyCoreTerminate(char const *msg)
{
    App_Error("Application terminated due to exception:\n%s\n", msg);
}

static void continueInitWithEventLoopRunning()
{
    if (!ClientWindowSystem::mainExists()) return;

#if !defined (DE_MOBILE)
    // Show the main window. This causes initialization to finish (in busy mode)
    // as the canvas is visible and ready for initialization.
    ClientWindowSystem::main().show();
#endif

#if defined (DE_HAVE_UPDATER)
    ClientApp::updater().setupUI();
#endif
}

static Value *Function_App_GamePlugin(Context &, Function::ArgumentValues const &)
{
    if (App_CurrentGame().isNull())
    {
        // The null game has no plugin.
        return 0;
    }
//            .name().fileNameWithoutExtension();
//    if (name.beginsWith("lib")) name.remove(BytePos(0), 3);
    return new TextValue(DoomsdayApp::plugins().extensionName(App_CurrentGame().pluginId()));
}

static Value *Function_App_Quit(Context &, Function::ArgumentValues const &)
{
    Sys_Quit();
    return 0;
}

DE_PIMPL(ClientApp)
, DE_OBSERVES(Plugins, PublishAPI)
, DE_OBSERVES(Plugins, Notification)
, DE_OBSERVES(App, StartupComplete)
, DE_OBSERVES(Games, Progress)
, DE_OBSERVES(DoomsdayApp, GameChange)
, DE_OBSERVES(DoomsdayApp, GameUnload)
, DE_OBSERVES(DoomsdayApp, ConsoleRegistration)
, DE_OBSERVES(DoomsdayApp, PeriodicAutosave)
{
    SDL_Window *splashWindow = nullptr;
    Binder binder;
#if defined (DE_HAVE_UPDATER)
    std::unique_ptr<Updater> updater;
#endif
#if defined (DE_HAVE_BUSYRUNNER)
    BusyRunner busyRunner;
#endif
    ConfigProfiles              audioSettings;
    ConfigProfiles              networkSettings;
    ConfigProfiles              logSettings;
    ConfigProfiles              uiSettings;
    std::unique_ptr<NativeMenu> nativeAppMenu;
    InputSystem *               inputSys  = nullptr;
    AudioSystem *               audioSys  = nullptr;
    RenderSystem *              rendSys   = nullptr;
    ClientResources *           resources = nullptr;
    ClientWindowSystem *        winSys    = nullptr;
    InFineSystem                infineSys; // instantiated at construction time
    ServerLink *                svLink = nullptr;
    ClientServerWorld *         world  = nullptr;

    /**
     * Log entry sink that passes warning messages to the main window's alert
     * notification dialog.
     */
    struct LogWarningAlarm : public LogSink
    {
        AlertMask alertMask;
        StyledLogSinkFormatter formatter;

        LogWarningAlarm()
            : LogSink(formatter)
            , formatter(LogEntry::Styled | LogEntry::OmitLevel | LogEntry::Simple)
        {
            //formatter.setOmitSectionIfNonDev(false); // always show section
            setMode(OnlyWarningEntries);
        }

        LogSink &operator<<(LogEntry const &entry)
        {
            if (alertMask.shouldRaiseAlert(entry.metadata()))
            {
                // Don't raise alerts if the console history is open; the
                // warning/error will be shown there.
                if (ClientWindow::mainExists() &&
                    ClientWindow::main().isUICreated() &&
                    ClientWindow::main().taskBar().isOpen() &&
                    ClientWindow::main().taskBar().console().isLogOpen())
                {
                    return *this;
                }

                // We don't want to raise alerts about problems in id/Raven WADs,
                // since these just have to be accepted by the user.
                if ((entry.metadata() & LogEntry::Map) && ClientApp::world().hasMap())
                {
                    const auto &map = ClientApp::world().map();
                    if (map.hasManifest() && !map.manifest().sourceFile()->hasCustom())
                    {
                        return *this;
                    }
                }

                for (const String &msg : formatter.logEntryToTextLines(entry))
                {
                    ClientApp::alert(msg, entry.level());
                }
            }
            return *this;
        }

        LogSink &operator<<(String const &plainText)
        {
            ClientApp::alert(plainText);
            return *this;
        }

        void flush() {} // not buffered
    };

    LogWarningAlarm logAlarm;

    Impl(Public *i) : Base(i)
    {
        clientAppSingleton = thisPublic;

        LogBuffer::get().addSink(logAlarm);
        DoomsdayApp::plugins().audienceForPublishAPI() += this;
        DoomsdayApp::plugins().audienceForNotification() += this;
        self().audienceForGameChange() += this;
        self().audienceForGameUnload() += this;
        self().audienceForConsoleRegistration() += this;
        self().games().audienceForProgress() += this;
        self().audienceForPeriodicAutosave() += this;
        self().audienceForStartupComplete() += this;
    }

    ~Impl() override
    {
        try
        {
            ClientWindow::glActivateMain(); // for GL deinit

            LogBuffer::get().removeSink(logAlarm);

            self().players().forAll([](Player &p) {
                p.as<ClientPlayer>().viewCompositor().glDeinit();
                return LoopContinue;
            });
            self().glDeinit();

            Sys_Shutdown();
            DD_Shutdown();
        }
        catch (Error const &er)
        {
            warning("Exception during ~ClientApp: %s", er.asText().c_str());
            DE_ASSERT_FAIL("Unclean shutdown: exception in ~ClientApp");
        }

#if defined (DE_HAVE_UPDATER)
        updater.reset();
#endif
        delete inputSys;
        delete resources;
        delete winSys;
        delete audioSys;
        delete rendSys;
        delete world;
        delete svLink;
        clientAppSingleton = 0;
    }

    void appStartupCompleted() override
    {
        // Get rid of the splash window.
        if (splashWindow)
        {
            SDL_DestroyWindow(splashWindow);
            splashWindow = nullptr;
        }
    }

    void publishAPIToPlugin(const char *plugName) override
    {
        DD_PublishAPIs(plugName);
    }

    void pluginSentNotification(int notification, void *data) override
    {
        LOG_AS("ClientApp::pluginSentNotification");

        switch (notification)
        {
        case DD_NOTIFY_GAME_SAVED:
            // If an update has been downloaded and is ready to go, we should
            // re-show the dialog now that the user has saved the game as prompted.
            LOG_DEBUG("Game saved");
#if defined (DE_HAVE_UPDATER)
            UpdateDownloadDialog::showCompletedDownload();
#endif
            break;

        case DD_NOTIFY_PSPRITE_STATE_CHANGED:
            if (data)
            {
                auto const *args = (ddnotify_psprite_state_changed_t *) data;
                self().player(args->player).weaponStateChanged(args->state);
            }
            break;

        case DD_NOTIFY_PLAYER_WEAPON_CHANGED:
            if (data)
            {
                auto const *args = (ddnotify_player_weapon_changed_t *) data;
                self().player(args->player).setWeaponAssetId(args->weaponId);
            }
            break;

        default:
            break;
        }
    }

    void gameWorkerProgress(int progress) override
    {
        Con_SetProgress(progress);
    }

    void consoleRegistration() override
    {
        DD_ConsoleRegister();
    }

    void aboutToUnloadGame(Game const &/*gameBeingUnloaded*/) override
    {
        DE_ASSERT(ClientWindow::mainExists());

        // Quit netGame if one is in progress.
        if (netGame)
        {
            Con_Execute(CMDS_DDAY, "net disconnect", true, false);
        }

        Demo_StopPlayback();
        GL_PurgeDeferredTasks();

        App_Resources().releaseAllGLTextures();
        App_Resources().pruneUnusedTextureSpecs();
        GL_LoadLightingSystemTextures();
        GL_LoadFlareTextures();
        Rend_ParticleLoadSystemTextures();
        GL_ResetViewEffects();

        infineSystem().reset();

        if (App_GameLoaded())
        {
            // Write cvars and bindings to .cfg files.
            Con_SaveDefaults();

            // Disallow further saving of bindings until another game is loaded.
            Con_SetAllowed(0);

            R_ClearViewData();
            world::R_DestroyContactLists();
            P_ClearPlayerImpulses();

            Con_Execute(CMDS_DDAY, "clearbindings", true, false);
            inputSys->bindDefaults();
            inputSys->initialContextActivations();
        }

        infineSys.deinitBindingContext();
    }

    void currentGameChanged(Game const &newGame) override
    {
        if (Sys_IsShuttingDown()) return;

        infineSys.initBindingContext();

        // Process any GL-related tasks we couldn't while Busy.
        Rend_ParticleLoadExtraTextures();

        /**
         * Clear any input events we may have accumulated during this process.
         * @note Only necessary here because we might not have been able to use
         *       busy mode (which would normally do this for us on end).
         */
        inputSys->clearEvents();

        if (newGame.isNull())
        {
            // The mouse is free while in the Home.
            ClientWindow::main().eventHandler().trapMouse(false);
        }

        ClientWindow::main().console().zeroLogHeight();

        if (!newGame.isNull())
        {
            // Auto-start the game?
            auto const *prof = self().currentGameProfile();
            if (prof && prof->autoStartMap())
            {
                LOG_NOTE("Starting in %s as configured in the game profile")
                        << prof->autoStartMap();

                Con_Executef(CMDS_DDAY, false, "setdefaultskill %i; setmap %s",
                             prof->autoStartSkill(),
                             prof->autoStartMap().c_str());
            }
        }
    }

    void periodicAutosave() override
    {
        if (Config::exists())
        {
            Config::get().writeIfModified();
        }
        Con_SaveDefaultsIfChanged();
    }

    /**
     * Set up an application-wide menu.
     */
    void setupAppMenu()
    {
        #if defined (MACOSX)
        {
            nativeAppMenu.reset(new NativeMenu);
        }
        #endif
    }

    void initSettings()
    {
        using Prof = ConfigProfiles; // convenience

        // Log filter and alert settings.
        for (int i = LogEntry::FirstDomainBit; i <= LogEntry::LastDomainBit; ++i)
        {
            String const name = LogFilter::domainRecordName(LogEntry::Context(1 << i));
            logSettings
                    .define(Prof::ConfigVariable, Stringf("log.filter.%s.minLevel", name.c_str()))
                    .define(Prof::ConfigVariable, Stringf("log.filter.%s.allowDev", name.c_str()))
                    .define(Prof::ConfigVariable, "alert." + name);
        }

        uiSettings
                .define(Prof::ConfigVariable, "ui.scaleFactor")
                .define(Prof::ConfigVariable, "ui.showAnnotations")
                .define(Prof::ConfigVariable, "home.showColumnDescription")
                .define(Prof::ConfigVariable, "home.showUnplayableGames")
                .define(Prof::ConfigVariable, "home.columns.doom")
                .define(Prof::ConfigVariable, "home.columns.heretic")
                .define(Prof::ConfigVariable, "home.columns.hexen")
                .define(Prof::ConfigVariable, "home.columns.otherGames")
                .define(Prof::ConfigVariable, "home.columns.multiplayer")
                .define(Prof::ConfigVariable, "home.sortBy")
                .define(Prof::ConfigVariable, "home.sortAscending")
                .define(Prof::ConfigVariable, "home.sortCustomSeparately");

        /// @todo These belong in their respective subsystems.

        networkSettings
                .define(Prof::ConfigVariable, "apiUrl")
                .define(Prof::ConfigVariable, "resource.localPackages")
                .define(Prof::IntCVar,        "net-dev",             NumberValue::zero);

        audioSettings
                .define(Prof::IntCVar,        "sound-volume",        NumberValue(255 * 2/3))
                .define(Prof::IntCVar,        "music-volume",        NumberValue(255 * 2/3))
                .define(Prof::FloatCVar,      "sound-reverb-volume", NumberValue(0.5f))
                .define(Prof::IntCVar,        "sound-info",          NumberValue::zero)
                //.define(Prof::IntCVar,        "sound-rate",          11025)
                //.define(Prof::IntCVar,        "sound-16bit",         0)
                .define(Prof::IntCVar,        "sound-3d",            NumberValue::zero)
                .define(Prof::IntCVar,        "sound-overlap-stop",  NumberValue::zero)
                .define(Prof::IntCVar,        "music-source",        NumberValue(AudioSystem::MUSP_EXT))
                .define(Prof::StringCVar,     "music-soundfont",     TextValue())
                .define(Prof::ConfigVariable, "audio.soundPlugin")
                .define(Prof::ConfigVariable, "audio.musicPlugin")
                .define(Prof::ConfigVariable, "audio.cdPlugin")
                .define(Prof::ConfigVariable, "audio.channels")
                .define(Prof::ConfigVariable, "audio.pauseOnFocus")
                .define(Prof::ConfigVariable, "audio.output");
    }

#if defined (UNIX)
    void printVersionToStdOut() { printf("%s %s\n", DOOMSDAY_NICENAME, DOOMSDAY_VERSION_FULLTEXT); }

    void printHelpToStdOut()
    {
        printVersionToStdOut();
        printf("Usage: %s [options]\n", self().commandLine().at(0).toLatin1().constData());
        printf(" -iwad (dir)  Set directory containing IWAD files.\n");
        printf(" -file (f)    Load one or more PWAD files at startup.\n");
        printf(" -game (id)   Set game to load at startup.\n");
        printf(" -nomaximize  Do not maximize window at startup.\n");
        printf(" -wnd         Start in windowed mode.\n");
        printf(" -wh (w) (h)  Set window width and height.\n");
        printf(" --version    Print current version.\n");
        printf("For more options and information, see \"man doomsday\".\n");
    }
#endif

    String mapClientStatePath(String const &mapId) const
    {
        return Stringf("maps/%sClientState", mapId.c_str());
    }

    String mapObjectStatePath(String const &mapId) const
    {
        return Stringf("maps/%sObjectState", mapId.c_str());
    }
};

ClientApp::ClientApp(const StringList &args)
    : BaseGuiApp(args)
    , DoomsdayApp([] () -> Player * { return new ClientPlayer; })
    , d(new Impl(this))
{
    novideo = false;

    // Use the host system's proxy configuration.
//    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Metadata.
    setMetadata("Deng Team", "dengine.net", "Doomsday Engine", DOOMSDAY_VERSION_BASE);
    setUnixHomeFolderName(".doomsday");

    // Callbacks.
    setTerminateFunc(handleLegacyCoreTerminate);

    // We must presently set the current game manually (the collection is global).
    setGame(games().nullGame());

    d->binder.init(scriptSystem()["App"])
            << DE_FUNC_NOARG (App_GamePlugin, "gamePlugin")
            << DE_FUNC_NOARG (App_Quit,       "quit");

/*#if !defined (DE_MOBILE)
    /// @todo Remove the splash screen when file system indexing can be done as
    /// a background task and the main window can be opened instantly. -jk
    QPixmap const pixmap(doomsdaySplashXpm);
    QSplashScreen *splash = new QSplashScreen(pixmap);
    splash->show();
    splash->showMessage(Version::currentBuild().asHumanReadableText(),
                        Qt::AlignHCenter | Qt::AlignBottom,
                        QColor(90, 110, 95));
    processEvents();
    splash->deleteLater();
#endif*/

    // Show the splash image in a separate window.
    {
        const Image splashImage = Image::fromXpmData(doomsdaySplashXpm);

        const int imageWidth  = int(splashImage.width());
        const int imageHeight = int(splashImage.height());

        SDL_Surface *splashSurface = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<dbyte *>(splashImage.bits()),
                                                                        imageWidth,
                                                                        imageHeight,
                                                                        int(splashImage.depth()),
                                                                        int(splashImage.stride()),
                                                                        SDL_PIXELFORMAT_ABGR8888);

        d->splashWindow =
            SDL_CreateWindow(DOOMSDAY_NICENAME,
                             SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED,
                             imageWidth,
                             imageHeight,
                             SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP);

        SDL_BlitSurface(splashSurface, nullptr, SDL_GetWindowSurface(d->splashWindow), nullptr);
        SDL_UpdateWindowSurface(d->splashWindow);
        SDL_FreeSurface(splashSurface);
        SDL_RaiseWindow(d->splashWindow);
        SDL_PumpEvents(); // allow it to appear immediately
    }
}

void ClientApp::initialize()
{
    Libdeng_Init();
    DD_InitCommandLine();

    #if defined (UNIX)
    {
        // Some common Unix command line options.
        if (commandLine().has("--version") || commandLine().has("-version"))
        {
            d->printVersionToStdOut();
            ::exit(0);
        }
        if (commandLine().has("--help") || commandLine().has("-h") || commandLine().has("-?"))
        {
            d->printHelpToStdOut();
            ::exit(0);
        }
    }
    #endif

    d->svLink = new ServerLink;

    // Initialize definitions before the files are indexed.
    Def_Init();

    addInitPackage("net.dengine.client");
    initSubsystems(); // loads Config
    DoomsdayApp::initialize();

    // Initialize players.
    for (int i = 0; i < players().count(); ++i)
    {
        player(i).viewCompositor().setPlayerNumber(i);
    }

    // Set up the log alerts (observes Config variables).
    d->logAlarm.alertMask.init();

    // Create the user's configurations and settings folder, if it doesn't exist.
    fileSystem().makeFolder("/home/configs");

    d->initSettings();

    // Initialize.
    #if defined (WIN32)
    {
        if (!DD_Win32_Init())
        {
            throw Error("ClientApp::initialize", "DD_Win32_Init failed");
        }
    }
    #elif defined (UNIX)
    {
        if (!DD_Unix_Init())
        {
            throw Error("ClientApp::initialize", "DD_Unix_Init failed");
        }
    }
    #endif

    // Create the world system.
    d->world = new ClientServerWorld;
    addSystem(*d->world);

    // Create the render system.
    d->rendSys = new RenderSystem;
    addSystem(*d->rendSys);

    // Create the audio system.
    d->audioSys = new AudioSystem;
    addSystem(*d->audioSys);

    // Create the window system.
    d->winSys = new ClientWindowSystem;
    WindowSystem::setAppWindowSystem(*d->winSys);
    addSystem(*d->winSys);

#if defined (DE_HAVE_UPDATER)
    // Check for updates automatically.
    d->updater.reset(new Updater);
#endif

    // Create the resource system.
    d->resources = new ClientResources;
    addSystem(*d->resources);

    plugins().loadAll();

    // On mobile, the window is instantiated via QML.
#if !defined (DE_MOBILE)
    d->winSys->createWindow()->setTitle(DD_ComposeMainWindowTitle());
#endif

    d->setupAppMenu();

    // Create the input system.
    d->inputSys = new InputSystem;
    addSystem(*d->inputSys);
    B_Init();

    // Finally, run the bootstrap script.
    scriptSystem().importModule("bootstrap");

    App_Timer(1, continueInitWithEventLoopRunning);
}

void ClientApp::preFrame()
{
    DGL_BeginFrame();

    // Frame synchronous I/O operations.
    App_AudioSystem().startFrame();

    if (gx.BeginFrame) /// @todo Move to GameSystem::timeChanged().
    {
        gx.BeginFrame();
    }
}

void ClientApp::postFrame()
{
    /// @todo Should these be here? Consider multiple windows, each having a postFrame?
    /// Or maybe the frames need to be synced? Or only one of them has a postFrame?

    // We will arrive here always at the same time in relation to the displayed
    // frame: it is a good time to update the mouse state.
//    Mouse_Poll();

    if (!BusyMode_Active())
    {
        if (gx.EndFrame)
        {
            gx.EndFrame();
        }
    }

    App_AudioSystem().endFrame();

    // This is a good time to recycle unneeded memory allocations, as we're just
    // finished and shown a frame and there might be free time before we have to
    // begin drawing the next frame.
    Garbage_Recycle();
}

void ClientApp::checkPackageCompatibility(const StringList &packageIds,
                                          const String &userMessageIfIncompatible,
                                          const std::function<void ()>& finalizeFunc)
{
    if (packageIds.isEmpty() || // Metadata did not specify packages.
        GameProfiles::arePackageListsCompatible(packageIds, loadedPackagesAffectingGameplay()))
    {
        finalizeFunc();
    }
    else
    {
        auto *dlg = new PackageCompatibilityDialog;
        dlg->setMessage(userMessageIfIncompatible);
        dlg->setWantedPackages(packageIds);
        dlg->setAcceptanceAction(new CallbackAction(finalizeFunc));

        if (!dlg->isCompatible())
        {
            // Run the dialog's event loop in a separate timer callback so it doesn't
            // interfere with the app's event loop.
            Loop::timer(.01, [dlg] ()
            {
                dlg->setDeleteAfterDismissed(true);
                dlg->exec(ClientWindow::main().root());
            });
        }
        else
        {
            delete dlg;
        }
    }
}

void ClientApp::gameSessionWasSaved(AbstractSession const &session,
                                    GameStateFolder &toFolder)
{
    DoomsdayApp::gameSessionWasSaved(session, toFolder);

    try
    {
        String const mapId = session.mapUri().path();

        // Internal map state.
        {
            File &file = toFolder.replaceFile(d->mapClientStatePath(mapId));
            Writer writer(file);
            world().map().serializeInternalState(writer.withHeader());
        }

        // Object state.
        {
            File &file = toFolder.replaceFile(d->mapObjectStatePath(mapId));
            file << world().map().objectsDescription().toUtf8(); // plain text
        }
    }
    catch (Error const &er)
    {
        LOGDEV_MAP_WARNING("Internal map state was not serialized: %s") << er.asText();
    }
}

void ClientApp::gameSessionWasLoaded(AbstractSession const &session,
                                     GameStateFolder const &fromFolder)
{
    DoomsdayApp::gameSessionWasLoaded(session, fromFolder);

    String const mapId = session.mapUri().path();

    // Internal map state. This might be missing.
    try
    {
        if (File const *file = fromFolder.tryLocate<File const>(d->mapClientStatePath(mapId)))
        {
            DE_ASSERT(session.thinkerMapping() != nullptr);

            Reader reader(*file);
            world().map().deserializeInternalState(reader.withHeader(),
                                                   *session.thinkerMapping());
        }
    }
    catch (Error const &er)
    {
        LOGDEV_MAP_WARNING("Internal map state not deserialized: %s") << er.asText();
    }

    // Restore object state.
    try
    {
        if (File const *file = fromFolder.tryLocate<File const>(d->mapObjectStatePath(mapId)))
        {
            // Parse the info and cross-check with current state.
            world().map().restoreObjects(Info(*file), *session.thinkerMapping());
        }
        else
        {
            LOGDEV_MSG("\"%s\" not found") << d->mapObjectStatePath(mapId);
        }
    }
    catch (Error const &er)
    {
        LOGDEV_MAP_WARNING("Object state check error: %s") << er.asText();
    }
}

ClientPlayer &ClientApp::player(int console) // static
{
    return DoomsdayApp::players().at(console).as<ClientPlayer>();
}

LoopResult ClientApp::forLocalPlayers(const std::function<LoopResult (ClientPlayer &)> &func) // static
{
    auto const &players = DoomsdayApp::players();
    for (int i = 0; i < players.count(); ++i)
    {
        ClientPlayer &player = players.at(i).as<ClientPlayer>();
        if (player.isInGame() &&
            player.publicData().flags & DDPF_LOCAL)
        {
            if (auto result = func(player))
            {
                return result;
            }
        }
    }
    return LoopContinue;
}

void ClientApp::alert(String const &msg, LogEntry::Level level)
{
    if (ClientWindow::mainExists())
    {
        auto &win = ClientWindow::main();
        if (win.isUICreated())
        {
            win.alerts()
                    .newAlert(msg, level >= LogEntry::Error?   AlertDialog::Major  :
                                   level == LogEntry::Warning? AlertDialog::Normal :
                                                               AlertDialog::Minor);
        }
    }
    /**
     * @todo If there is no window, the alert could be stored until the window becomes
     * available. -jk
     */
}

ClientApp &ClientApp::app()
{
    DE_ASSERT(clientAppSingleton != 0);
    return *clientAppSingleton;
}

#if defined (DE_HAVE_UPDATER)
Updater &ClientApp::updater()
{
    DE_ASSERT(app().d->updater);
    return *app().d->updater;
}
#endif

#if defined (DE_HAVE_BUSYRUNNER)
BusyRunner &ClientApp::busyRunner()
{
    return app().d->busyRunner;
}
#endif

ConfigProfiles &ClientApp::logSettings()
{
    return app().d->logSettings;
}

ConfigProfiles &ClientApp::networkSettings()
{
    return app().d->networkSettings;
}

ConfigProfiles &ClientApp::audioSettings()
{
    return app().d->audioSettings;
}

ConfigProfiles &ClientApp::uiSettings()
{
    return app().d->uiSettings;
}

ServerLink &ClientApp::serverLink()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(a.d->svLink != 0);
    return *a.d->svLink;
}

InFineSystem &ClientApp::infineSystem()
{
    ClientApp &a = ClientApp::app();
    //DE_ASSERT(a.d->infineSys != 0);
    return a.d->infineSys;
}

InputSystem &ClientApp::inputSystem()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(a.d->inputSys != 0);
    return *a.d->inputSys;
}

bool ClientApp::hasInputSystem()
{
    return ClientApp::app().d->inputSys != nullptr;
}

RenderSystem &ClientApp::renderSystem()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(hasRenderSystem());
    return *a.d->rendSys;
}

bool ClientApp::hasRenderSystem()
{
    return ClientApp::app().d->rendSys != nullptr;
}

AudioSystem &ClientApp::audioSystem()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(hasAudioSystem());
    return *a.d->audioSys;
}

bool ClientApp::hasAudioSystem()
{
    return ClientApp::app().d->audioSys != nullptr;
}

ClientResources &ClientApp::resources()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(a.d->resources != 0);
    return *a.d->resources;
}

ClientWindowSystem &ClientApp::windowSystem()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(a.d->winSys != 0);
    return *a.d->winSys;
}

ClientServerWorld &ClientApp::world()
{
    ClientApp &a = ClientApp::app();
    DE_ASSERT(a.d->world != 0);
    return *a.d->world;
}

void ClientApp::openHomepageInBrowser()
{
    openInBrowser(DOOMSDAY_HOMEURL);
}

void ClientApp::showLocalFile(const NativePath &path)
{
    revealFile(path);
}

void ClientApp::openInBrowser(const String &url)
{
#if !defined (DE_MOBILE)
    // Get out of fullscreen mode.
    int windowed[] = {
        ClientWindow::Fullscreen, false,
        ClientWindow::End
    };
    ClientWindow::main().changeAttributes(windowed);
#endif

#if defined (MACOSX)
    CommandLine({"/usr/bin/open", url}).execute();
#else
    DE_ASSERT_FAIL("Open a browser");
#endif
}

void ClientApp::unloadGame(const GameProfile &upcomingGame)
{
    DoomsdayApp::unloadGame(upcomingGame);

    // Game has been set to null, update window.
    ClientWindow::main().setTitle(DD_ComposeMainWindowTitle());

    if (!upcomingGame.gameId().isEmpty())
    {
        ClientWindow &mainWin = ClientWindow::main();
        mainWin.taskBar().close();

        // Trap the mouse automatically when loading a game in fullscreen.
        if (mainWin.isFullScreen())
        {
            mainWin.eventHandler().trapMouse();
        }
    }

    R_InitViewWindow();
    R_InitSvgs();

    world::Map::initDummies();
}

void ClientApp::makeGameCurrent(GameProfile const &newGame)
{
    DoomsdayApp::makeGameCurrent(newGame);

    // Game has been changed, update window.
    ClientWindow::main().setTitle(DD_ComposeMainWindowTitle());
}

void ClientApp::reset()
{
    DoomsdayApp::reset();

    Rend_ResetLookups();
    for (int i = 0; i < ClientApp::players().count(); ++i)
    {
        ClientApp::player(i).viewCompositor().glDeinit();
    }
    if (App_GameLoaded())
    {
        d->inputSys->initAllDevices();
    }
}
