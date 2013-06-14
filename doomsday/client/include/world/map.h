/** @file map.h World map.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_H
#define DENG_WORLD_MAP_H

#include <QList>
#include <QSet>

#include "m_nodepile.h"
#include "p_particle.h"
#include "Polyobj"

class BspLeaf;
class BspNode;
class Line;
class Plane;
class Sector;
class Segment;
class Vertex;

#ifdef __CLIENT__

struct clmoinfo_s;

/**
 * The client mobjs are stored into a hash for quickly locating a ClMobj by its identifier.
 */
#define CLIENT_MOBJ_HASH_SIZE       (256)

/**
 * @ingroup world
 */
typedef struct cmhash_s {
    struct clmoinfo_s *first, *last;
} cmhash_t;

#define CLIENT_MAX_MOVERS          1024 // Definitely enough!

/**
 * @ingroup world
 */
typedef enum {
    CPT_FLOOR,
    CPT_CEILING
} clplanetype_t;

struct clplane_s;
struct clpolyobj_s;

#endif // __CLIENT__

namespace de {

class Blockmap;
class EntityDatabase;
#ifdef __CLIENT__
class Generators;
class LightGrid;
#endif
class Mesh;
class Thinkers;

/**
 * World map.
 *
 * @ingroup world
 */
class Map
{
    DENG2_NO_COPY  (Map)
    DENG2_NO_ASSIGN(Map)

public:
#ifdef __CLIENT__
    /// Required light grid is missing. @ingroup errors
    DENG2_ERROR(MissingLightGridError);
#endif

    /// Base error for runtime map editing errors. @ingroup errors
    DENG2_ERROR(EditError);

    /*
     * Linked-element lists/sets:
     */
    typedef QList<Vertex *>  Vertexes;
    typedef QList<Line *>    Lines;
    typedef QList<Polyobj *> Polyobjs;
    typedef QList<Sector *>  Sectors;

    typedef QList<BspNode *> BspNodes;
    typedef QList<BspLeaf *> BspLeafs;
    typedef QList<Segment *> Segments;

#ifdef __CLIENT__
    typedef QSet<Plane *>    PlaneSet;
    typedef QSet<Surface *>  SurfaceSet;
#endif

public: /// @todo make private:
#ifdef __CLIENT__
    cmhash_t clMobjHash[CLIENT_MOBJ_HASH_SIZE];

    struct clplane_s *clActivePlanes[CLIENT_MAX_MOVERS];
    struct clpolyobj_s *clActivePolyobjs[CLIENT_MAX_MOVERS];
#endif

    nodepile_t mobjNodes, lineNodes; // All kinds of wacky links.
    nodeindex_t *lineLinks; // Indices to roots.

    coord_t _globalGravity; // The defined gravity for this map.
    coord_t _effectiveGravity; // The effective gravity for this map.

    int _ambientLightLevel; // Ambient lightlevel for the current map.

public:
    Map(Uri const &uri = Uri());
    ~Map();

    /**
     * To be called to register the cvars and ccmds for this module.
     */
    static void consoleRegister();

    /**
     * This ID is the name of the lump tag that marks the beginning of map
     * data, e.g. "MAP03" or "E2M8".
     */
    Uri const &uri() const;

    /**
     * Returns the old 'unique' identifier attributed to the map.
     */
    char const *oldUniqueId() const;

    /**
     * Change the old 'unique' identifier attributed to the map.
     *
     * @param newUniqueId  New identifier to attribute.
     */
    void setOldUniqueId(char const *newUniqueId);

    /**
     * Returns the minimal and maximal boundary points for the map.
     */
    AABoxd const &bounds() const;

    /**
     * Returns the currently effective gravity multiplier for the map.
     */
    coord_t gravity() const;

    /**
     * Change the effective gravity multiplier for the map.
     *
     * @param gravity  New gravity multiplier.
     */
    void setGravity(coord_t gravity);

    /**
     * Returns the global ambient light level for the map.
     */
    int ambientLightLevel() const;

    /**
     * Provides access to the thinker lists for the map.
     */
    Thinkers /*const*/ &thinkers() const;

    /**
     * Provides a list of all the non-editable vertexes in the map.
     */
    Vertexes const &vertexes() const;

    /**
     * Provides a list of all the non-editable lines in the map.
     */
    Lines const &lines() const;

    /**
     * Provides a list of all the non-editable sectors in the map.
     */
    Sectors const &sectors() const;

    /**
     * Provides a list of all the non-editable polyobjs in the map.
     */
    Polyobjs const &polyobjs() const;

    /**
     * Provides access to the list of line segments for efficient traversal.
     */
    Segments const &segments() const;

    /**
     * Provides access to the list of BSP nodes for efficient traversal.
     */
    BspNodes const &bspNodes() const;

    /**
     * Provides access to the list of BSP leafs for efficient traversal.
     */
    BspLeafs const &bspLeafs() const;

    inline int vertexCount() const  { return vertexes().count(); }

    inline int lineCount() const    { return lines().count(); }

    inline int sideCount() const    { return lines().count() * 2; }

    inline int sectorCount() const  { return sectors().count(); }

    inline int polyobjCount() const { return polyobjs().count(); }

    inline int segmentCount() const { return segments().count(); }

    inline int bspNodeCount() const { return bspNodes().count(); }

    inline int bspLeafCount() const { return bspLeafs().count(); }

    /**
     * Given an @a emitter origin, attempt to identify the map element
     * to which it belongs.
     *
     * @param emitter  The sound emitter to be identified.
     * @param sector   The identified sector if found is written here.
     * @param poly     The identified polyobj if found is written here.
     * @param plane    The identified plane if found is written here.
     * @param surface  The identified line side surface if found is written here.
     *
     * @return  @c true iff @a emitter is an identifiable map element.
     */
    bool identifySoundEmitter(ddmobj_base_t const &emitter, Sector **sector,
        Polyobj **poly, Plane **plane, Surface **surface) const;

    /**
     * Locate a polyobj in the map by unique in-map tag.
     *
     * @param tag  Tag associated with the polyobj to be located.
     * @return  Pointer to the referenced polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjByTag(int tag) const;

    /**
     * Returns the root element for the map's BSP tree.
     */
    MapElement *bspRoot() const;

    /**
     * Determine the BSP leaf on the back side of the BS partition that lies
     * in front  of the specified point within the map's coordinate space.
     *
     * @note Always returns a valid BspLeaf although the point may not actually
     * lay within it (however it is on the same side of the space partition)!
     *
     * @param point  XY coordinates of the point to test.
     *
     * @return  BspLeaf instance for that BSP node's leaf.
     */
    BspLeaf *bspLeafAtPoint(Vector2d const &point) const;

    /**
     * @copydoc bspLeafAtPoint()
     *
     * The test is carried out using fixed-point math for behavior compatible
     * with vanilla DOOM. Note that this means there is a maximum size for the
     * point: it cannot exceed the fixed-point 16.16 range (about 65k units).
     */
    BspLeaf *bspLeafAtPoint_FixedPrecision(Vector2d const &point) const;

    /**
     * Provides access to the mobj blockmap.
     */
    Blockmap /*const*/ *mobjBlockmap() const;

    /**
     * Provides access to the polyobj blockmap.
     */
    Blockmap /*const*/ *polyobjBlockmap() const;

    /**
     * Provides access to the line blockmap.
     */
    Blockmap /*const*/ *lineBlockmap() const;

    /**
     * Provides access to the BSP leaf blockmap.
     */
    Blockmap /*const*/ *bspLeafBlockmap() const;

    int mobjsBoxIterator(AABoxd const &box,
        int (*callback) (struct mobj_s *, void *), void *parameters = 0) const;

    int linesBoxIterator(AABoxd const &box,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    int polyobjLinesBoxIterator(AABoxd const &box,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    /**
     * Lines and Polyobj lines (note polyobj lines are iterated first).
     *
     * @note validCount should be incremented before calling this to begin
     * a new logical traversal. Otherwise Lines marked with a validCount
     * equal to this will be skipped over (can be used to avoid processing
     * a line multiple times during complex / non-linear traversals.
     */
    int allLinesBoxIterator(AABoxd const &box,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    int bspLeafsBoxIterator(AABoxd const &box, Sector *sector,
        int (*callback) (BspLeaf *, void *), void *parameters = 0) const;

    /**
     * @note validCount should be incremented before calling this to begin a
     * new logical traversal. Otherwise Lines marked with a validCount equal
     * to this will be skipped over (can be used to avoid processing a line
     * multiple times during complex / non-linear traversals.
     */
    int polyobjsBoxIterator(AABoxd const &box,
        int (*callback) (struct polyobj_s *, void *), void *parameters = 0) const;

    /**
     * The callback function will be called once for each line that crosses
     * trough the object. This means all the lines will be two-sided.
     */
    int mobjLinesIterator(struct mobj_s *mo,
        int (*callback) (Line *, void *), void *parameters = 0) const;

    /**
     * Increment validCount before calling this routine. The callback function
     * will be called once for each sector the mobj is touching (totally or
     * partly inside). This is not a 3D check; the mobj may actually reside
     * above or under the sector.
     */
    int mobjSectorsIterator(struct mobj_s *mo,
        int (*callback) (Sector *, void *), void *parameters = 0) const;

    int lineMobjsIterator(Line *line,
        int (*callback) (struct mobj_s *, void *), void *parameters = 0) const;

    /**
     * Increment validCount before using this. 'func' is called for each mobj
     * that is (even partly) inside the sector. This is not a 3D test, the
     * mobjs may actually be above or under the sector.
     *
     * (Lovely name; actually this is a combination of SectorMobjs and
     * a bunch of LineMobjs iterations.)
     */
    int sectorTouchingMobjsIterator(Sector *sector,
        int (*callback) (struct mobj_s *, void *), void *parameters = 0) const;

    /**
     * Retrieve an immutable copy of the LOS trace line state.
     *
     * @todo Map should not own this data.
     */
    divline_t const &traceLine() const;

    /**
     * Retrieve an immutable copy of the LOS TraceOpening state.
     *
     * @todo Map should not own this data.
     */
    TraceOpening const &traceOpening() const;

    /**
     * Update the TraceOpening state for according to the opening defined by the
     * inner-minimal planes heights which intercept @a line
     *
     * If @a line is not owned by the map this is a no-op.
     *
     * @todo Map should not own this data.
     *
     * @param line  Map line to configure the opening for.
     */
    void setTraceOpening(Line &line);

    /**
     * Trace a line between @a from and @a to, making a callback for each
     * interceptable object linked within Blockmap cells which cover the path
     * this defines.
     */
    int pathTraverse(const_pvec2d_t from, const_pvec2d_t to, int flags,
                     traverser_t callback, void *parameters = 0);

    /**
     * @copydoc pathTraverse()
     *
     * @param fromX         X axis map space coordinate for the path origin.
     * @param fromY         Y axis map space coordinate for the path origin.
     * @param toX           X axis map space coordinate for the path destination.
     * @param toY           Y axis map space coordinate for the path destination.
     */
    inline int pathTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
                            int flags, traverser_t callback, void *parameters = 0)
    {
        coord_t from[2] = { fromX, fromY };
        coord_t to[2]   = { toX, toY };
        return pathTraverse(from, to, flags, callback, parameters);
    }

    /**
     * Links a mobj into both a block and a BSP leaf based on it's (x,y).
     * Sets mobj->bspLeaf properly. Calling with flags==0 only updates
     * the BspLeaf pointer. Can be called without unlinking first.
     */
    void link(struct mobj_s &mobj, byte flags);

    /**
     * Unlinks a mobj from everything it has been linked to.
     *
     * @param mo  Mobj to be unlinked.
     *
     * @return  DDLINK_* flags denoting what the mobj was unlinked from
     * (in case we need to re-link).
     */
    int unlink(struct mobj_s &mobj);

    /**
     * Link the specified @a mobj in any internal data structures for
     * bookkeeping purposes. Should be called AFTER mobj translation to
     * (re-)insert the mobj.
     *
     * @param mobj  Mobj to be linked (must be currently unlinked).
     */
    void linkMobjInBlockmap(struct mobj_s &mobj);

    /**
     * Unlink the specified @a mobj from any internal data structures for
     * bookkeeping purposes. Should be called BEFORE mobj translation to
     * extract the mobj.
     *
     * @param mobj  Mobj to be unlinked.
     */
    bool unlinkMobjInBlockmap(struct mobj_s &mobj);

    /**
     * Link the specified @a polyobj in any internal data structures for
     * bookkeeping purposes. Should be called AFTER Polyobj rotation and/or
     * translation to (re-)insert the polyobj.
     *
     * @param polyobj  Polyobj to be linked.
     */
    void linkPolyobj(Polyobj &polyobj);

    /**
     * Unlink the specified @a polyobj from any internal data structures for
     * bookkeeping purposes. Should be called BEFORE Polyobj rotation and/or
     * translation to extract the polyobj.
     *
     * @param polyobj  Polyobj to be unlinked.
     */
    void unlinkPolyobj(Polyobj &polyobj);

    EntityDatabase &entityDatabase() const;

#ifdef __CLIENT__
    coord_t skyFix(bool ceiling) const;

    inline coord_t skyFixFloor() const   { return skyFix(false /*the floor*/); }
    inline coord_t skyFixCeiling() const { return skyFix(true /*the ceiling*/); }

    void setSkyFix(bool ceiling, coord_t newHeight);

    inline void setSkyFixFloor(coord_t newHeight) {
        setSkyFix(false /*the floor*/, newHeight);
    }
    inline void setSkyFixCeiling(coord_t newHeight) {
        setSkyFix(true /*the ceiling*/, newHeight);
    }

    /**
     * Retrieve a pointer to the Generators collection for the map. If no collection
     * has yet been constructed a new empty collection will be initialized.
     *
     * @return  Generators collection for the map.
     */
    Generators &generators();

    /// @todo Should be private?
    void initClMobjs();

    /**
     * To be called when the client is shut down.
     * @todo Should be private?
     */
    void destroyClMobjs();

    /**
     * Deletes hidden, unpredictable or nulled mobjs for which we have not received
     * updates in a while.
     */
    void expireClMobjs();

    /**
     * Reset the client status. To be called when the map changes.
     */
    void reinitClMobjs();

    /**
     * Iterate the client mobj hash, exec the callback on each. Abort if callback
     * returns non-zero.
     *
     * @param callback  Function to callback for each client mobj.
     * @param context   Data pointer passed to the callback.
     *
     * @return  @c 0 if all callbacks return @c 0; otherwise the result of the last.
     */
    int clMobjIterator(int (*callback) (struct mobj_s *, void *), void *context);

    void initClMovers();

    void resetClMovers();

    /**
     * Allocate a new client-side plane mover.
     *
     * @return  The new mover or @c NULL if arguments are invalid.
     */
    struct clplane_s *newClPlane(int sectorIdx, clplanetype_t type, coord_t dest, float speed);

    void deleteClPlane(struct clplane_s *mover);

    int clPlaneIndex(struct clplane_s *mover);

    struct clplane_s *clPlaneBySectorIndex(int index, clplanetype_t type);

    bool isValidClPlane(int i);

    /**
     * @note Assumes there is no existing ClPolyobj for Polyobj @a index.
     */
    struct clpolyobj_s *newClPolyobj(int polyobjIndex);

    void deleteClPolyobj(struct clpolyobj_s *mover);

    int clPolyobjIndex(struct clpolyobj_s *mover);

    struct clpolyobj_s *clPolyobjByPolyobjIndex(int index);

    bool isValidClPolyobj(int i);

    /**
     * Returns the set of decorated surfaces for the map.
     */
    SurfaceSet /*const*/ &decoratedSurfaces();

    /**
     * Returns the set of glowing surfaces for the map.
     */
    SurfaceSet /*const*/ &glowingSurfaces();

    /**
     * $smoothmatoffset: interpolate the visual offset.
     */
    void lerpScrollingSurfaces(bool resetNextViewer = false);

    /**
     * $smoothmatoffset: Roll the surface material offset tracker buffers.
     */
    void updateScrollingSurfaces();

    /**
     * Returns the set of scrolling surfaces for the map.
     */
    SurfaceSet /*const*/ &scrollingSurfaces();

    /**
     * $smoothplane: interpolate the visual offset.
     */
    void lerpTrackedPlanes(bool resetNextViewer = false);

    /**
     * $smoothplane: Roll the height tracker buffers.
     */
    void updateTrackedPlanes();

    /**
     * Returns the set of tracked planes for the map.
     */
    PlaneSet /*const*/ &trackedPlanes();

#endif // __CLIENT__

    /**
     * Helper function for returning the relevant line side index for @a lineIndex
     * and @a backSide.
     *
     * Indices are produced as follows:
     * @code
     *  lineIndex / 2 + (backSide? 1 : 0);
     * @endcode
     *
     * @param lineIndex  Index of the Line in the map.
     * @param backSide   If @c =0 the Line::Front else Line::Back
     *
     * @return  Unique index for the line side.
     */
    static int toSideIndex(int lineIndex, int backSide);

    /**
     * Returns a pointer to the Line::Side associated with the specified @a index;
     * otherwise @c 0.
     */
    Line::Side *sideByIndex(int index) const;

public: /// @todo Make private:

    /**
     * Initializes the dummy element arrays used with the DMU API, with a
     * fixed number of dummies.
     */
    static void initDummies();

    /**
     * Initialize the node piles and link rings. To be called after map load.
     */
    void initNodePiles();

    /**
     * Initialize all polyobjs in the map. To be called after map load.
     */
    void initPolyobjs();

#ifdef __CLIENT__
    /**
     * Fixing the sky means that for adjacent sky sectors the lower sky
     * ceiling is lifted to match the upper sky. The raising only affects
     * rendering, it has no bearing on gameplay.
     */
    void initSkyFix();

    void buildSurfaceLists();

    /**
     * Returns @c true iff a LightGrid has been initialized for the map.
     *
     * @see lightGrid()
     */
    bool hasLightGrid();

    /**
     * Provides access to the light grid for the map.
     *
     * @see hasLightGrid()
     */
    LightGrid &lightGrid();

    /**
     * Initialize the ambient lighting grid (smoothed sector lighting).
     * If the grid has already been initialized calling this will perform
     * a full update.
     */
    void initLightGrid();

    /**
     * @todo Replace with a de::Observers-based mechanism.
     */
    void updateMissingMaterialsForLinesOfSector(Sector const &sec);

#endif // __CLIENT__

    /**
     * To be called in response to a Material property changing which may
     * require updating any map surfaces which are presently using it.
     *
     * @todo Replace with a de::Observers-based mechanism.
     */
    void updateSurfacesOnMaterialChange(Material &material);

public:
    /*
     * Runtime map editing:
     */

    /// @return @c true= a new BSP was built successfully for the map.
    bool endEditing();

    Vertex *createVertex(Vector2d const &origin,
                         int archiveIndex = MapElement::NoIndex);

    Line *createLine(Vertex &v1, Vertex &v2, int flags = 0,
                     Sector *frontSector = 0, Sector *backSector = 0,
                     int archiveIndex = MapElement::NoIndex);

    Sector *createSector(float lightLevel, Vector3f const &lightColor,
                         int archiveIndex = MapElement::NoIndex);

    Polyobj *createPolyobj(Vector2d const &origin);

    /**
     * Provides a list of all the editable vertexes in the map.
     */
    Vertexes const &editableVertexes() const;

    /**
     * Provides a list of all the editable lines in the map.
     */
    Lines const &editableLines() const;

    /**
     * Provides a list of all the editable sectors in the map.
     */
    Sectors const &editableSectors() const;

    /**
     * Provides a list of all the editable polyobjs in the map.
     */
    Polyobjs const &editablePolyobjs() const;

    inline int editableVertexCount() const  { return editableVertexes().count(); }

    inline int editableLineCount() const    { return editableLines().count(); }

    inline int editableSectorCount() const  { return editableSectors().count(); }

    inline int editablePolyobjCount() const { return editablePolyobjs().count(); }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_MAP_H
