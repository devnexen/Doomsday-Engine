/**
 * @file pathmap.c
 * Fragment map of a delimited string @ingroup fs
 *
 * @authors Copyright © 2011-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"

#include "pathmap.h"

static ushort PathMap_HashFragment(PathMap* pm, PathMapFragment* fragment)
{
    assert(pm && fragment);
    // Is it time to compute the hash for this fragment?
    if(fragment->hash == PATHDIRECTORY_NOHASH)
    {
        fragment->hash = PathDirectory_HashPath(fragment->from,
            (fragment->to - fragment->from) + 1, pm->delimiter);
    }
    return fragment->hash;
}

/// @return  @c true iff @a fragment comes from the static fragment buffer
///          allocated along with @a pm.
static __inline boolean PathMap_IsStaticFragment(const PathMap* pm, const PathMapFragment* fragment)
{
    assert(pm && fragment);
    return fragment >= pm->fragmentBuffer &&
           fragment < (pm->fragmentBuffer + sizeof(*pm->fragmentBuffer) * PATHMAP_FRAGMENTBUFFER_SIZE);
}

static PathMapFragment* PathMap_AllocFragment(PathMap* pm)
{
    PathMapFragment* fragment;
    assert(pm);

    // Retrieve another fragment.
    if(pm->fragmentCount < PATHMAP_FRAGMENTBUFFER_SIZE)
    {
        fragment = pm->fragmentBuffer + pm->fragmentCount;
    }
    else
    {
        // Allocate an "extra" fragment.
        fragment = (PathMapFragment*)malloc(sizeof *fragment);
        if(!fragment) Con_Error("PathMap::AllocFragment: Failed on allocation of %lu bytes for new PathMap::Fragment.", (unsigned long) sizeof *fragment);
    }

    fragment->from = fragment->to = NULL;

    // Hashing is deferred; means not-hashed yet.
    fragment->hash = PATHDIRECTORY_NOHASH;
    fragment->next = NULL;

    // There is now one more fragment in the map.
    pm->fragmentCount += 1;

    return fragment;
}

static void PathMap_MapAllFragments(PathMap* pm, const char* path, size_t pathLen)
{
    PathMapFragment* fragment = NULL;
    const char* begin = path;
    const char* to = begin + pathLen - 1;
    const char* from;
    size_t i;
    assert(pm);

    pm->fragmentCount = 0;
    pm->extraFragments = NULL;

    if(pathLen == 0) return;

    // Skip over any trailing delimiters.
    for(i = pathLen; *to && *to == pm->delimiter && i-- > 0; to--) {}

    // Scan for discreet fragments in the path, in reverse order.
    for(;;)
    {
        // Find the start of the next path fragment.
        for(from = to; from > begin && !(*from == pm->delimiter); from--) {}

        { PathMapFragment* newFragment = PathMap_AllocFragment(pm);

        // If this is an "extra" fragment, link it to the tail of the list.
        if(!PathMap_IsStaticFragment(pm, newFragment))
        {
            if(!pm->extraFragments)
            {
                pm->extraFragments = newFragment;
            }
            else
            {
                fragment->next = pm->extraFragments;
            }
        }

        fragment = newFragment;
        }

        fragment->from = (*from == pm->delimiter? from + 1 : from);
        fragment->to   = to;

        // Are there no more parent directories?
        if(from == begin) break;

        // So far so good. Move one directory level upwards.
        // The next fragment ends here.
        to = from-1;
    }

    // Deal with the special case of a Unix style zero-length root name.
    if(*begin == pm->delimiter)
    {
        fragment = PathMap_AllocFragment(pm);
        fragment->from = fragment->to = "";
    }
}

static void PathMap_ClearFragments(PathMap* pm)
{
    assert(pm);
    while(pm->extraFragments)
    {
        PathMapFragment* next = pm->extraFragments->next;
        free(pm->extraFragments);
        pm->extraFragments = next;
    }
}

PathMap* PathMap_Initialize2(PathMap* pm, const char* path, char delimiter)
{
    assert(pm);

#if _DEBUG
    // Perform unit tests.
    PathMap_Test();
#endif

    pm->path = path;
    pm->delimiter = delimiter;

    // Create the fragment map of the path.
    PathMap_MapAllFragments(pm, pm->path, pm->path? strlen(pm->path) : 0);

    // Hash the first (i.e., rightmost) fragment right away.
    PathMap_Fragment(pm, 0);

    return pm;
}

PathMap* PathMap_Initialize(PathMap* pm, const char* path)
{
    return PathMap_Initialize2(pm, path, '/');
}

void PathMap_Destroy(PathMap* pm)
{
    assert(pm);
    PathMap_ClearFragments(pm);
}

uint PathMap_Size(PathMap* pm)
{
    assert(pm);
    return pm->fragmentCount;
}

const PathMapFragment* PathMap_Fragment(PathMap* pm, uint idx)
{
    PathMapFragment* fragment;
    if(!pm || idx >= pm->fragmentCount) return NULL;
    if(idx < PATHMAP_FRAGMENTBUFFER_SIZE)
    {
        fragment = pm->fragmentBuffer + idx;
    }
    else
    {
        uint n = PATHMAP_FRAGMENTBUFFER_SIZE;
        fragment = pm->extraFragments;
        while(n++ < idx)
        {
            fragment = fragment->next;
        }
    }
    PathMap_HashFragment(pm, fragment);
    return fragment;
}

#if _DEBUG
void PathMap_Test(void)
{
    static boolean alreadyTested = false;

    const PathMapFragment* fragment;
    size_t len;
    PathMap pm;

    if(alreadyTested) return;
    alreadyTested = true;

    // Test a zero-length path.
    PathMap_Initialize(&pm, "");
    assert(PathMap_Size(&pm) == 0);
    PathMap_Destroy(&pm);

    // Test a Windows style path with a drive plus file path.
    PathMap_Initialize(&pm, "c:/something.ext");
    assert(PathMap_Size(&pm) == 2);

    fragment = PathMap_Fragment(&pm, 0);
    len = fragment->to - fragment->from;
    assert(len == 12);
    assert(!strncmp(fragment->from, "something.ext", len+1));

    fragment = PathMap_Fragment(&pm, 1);
    len = fragment->to - fragment->from;
    assert(len == 1);
    assert(!strncmp(fragment->from, "c:", len+1));

    PathMap_Destroy(&pm);

    // Test a Unix style path with a zero-length root node name.
    PathMap_Initialize(&pm, "/something.ext");
    assert(PathMap_Size(&pm) == 2);

    fragment = PathMap_Fragment(&pm, 0);
    len = fragment->to - fragment->from;
    assert(len == 12);
    assert(!strncmp(fragment->from, "something.ext", len+1));

    fragment = PathMap_Fragment(&pm, 1);
    len = fragment->to - fragment->from;
    assert(len == 0);
    assert(!strncmp(fragment->from, "", len));

    PathMap_Destroy(&pm);

    // Test a relative directory.
    PathMap_Initialize(&pm, "some/dir/structure/");
    assert(PathMap_Size(&pm) == 3);

    fragment = PathMap_Fragment(&pm, 0);
    len = fragment->to - fragment->from;
    assert(len == 8);
    assert(!strncmp(fragment->from, "structure", len+1));

    fragment = PathMap_Fragment(&pm, 1);
    len = fragment->to - fragment->from;
    assert(len == 2);
    assert(!strncmp(fragment->from, "dir", len+1));

    fragment = PathMap_Fragment(&pm, 2);
    len = fragment->to - fragment->from;
    assert(len == 3);
    assert(!strncmp(fragment->from, "some", len+1));

    PathMap_Destroy(&pm);
}
#endif
