/** @file plane.h World map plane.
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

#include <de/Log>

#include "world/map.h"
#include "world/world.h" /// ddMapSetup
#include "Surface"

#include "render/r_main.h" // frameTimePos

#include "world/plane.h"

using namespace de;

DENG2_PIMPL(Plane)
{
    /// Sound emitter.
    SoundEmitter soundEmitter;

    /// Index of the plane in the owning sector.
    int indexInSector;

    /// Current @em sharp height relative to @c 0 on the map up axis (positive is up).
    coord_t height;

    /// @em sharp height change tracking buffer (for smoothing).
    coord_t oldHeight[2];

    /// Target @em sharp height.
    coord_t targetHeight;

    /// Visual plane height (smoothed).
    coord_t visHeight;

    /// Delta between the current @em sharp height and the visual height.
    coord_t visHeightDelta;

    /// Movement speed (map space units per tic).
    coord_t speed;

    /// Plane surface.
    Surface surface;

    Instance(Public *i, coord_t height)
        : Base(i),
          indexInSector(-1),
          height(height),
          targetHeight(height),
          visHeight(height),
          visHeightDelta(0),
          speed(0),
          surface(dynamic_cast<MapElement &>(*i))
    {
        oldHeight[0] = oldHeight[1] = height;
        zap(soundEmitter);
    }

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->planeBeingDeleted(self);

#ifdef __CLIENT__
        Map &map = self.map();

        // If this plane is currently being watched - remove it.
        map.trackedPlanes().remove(&self);

        // If this plane's surface is in the moving list - remove it.
        map.scrollingSurfaces().remove(&surface);

        // If this plane's surface is linked any material list(s) - remove it.
        map.unlinkInMaterialLists(&surface);

#endif // __CLIENT__
    }

    void notifyHeightChanged(coord_t oldHeight)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(HeightChange, i)
        {
            i->planeHeightChanged(self, oldHeight);
        }
    }

    void applySharpHeightChange(coord_t newHeight)
    {
        // No change?
        if(de::fequal(newHeight, height))
            return;

        coord_t oldHeight = height;
        height = newHeight;

        if(!ddMapSetup)
        {
            // Update the sound emitter origin for the plane.
            self.updateSoundEmitterOrigin();

#ifdef __CLIENT__
            // We need the decorations updated.
            /// @todo optimize: Translation on the world up axis would be a
            /// trivial operation to perform, which, would not require plotting
            /// decorations again. This frequent case should be designed for.
            surface.markAsNeedingDecorationUpdate();
#endif
        }

        // Notify interested parties of the change.
        notifyHeightChanged(oldHeight);

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            // Add ourself to tracked plane list (for movement interpolation).
            self.map().trackedPlanes().insert(&self);

            markDependantSurfacesForDecorationUpdate();
        }
#endif
    }

#ifdef __CLIENT__
    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     *
     * @todo Sector should observe instead.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        if(ddMapSetup) return;

        // "Middle" planes have no dependent surfaces.
        if(indexInSector > Sector::Ceiling) return;

        // Mark the decor lights on the sides of this plane as requiring an update.
        foreach(LineSide *side, self.sector().sides())
        {
            if(side->hasSections())
            {
                side->middle().markAsNeedingDecorationUpdate();
                side->bottom().markAsNeedingDecorationUpdate();
                side->top().markAsNeedingDecorationUpdate();
            }

            if(side->back().hasSections())
            {
                LineSide &back = side->back();
                back.middle().markAsNeedingDecorationUpdate();
                back.bottom().markAsNeedingDecorationUpdate();
                back.top().markAsNeedingDecorationUpdate();
            }
        }
    }
#endif // __CLIENT__
};

Plane::Plane(Sector &sector, Vector3f const &normal, coord_t height)
    : MapElement(DMU_PLANE, &sector), d(new Instance(this, height))
{
    setNormal(normal);
}

Sector &Plane::sector()
{
    return this->parent().as<Sector>();
}

Sector const &Plane::sector() const
{
    return const_cast<Sector const &>(const_cast<Plane *>(this)->sector());
}

int Plane::indexInSector() const
{
    return d->indexInSector;
}

void Plane::setIndexInSector(int newIndex)
{
    d->indexInSector = newIndex;
}

bool Plane::isSectorFloor() const
{
    return this == &sector().floor();
}

bool Plane::isSectorCeiling() const
{
    return this == &sector().ceiling();
}

Surface &Plane::surface()
{
    return d->surface;
}

Surface const &Plane::surface() const
{
    return d->surface;
}

void Plane::setNormal(Vector3f const &newNormal)
{
    d->surface.setNormal(newNormal); // will normalize
}

SoundEmitter &Plane::soundEmitter()
{
    return d->soundEmitter;
}

SoundEmitter const &Plane::soundEmitter() const
{
    return const_cast<SoundEmitter const &>(const_cast<Plane &>(*this).soundEmitter());
}

void Plane::updateSoundEmitterOrigin()
{
    LOG_AS("Plane::updateSoundEmitterOrigin");

    d->soundEmitter.origin[VX] = sector().soundEmitter().origin[VX];
    d->soundEmitter.origin[VY] = sector().soundEmitter().origin[VY];
    d->soundEmitter.origin[VZ] = d->height;
}

coord_t Plane::height() const
{
    return d->height;
}

coord_t Plane::targetHeight() const
{
    return d->targetHeight;
}

coord_t Plane::speed() const
{
    return d->speed;
}

#ifdef __CLIENT__

coord_t Plane::visHeight() const
{
    // $smoothplane
    return d->visHeight;
}

coord_t Plane::visHeightDelta() const
{
    // $smoothplane
    return d->visHeightDelta;
}

void Plane::lerpVisHeight()
{
    // $smoothplane
    d->visHeightDelta = d->oldHeight[0] * (1 - frameTimePos) + d->height * frameTimePos - d->height;

    // Visible plane height.
    d->visHeight = d->height + d->visHeightDelta;

    d->markDependantSurfacesForDecorationUpdate();
}

void Plane::resetVisHeight()
{
    // $smoothplane
    d->visHeightDelta = 0;
    d->visHeight = d->oldHeight[0] = d->oldHeight[1] = d->height;

    d->markDependantSurfacesForDecorationUpdate();
}

void Plane::updateHeightTracking()
{
    // $smoothplane
    d->oldHeight[0] = d->oldHeight[1];
    d->oldHeight[1] = d->height;

    if(d->oldHeight[0] != d->oldHeight[1])
    {
        if(de::abs(d->oldHeight[0] - d->oldHeight[1]) >= MAX_SMOOTH_MOVE)
        {
            // Too fast: make an instantaneous jump.
            d->oldHeight[0] = d->oldHeight[1];
        }
    }
}

#endif // __CLIENT__

int Plane::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_EMITTER:
        args.setValue(DMT_PLANE_EMITTER, &d->soundEmitter, 0);
        break;
    case DMU_SECTOR: {
        Sector *secPtr = &const_cast<Plane *>(this)->sector();
        args.setValue(DMT_PLANE_SECTOR, &secPtr, 0);
        break; }
    case DMU_HEIGHT:
        args.setValue(DMT_PLANE_HEIGHT, &d->height, 0);
        break;
    case DMU_TARGET_HEIGHT:
        args.setValue(DMT_PLANE_TARGET, &d->targetHeight, 0);
        break;
    case DMU_SPEED:
        args.setValue(DMT_PLANE_SPEED, &d->speed, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Plane::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_HEIGHT: {
        coord_t newHeight = d->height;
        args.value(DMT_PLANE_HEIGHT, &newHeight, 0);
        d->applySharpHeightChange(newHeight);
        break; }
    case DMU_TARGET_HEIGHT:
        args.value(DMT_PLANE_TARGET, &d->targetHeight, 0);
        break;
    case DMU_SPEED:
        args.value(DMT_PLANE_SPEED, &d->speed, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
