/** @file materialmanifest.cpp  Description of a logical material resource.
 *
 * @authors Copyright © 2011-2015 Daniel Swanson <danij@dengine.net>
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

#include "resource/materialmanifest.h"

#include "dd_main.h"  // App_ResourceSystem()

using namespace de;

DENG2_PIMPL_NOREF(MaterialManifest)
, DENG2_OBSERVES(Material, Deletion)
{
    Flags flags;                         ///< Classification flags.
    materialid_t id = 0;                 ///< Globally unique identifier.
    std::unique_ptr<Material> material;  ///< Associated resource (if any).

//    ~Instance()
//    {
//        if(material) material->audienceForDeletion() -= this;
//    }

    void materialBeingDeleted(Material const &)
    {
        material.release();
    }
};

MaterialManifest::MaterialManifest(PathTree::NodeArgs const &args)
    : Node(args)
    , d(new Instance)
{}

MaterialManifest::~MaterialManifest()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->materialManifestBeingDeleted(*this);
}

Material *MaterialManifest::derive()
{
    if(!hasMaterial())
    {
        // Instantiate and associate the new material with this.
        setMaterial(new Material(*this));

        // Notify interested parties that a new material was derived from the manifest.
        DENG2_FOR_AUDIENCE(MaterialDerived, i) i->materialManifestMaterialDerived(*this, material());
    }
    return &material();
}

materialid_t MaterialManifest::id() const
{
    return d->id;
}

void MaterialManifest::setId(materialid_t id)
{
    d->id = id;
}

MaterialScheme &MaterialManifest::scheme() const
{
    LOG_AS("MaterialManifest::scheme");

    /// @todo Optimize: MaterialManifest should contain a link to the owning MaterialScheme.
    MaterialScheme *found = nullptr;
    App_ResourceSystem().forAllMaterialSchemes([this, &found] (MaterialScheme &scheme)
    {
        if(&scheme.index() == &tree())
        {
            found = &scheme;
            return LoopAbort;
        }
        return LoopContinue;
    });
    if(found) return *found;

    /// @throw Error Failed to determine the scheme of the manifest (should never happen...).
    throw Error("MaterialManifest::scheme", String("Failed to determine scheme for manifest [%1]").arg(de::dintptr(this)));
}

String const &MaterialManifest::schemeName() const
{
    return scheme().name();
}

String MaterialManifest::description(de::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    String info = String("%1 %2")
                      .arg(composeUri().compose(uriCompositionFlags | de::Uri::DecodePath),
                           ( uriCompositionFlags.testFlag(de::Uri::OmitScheme)? -14 : -22 ) )
                      .arg(sourceDescription(), -7);
#ifdef __CLIENT__
    info += String("x%1").arg(!hasMaterial()? 0 : material().animatorCount());
#endif
    return info;
}

String MaterialManifest::sourceDescription() const
{
    if(!isCustom())       return "game";
    if(isAutoGenerated()) return "add-on";  // Unintuitive but correct.
    return "def";
}

MaterialManifest::Flags MaterialManifest::flags() const
{
    return d->flags;
}

void MaterialManifest::setFlags(MaterialManifest::Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

bool MaterialManifest::hasMaterial() const
{
    return bool(d->material);
}

Material &MaterialManifest::material() const
{
    if(hasMaterial()) return *d->material;
    /// @throw MissingMaterialError  The manifest is not presently associated with a material.
    throw MissingMaterialError("MaterialManifest::material", "Missing required material");
}

Material *MaterialManifest::materialPtr() const
{
    return hasMaterial()? &material() : nullptr;
}

void MaterialManifest::setMaterial(Material *newMaterial)
{
    if(d->material.get() != newMaterial)
    {
        if(d->material)
        {
            // Cancel notifications about the existing material.
            d->material->audienceForDeletion() -= d;
        }

        d->material.reset(newMaterial);

        if(d->material)
        {
            // We want notification when the new material is about to be deleted.
            d->material->audienceForDeletion() += d;
        }
    }
}
