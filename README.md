# The Doomsday Engine Project Repository

This is the source code for Doomsday Engine: a portable, enhanced source port of id Software's Doom I/II and Raven Software's Heretic and Hexen. The sources are under the GNU General Public license (see doomsday/engine/doc/LICENSE).

For compilation instructions and other details, see the documentation wiki: http://dengine.net/dew/

## Libraries

**libdeng2** is the core of Doomsday 2. It is a C++ class framework containing functionality such as the file system, plugin loading, Doomsday Script, network communications, and generic data structures. Almost everything relies or will rely on this core library.

**libdeng1** is a collection of C language routines extracted from the old Doomsday 1 code base. Its purpose is to (eventually) act as a C wrapper for libdeng2. (Game plugins are mostly in C.)

**libgui** builds on libdeng2 to add low-level GUI capabilities such as OpenGL graphics, fonts, images, and input devices.

**libappfw** contains the Doomsday UI framework: widgets, generic dialogs, abstract data models. libappfw is built on libgui and libdeng2.

**libshell** has functionality related to connecting to and controlling Doomsday servers remotely.

## External Dependencies

### Qt

The minimum required version of Qt is 4.7. See [Supported platforms](http://dengine.net/dew/index.php?title=Supported_platforms) in the wiki for details about which version is being used on which platform.

### Open Asset Import Library

libgui requires the [Open Asset Import Library](http://assimp.sourceforge.net/lib_html/index.html) to read 3D model and animation files.

1. Clone https://github.com/skyjake/assimp.
2. Check out the "deng-patches" branch.
3. Run [cmake](http://cmake.org) to generate appropriate project files (e.g., Visual Studio on Windows).
4. Compile the generated project.
5. Add `ASSIMP_DIR` to your *config_user.pri*.

### FMOD Ex

The optional FMOD audio plugin requires the [FMOD Ex Programmer's API](http://fmod.org/).

## Branches

The following branches are currently active in the repository.

- **master**: Main code base. This is where releases are made from on a biweekly basis. Bug fixing is done in this branch, while larger development efforts occur in separate work branches.
- **stable**: Latest stable release. Patch releases can be made from this branch when necessary.
- **stable-x.y.z**: Stable release x.y.z.
- **legacy**: Old stable code base. Currently at the 1.8.6 release.
