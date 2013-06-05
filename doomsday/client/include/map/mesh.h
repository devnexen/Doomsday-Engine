/** @file data/mesh.h Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_DATA_MESH
#define DENG_DATA_MESH

#include <QSet>

#include <de/libdeng2.h>

class Vertex;

namespace de {

class Face;
class HEdge;

/**
 * Two dimensioned mesh geometry data structure employing the half-edge model
 * (more formally known as "Doubly connected edge list" (DECL)).
 *
 * @see http://en.wikipedia.org/wiki/Doubly_connected_edge_list
 *
 * @ingroup data
 */
class Mesh
{
public:
    //typedef QSet<Vertex *> Vertexes;
    typedef QSet<Face *> Faces;
    typedef QSet<HEdge *> HEdges;

public:
    Mesh();

    /**
     * Construct a new vertex.
     */
    //Vertex *newVertex();

    /**
     * Construct a new half-edge.
     */
    HEdge *newHEdge(Vertex &vertex);

    /**
     * Construct a new face.
     */
    Face *newFace();

    /**
     * Provides access to the set of all vertexes in the mesh.
     */
    //Vertexes const &vertexes() const;

    /**
     * Provides access to the set of all faces in the mesh.
     */
    Faces const &faces() const;

    /**
     * Provides access to the set of all half-edges in the mesh.
     */
    HEdges const &hedges() const;

    /**
     * Returns a pointer to the first Face in the mesh.
     *
     * @todo Refactor away.
     */
    Face *firstFace() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_DATA_MESH
