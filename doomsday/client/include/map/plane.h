/** @file plane.h World Map Plane.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_PLANE
#define DENG_WORLD_MAP_PLANE

#include <QSet>

#include <de/Error>
#include <de/Observers>
#include <de/Vector>

#include "MapElement"
#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "map/surface.h"

class Sector;

/**
 * World map sector plane.
 *
 * @ingroup map
 */
class Plane : public de::MapElement
{
public:
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /**
     * Observers to be notified when a Plane is about to be deleted.
     */
    DENG2_DEFINE_AUDIENCE(Deletion, void planeBeingDeleted(Plane const &plane))

    /**
     * Observers to be notified whenever a @em sharp height change occurs.
     */
    DENG2_DEFINE_AUDIENCE(HeightChange, void planeHeightChanged(Plane &plane, coord_t oldHeight))

    static int const MAX_SMOOTH_MOVE; ///< 64, $smoothplane: Maximum speed for a smoothed plane.

    /// In-Sector plane types: @todo move to Sector
    enum Type
    {
        Floor,
        Ceiling,
        Middle
    };

public:
    /**
     * Construct a new plane.
     *
     * @param sector  Sector that will own the plane.
     * @param normal  Normal of the plane (will be normalized if necessary).
     * @param height  Height of the plane in map space coordinates.
     */
    Plane(Sector &sector, de::Vector3f const &normal, coord_t height = 0);
    ~Plane();

    /**
     * Returns the owning Sector of the plane.
     */
    Sector &sector();

    /// @copydoc sector()
    Sector const &sector() const;

    /**
     * Returns the index of the plane within the owning Sector.
     *
     * @todo Refactor away.
     */
    uint inSectorIndex() const;

    /// @todo Refactor away.
    void setInSectorIndex(uint newIndex);

    /**
     * Returns the Surface of the plane.
     */
    Surface &surface();

    /// @copydoc surface()
    Surface const &surface() const;

    /**
     * Returns the current @em sharp height of the plane relative to @c 0 on the
     * map up axis. The HeightChange audience is notified whenever the height
     * changes.
     */
    coord_t height() const;

    /**
     * Returns the target height of the plane in the map coordinate space.
     * The target height is the destination height following a successful move.
     * Note that this may be the same as @ref height(), in which case the plane
     * is not currently moving. The HeightChange audience is notified whenever
     * the current @em sharp height changes.
     *
     * @see speed(), height()
     */
    coord_t targetHeight() const;

    /**
     * Returns the rate at which the plane height will be updated (units per tic)
     * when moving to the target height in the map coordinate space.
     *
     * @see targetHeight(), height()
     */
    coord_t speed() const;

    /**
     * Returns the current interpolated visual height of the plane in the map
     * coordinate space.
     *
     * @see targetHeight(), height()
     */
    coord_t visHeight() const;

    /**
     * Returns the delta between current height and the interpolated visual
     * height of the plane in the map coordinate space.
     *
     * @see targetHeight()
     */
    coord_t visHeightDelta() const;

    /**
     * Set the visible offsets.
     *
     * @see visHeight(), targetHeight()
     */
    void lerpVisHeight();

    /**
     * Reset the plane's height tracking.
     *
     * @see visHeight(), targetHeight()
     */
    void resetVisHeight();

    /**
     * Roll the plane's height tracking buffer.
     *
     * @see targetHeight()
     */
    void updateHeightTracking();

    /**
     * Change the normal of the plane to @a newNormal (which if necessary will
     * be normalized before being assigned to the plane).
     *
     * @post The plane's tangent vectors and logical plane type will have been
     * updated also.
     */
    void setNormal(de::Vector3f const &newNormal);

    /**
     * Returns the logical type of the plane.
     */
    Type type() const;

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);

private:
    DENG2_PRIVATE(d)
};

/**
 * A set of Planes.
 * @ingroup map
 */
typedef QSet<Plane *> PlaneSet;

#endif // DENG_WORLD_MAP_PLANE
