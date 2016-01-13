/** @file databundle.h  Classic data files: PK3, WAD, LMP, DED, DEH.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DATABUNDLE_H
#define LIBDOOMSDAY_DATABUNDLE_H

#include "../libdoomsday.h"
#include "lumpdirectory.h"
#include <de/filesys/IInterpreter>
#include <de/Folder>

/**
 * Abstract base class for classic data files: PK3, WAD, LMP, DED, DEH.
 *
 * Specialized libcore Files representing loadable data files should be derived
 * from DataBundle.
 *
 * Generates Doomsday 2 compatible metadata for data files, allowing them to
 * be treated as packages at runtime.
 *
 * As packages, DataFile makes sure that the data gets loaded and unloaded
 * when games are loaded (and in the right order). The data is actually loaded
 * using the resource subsystem. To facilitate that, data file contents are
 * available as plain byte arrays.
 */
class LIBDOOMSDAY_PUBLIC DataBundle : public de::IByteArray
{
public:
    enum Format { Unknown, Pk3, Wad, Iwad, Pwad, Lump, Ded, Dehacked };

    DENG2_ERROR(FormatError);

    struct LIBDOOMSDAY_PUBLIC Interpreter : public de::filesys::IInterpreter {
        de::File *interpretFile(de::File *sourceData) const override;
    };

public:
    DataBundle(Format format, de::File &source);
    ~DataBundle();

    Format format() const;
    de::String description() const;

    /**
     * Generates appropriate packages accoding to the contents of the data bundle.
     */
    void identifyPackages() const;

    /**
     * Determines if the bundle is nested inside another bundle.
     */
    bool isNested() const;

    /**
     * Finds the bundle that contains this bundle, if this bundle is a
     * nested one.
     *
     * @return DataBundle that contains this bundle, or @c nullptr if not
     * nested. Ownership not transferred.
     */
    DataBundle const *containerBundle() const;

    /**
     * Returns the WAD file lump directory.
     * @return LumpDirectory for WADs; @c nullptr for non-WAD formats.
     */
    res::LumpDirectory const *lumpDirectory() const;

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

protected:
    void setFormat(Format format);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_DATABUNDLE_H