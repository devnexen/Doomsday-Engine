/** @file id1map.h  id Tech 1 map format reader.
 *
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef WADMAPCONVERTER_ID1MAP_H
#define WADMAPCONVERTER_ID1MAP_H

#include "doomsday.h"
#include "dd_types.h"
#include "maplumpinfo.h"
#include "id1map_datatypes.h"
#include <de/Error>
#include <de/String>
#include <vector>
#include <list>
#include <map>

/**
 * Logical map format identifier (unique).
 */
enum MapFormatId {
    MF_UNKNOWN              = -1,
    MF_DOOM                 = 0,
    MF_HEXEN,
    MF_DOOM64,
    NUM_MAPFORMATS
};

/**
 * Helper macro for determining whether a value can be interpreted as a logical
 * map format identifier (see mapformatid_t).
 */
#define VALID_MAPFORMATID(v)        ((v) >= MF_DOOM && (v) < NUM_MAPFORMATS)

/// Material dictionary groups.
enum MaterialDictGroup {
    MG_PLANE = 0,
    MG_WALL
};

typedef std::map<MapLumpType, MapLumpInfo *> MapLumpInfos;

/**
 *
 */
class Id1Map
{
public:
    /// There was a problem opening the lump data buffer. @ingroup errors
    DENG2_ERROR(LumpBufferError);

    typedef std::vector<mline_t> Lines;
    typedef std::vector<mside_t> Sides;
    typedef std::vector<msector_t> Sectors;
    typedef std::vector<mthing_t> Things;
    typedef std::vector<surfacetint_t> SurfaceTints;
    typedef std::list<mpolyobj_t> Polyobjs;

    typedef std::list<uint> LineList;

public:
    Id1Map(MapFormatId format);
    ~Id1Map();

    MapFormatId format() const { return mapFormat; }

    void load(MapLumpInfos &lumpInfos);

    void analyze();

    int transfer();

    MaterialDictId addMaterialToDictionary(char const *name, MaterialDictGroup group);

private:
    inline de::String const &findMaterialInDictionary(MaterialDictId id) {
        return materials.stringRef(id);
    }

    /// @todo fixme: A real performance killer...
    AutoStr *composeMaterialRef(MaterialDictId id);

    bool loadVertexes(Reader *reader, int numElements);
    bool loadLineDefs(Reader *reader, int numElements);
    bool loadSideDefs(Reader *reader, int numElements);
    bool loadSectors(Reader *reader, int numElements);
    bool loadThings(Reader *reader, int numElements);
    bool loadSurfaceTints(Reader *reader, int numElements);

    /**
     * Create a temporary polyobj.
     */
    mpolyobj_t *createPolyobj(LineList &lineList, int tag, int sequenceType,
                              int16_t anchorX, int16_t anchorY);
    /**
     * @param lineList      @c NULL, will cause IterFindPolyLines to count
     *                      the number of lines in the polyobj.
     */
    void collectPolyobjLinesWorker(LineList &lineList, coord_t x, coord_t y);

    void collectPolyobjLines(LineList &lineList, Lines::iterator lineIt);

    /**
     * Find all linedefs marked as belonging to a polyobject with the given tag
     * and attempt to create a polyobject from them.
     *
     * @param tag  Line tag of linedefs to search for.
     *
     * @return  @c true = successfully created polyobj.
     */
    bool findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY);

    void findPolyobjs();

    void transferVertexes();
    void transferSectors();
    void transferLinesAndSides();
    void transferSurfaceTints();
    void transferPolyobjs();
    void transferThings();

private:
    MapFormatId mapFormat;

    uint numVertexes;
    coord_t *vertexes; ///< Array of vertex coords [v0:X, vo:Y, v1:X, v1:Y, ..]

    Lines lines;
    Sides sides;
    Sectors sectors;
    Things things;
    SurfaceTints surfaceTints;
    Polyobjs polyobjs;

    de::StringPool materials; ///< Material dictionary.
};

#endif // WADMAPCONVERTER_ID1MAP_H
