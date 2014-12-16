/** @file materialtexturelayer.h  Logical material, texture layer.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RESOURCE_MATERIALTEXTURELAYER_H
#define CLIENT_RESOURCE_MATERIALTEXTURELAYER_H

#include <de/String>
#include <doomsday/defs/dedtypes.h>
#include "Material"
#include "Texture"

/**
 * Specialized MaterialLayer for describing an animated texture layer.
 *
 * @ingroup resource
 */
class MaterialTextureLayer : public MaterialLayer
{
public:
    /**
     * Stages describe texture change animations.
     */
    class AnimationStage : public Stage
    {
    public:
        AnimationStage(de::Texture *texture, int tics,
                       float variance                     = 0,
                       float glowStrength                 = 0,
                       float glowStrengthVariance         = 0,
                       de::Vector2f const origin          = de::Vector2f(),
                       de::Texture *maskTexture           = nullptr,
                       de::Vector2f const &maskDimensions = de::Vector2f(1, 1),
                       blendmode_t blendMode              = BM_NORMAL,
                       float opacity                      = 1);
        AnimationStage(AnimationStage const &other);
        virtual ~AnimationStage();

        /**
         * Construct a new AnimationStage from the given @a definition.
         */
        static AnimationStage *fromDef(ded_material_layer_stage_t const &definition);

        /**
         * Returns the Texture in effect for the animation stage.
         */
        de::Texture *texture() const;

        /**
         * Returns the material space, texture origin in effect for the animation stage.
         */
        de::Vector2f const &origin() const;

        /**
         * Returns the mask Texture in effect for the animation stage.
         */
        de::Texture *maskTexture() const;

        /**
         * Returns the mask dimensions in effect for the animation stage.
         */
        de::Vector2f const &maskDimensions() const;

        /**
         * Returns the opacity factor in effect for the animation stage.
         */
        float opacity() const;

        /**
         * Returns the texture blending mode in effect for the animation stage.
         */
        blendmode_t blendMode() const;

        /**
         * Returns the glow strength factor in effect for the animation stage.
         */
        float glowStrength() const;

        /**
         * Returns the glow strength variance factor in effect for the animation stage.
         */
        float glowStrengthVariance() const;

        de::String description() const;

        /// @todo refactor away:
        void setTexture(de::Texture *newTexture);

    private:
        DENG2_PRIVATE(d)
    };

public:
    virtual ~MaterialTextureLayer() {}

    /**
     * Construct a new TextureLayer from the given @a definition.
     */
    static MaterialTextureLayer *fromDef(ded_material_layer_t const &definition);

    /**
     * Returns @c true if glow is enabled for one or more animation stages.
     */
    bool hasGlow() const;

    /**
     * Add a new animation stage to the texture layer.
     *
     * @param stage  New stage to add (a copy is made).
     *
     * @return  Index of the newly added stage (0 based).
     */
    int addStage(AnimationStage const &stage);

    /**
     * Lookup an AnimationStage by it's unique @a index.
     *
     * @param index  Index of the AnimationStage to lookup. Will be cycled into valid range.
     */
    AnimationStage &stage(int index) const;

    de::String describe() const;
};

#endif  // CLIENT_RESOURCE_MATERIALTEXTURELAYER_H
