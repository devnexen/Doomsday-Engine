/** @file driver.cpp  Logical audio driver (model).
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "audio/driver.h"

#include "dd_main.h"
#include "audio/drivers/dummy.h"
#ifndef DENG_DISABLE_SDLMIXER
#  include "audio/drivers/sdlmixer.h"
#endif

#include <de/Library>
#include <de/LibraryFile>
#include <de/NativeFile>

using namespace de;

namespace audio {

DENG2_PIMPL(Driver)
{
    bool initialized   = false;
    ::Library *library = nullptr;

    audiodriver_t          iBase;
    audiointerface_sfx_t   iSfx;
    audiointerface_music_t iMusic;
    audiointerface_cd_t    iCd;

    Instance(Public *i) : Base(i)
    {
        zap(iBase);
        zap(iSfx);
        zap(iMusic);
        zap(iCd);
    }

    static LibraryFile *findPlugin(String const &name)
    {
        if(!name.isEmpty())
        {
            LibraryFile *found = nullptr;
            Library_forAll([&name, &found] (LibraryFile &lib)
            {
                // Plugins are native files.
                if(lib.source()->is<NativeFile>())
                {
                    if(lib.hasUnderscoreName(name))
                    {
                        found = &lib;
                        return LoopAbort;
                    }
                }
                return LoopContinue;
            });
            return found;
        }
        return nullptr;
    }

    void getDummyInterfaces()
    {
        DENG2_ASSERT(!initialized);

        library = nullptr;
        std::memcpy(&iBase, &::audiod_dummy,     sizeof(iBase));
        std::memcpy(&iSfx,  &::audiod_dummy_sfx, sizeof(iSfx));
        zap(iMusic);
        zap(iCd);
    }

#ifndef DENG_DISABLE_SDLMIXER
    void getSdlMixerInterfaces()
    {
        DENG2_ASSERT(!initialized);

        library = nullptr;
        std::memcpy(&iBase,  &::audiod_sdlmixer,       sizeof(iBase));
        std::memcpy(&iSfx,   &::audiod_sdlmixer_sfx,   sizeof(iSfx));
        std::memcpy(&iMusic, &::audiod_sdlmixer_music, sizeof(iMusic));
        zap(iCd);
    }
#endif

    void importInterfaces(LibraryFile &libFile)
    {
        DENG2_ASSERT(!initialized);

        library = Library_New(libFile.path().toUtf8().constData());
        zap(iBase);
        zap(iSfx);
        zap(iMusic);
        zap(iCd);

        if(!library) return;
        de::Library &lib = libFile.library();

        lib.setSymbolPtr( iBase.Init,        "DS_Init");
        lib.setSymbolPtr( iBase.Shutdown,    "DS_Shutdown");
        lib.setSymbolPtr( iBase.Event,       "DS_Event");
        lib.setSymbolPtr( iBase.Set,         "DS_Set", de::Library::OptionalSymbol);

        if(lib.hasSymbol("DS_SFX_Init"))
        {
            lib.setSymbolPtr( iSfx.gen.Init,      "DS_SFX_Init");
            lib.setSymbolPtr( iSfx.gen.Create,    "DS_SFX_CreateBuffer");
            lib.setSymbolPtr( iSfx.gen.Destroy,   "DS_SFX_DestroyBuffer");
            lib.setSymbolPtr( iSfx.gen.Load,      "DS_SFX_Load");
            lib.setSymbolPtr( iSfx.gen.Reset,     "DS_SFX_Reset");
            lib.setSymbolPtr( iSfx.gen.Play,      "DS_SFX_Play");
            lib.setSymbolPtr( iSfx.gen.Stop,      "DS_SFX_Stop");
            lib.setSymbolPtr( iSfx.gen.Refresh,   "DS_SFX_Refresh");
            lib.setSymbolPtr( iSfx.gen.Set,       "DS_SFX_Set");
            lib.setSymbolPtr( iSfx.gen.Setv,      "DS_SFX_Setv");
            lib.setSymbolPtr( iSfx.gen.Listener,  "DS_SFX_Listener");
            lib.setSymbolPtr( iSfx.gen.Listenerv, "DS_SFX_Listenerv");
            lib.setSymbolPtr( iSfx.gen.Getv,      "DS_SFX_Getv", de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_Music_Init"))
        {
            lib.setSymbolPtr( iMusic.gen.Init,    "DM_Music_Init");
            lib.setSymbolPtr( iMusic.gen.Update,  "DM_Music_Update");
            lib.setSymbolPtr( iMusic.gen.Get,     "DM_Music_Get");
            lib.setSymbolPtr( iMusic.gen.Set,     "DM_Music_Set");
            lib.setSymbolPtr( iMusic.gen.Pause,   "DM_Music_Pause");
            lib.setSymbolPtr( iMusic.gen.Stop,    "DM_Music_Stop");
            lib.setSymbolPtr( iMusic.SongBuffer,  "DM_Music_SongBuffer", de::Library::OptionalSymbol);
            lib.setSymbolPtr( iMusic.Play,        "DM_Music_Play",       de::Library::OptionalSymbol);
            lib.setSymbolPtr( iMusic.PlayFile,    "DM_Music_PlayFile",   de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_CDAudio_Init"))
        {
            lib.setSymbolPtr( iCd.gen.Init,       "DM_CDAudio_Init");
            lib.setSymbolPtr( iCd.gen.Update,     "DM_CDAudio_Update");
            lib.setSymbolPtr( iCd.gen.Set,        "DM_CDAudio_Set");
            lib.setSymbolPtr( iCd.gen.Get,        "DM_CDAudio_Get");
            lib.setSymbolPtr( iCd.gen.Pause,      "DM_CDAudio_Pause");
            lib.setSymbolPtr( iCd.gen.Stop,       "DM_CDAudio_Stop");
            lib.setSymbolPtr( iCd.Play,           "DM_CDAudio_Play");
        }
    }
};

Driver::Driver() : d(new Instance(this))
{}

String Driver::name() const
{
    if(!isLoaded()) return "(invalid)";
    return Driver_GetName(App_AudioSystem().toDriverId(this));
}

Driver::Status Driver::status() const
{
    if(d->initialized) return Initialized;
    if(d->iBase.Init != nullptr) return Loaded;
    return Invalid;
}

String Driver::statusAsText() const
{
    switch(status())
    {
    case Invalid:     return "Invalid";
    case Loaded:      return "Loaded";
    case Initialized: return "Initialized";

    default:
        DENG2_ASSERT(!"audio::Driver::statusAsText: Unknown status");
        return "Unknown";
    }
}

void Driver::load(String const &identifier)
{
    LOG_AS("audio::Driver");

    if(isLoaded())
    {
        /// @throw LoadError  Attempted to load on top of an already loaded driver.
        throw LoadError("audio::Driver::load", "Already initialized. Cannot load '" + identifier + "'");
    }

    // Perhaps a built-in audio driver?
    if(!identifier.compareWithoutCase("dummy"))
    {
        d->getDummyInterfaces();
        return;
    }
#ifndef DENG_DISABLE_SDLMIXER
    if(!identifier.compareWithoutCase("sdlmixer"))
    {
        d->getSdlMixerInterfaces();
        return;
    }
#endif

    // Perhaps a plugin audio driver?
    if(LibraryFile *plugin = Instance::findPlugin(identifier))
    {
        d->importInterfaces(*plugin);
        return;
    }

    /// @throw LoadError  Unknown driver specified.
    throw LoadError("audio::Driver::load", "Unknown driver \"" + identifier + "\"");
}

void Driver::unload()
{
    LOG_AS("audio::Driver");

    if(isInitialized())
    {
        /// @throw LoadError  Cannot unload while initialized.
        throw LoadError("audio::Driver::unload", "\"" + name() + "\" is still initialized, cannot unload");
    }

    if(isLoaded())
    {
        Library_Delete(d->library); d->library = nullptr;
        zap(d->iBase);
        zap(d->iSfx);
        zap(d->iMusic);
        zap(d->iCd);
    }
}

void Driver::initialize()
{
    LOG_AS("audio::Driver");

    // Already been here?
    if(d->initialized) return;

    DENG2_ASSERT(d->iBase.Init != nullptr);
    d->initialized = d->iBase.Init();
}

void Driver::deinitialize()
{
    LOG_AS("audio::Driver");

    // Already been here?
    if(!d->initialized) return;

    if(d->iBase.Shutdown)
    {
        d->iBase.Shutdown();
    }
    d->initialized = false;
}

::Library *Driver::library() const
{
    return d->library;
}

audiodriver_t /*const*/ &Driver::iBase() const
{
    return d->iBase;
}

bool Driver::hasSfx() const
{
    return iSfx().gen.Init != nullptr;
}

bool Driver::hasMusic() const
{
    return iMusic().gen.Init != nullptr;
}

bool Driver::hasCd() const
{
    return iCd().gen.Init != nullptr;
}

audiointerface_sfx_t /*const*/ &Driver::iSfx() const
{
    return d->iSfx;
}

audiointerface_music_t /*const*/ &Driver::iMusic() const
{
    return d->iMusic;
}

audiointerface_cd_t /*const*/ &Driver::iCd() const
{
    return d->iCd;
}

String Driver::interfaceName(void *anyAudioInterface) const
{
    if((void *)&d->iSfx == anyAudioInterface)
    {
        /// @todo  SFX interfaces can't be named yet.
        return name();
    }

    if((void *)&d->iMusic == anyAudioInterface || (void *)&d->iCd == anyAudioInterface)
    {
        char buf[256];  /// @todo  This could easily overflow...
        auto *gen = (audiointerface_music_generic_t *) anyAudioInterface;
        if(gen->Get(MUSIP_ID, buf))
        {
            return buf;
        }
        else
        {
            return "[MUSIP_ID not defined]";
        }
    }

    return "";  // Not recognized.
}

String Driver_GetName(audiodriverid_t id)
{
    static String const driverNames[AUDIODRIVER_COUNT] = {
        /* AUDIOD_DUMMY */      "Dummy",
        /* AUDIOD_SDL_MIXER */  "SDLMixer",
        /* AUDIOD_OPENAL */     "OpenAL",
        /* AUDIOD_FMOD */       "FMOD",
        /* AUDIOD_FLUIDSYNTH */ "FluidSynth",
        /* AUDIOD_DSOUND */     "DirectSound",        // Win32 only
        /* AUDIOD_WINMM */      "Windows Multimedia"  // Win32 only
    };
    if(VALID_AUDIODRIVER_IDENTIFIER(id))
        return driverNames[id];

    DENG2_ASSERT(!"audio::Driver_GetName: Unknown driver id");
    return "";
}

}  // namespace audio