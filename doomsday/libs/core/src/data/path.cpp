/** @file path.cpp Textual path composed of segments.
 *
 * @author Copyright © 2010-2013 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/Path"
#include "de/Reader"
#include "de/Writer"

#include <de/math.h>
#include <de/ByteRefArray>

namespace de {

/// Size of the fixed-size portion of the segment buffer.
static int const SEGMENT_BUFFER_SIZE = 8;

//---------------------------------------------------------------------------------------

Path::hash_type const Path::hash_range = 0xffffffff;

Path::hash_type Path::Segment::hash() const
{
    // Is it time to compute the hash?
    if (!(flags & GotHashKey))
    {
        hashKey = de::crc32(range.toString().lower()) % hash_range;
        flags |= GotHashKey;
    }
    return hashKey;
}

bool Path::Segment::hasWildCard() const
{
    if (flags & WildCardChecked)
    {
        return flags.testFlag(IncludesWildCard);
    }
    bool isWild = range.contains('*');
    applyFlagOperation(flags, IncludesWildCard, isWild? SetFlags : UnsetFlags);
    flags |= WildCardChecked;
    return isWild;
}

bool Path::Segment::operator==(Path::Segment const &other) const
{
    return !range.compare(other.range, &iCaseInsensitive);
}

bool Path::Segment::operator < (Path::Segment const &other) const
{
    return range.compare(other.range, &iCaseInsensitive) < 0;
}

int Path::Segment::length() const
{
    return range.size();
}

dsize Path::Segment::size() const
{
    return range.size();
}

//Path::Segment::operator String() const
//{
//    return range;
//}

//String Path::Segment::toString() const
//{
//    return range.string()->mid(range.position(), range.size());
//}

//---------------------------------------------------------------------------------------

DE_PIMPL_NOREF(Path)
{
    static String emptyPath;

    String path;

    /// The character(s) in Impl::path that act(s) as the segment separator.
    /// This is assumed to be a single multibyte character.
    String separator;

    /**
     * Total number of segments in the path. If 0, it means that the path
     * isn't parsed into segments yet -- all paths have at least one segment
     * (an empty one if nothing else).
     */
    int segmentCount;

    /**
     * Fixed-size array for the segments of the path.
     *
     * The segments array is composed of two parts: the first
     * SEGMENT_BUFFER_SIZE elements are placed into a fixed-size array which is
     * allocated along with the Instance, and additional segments are allocated
     * dynamically and linked in the extraSegments list.
     *
     * This optimized representation should mean that the majority of paths
     * can be represented without dynamically allocating memory from the heap
     * (apart from the Instance, that is).
     *
     * @note Contents of the array are not initialized to zero.
     */
    Path::Segment segments[SEGMENT_BUFFER_SIZE];

    /**
     * List of the extra segments that don't fit in segments, in reverse
     * order.
     */
    List<Path::Segment> extraSegments;

    Impl()
        : separator("/")
        , segmentCount(0)
    {}

    Impl(const String &p, const String &sep)
        : path(p)
        , separator(sep)
        , segmentCount(0)
    {}

    Impl(const String &p, Char sep)
        : path(p)
        , separator(1, sep)
        , segmentCount(0)
    {}

    ~Impl()
    {
        clearSegments();
    }

    /**
     * Clear the segment array.
     *
     * @post The map will need to be rebuilt with parse().
     */
    void clearSegments()
    {
        extraSegments.clear();
        zap(segments);
        segmentCount = 0;
    }

    /**
     * Allocate a new segment, either from the fixed-size segments array if it
     * isn't full, or dynamically from the heap.
     *
     * @return  New segment.
     */
    Path::Segment *allocSegment(const String &range)
    {
        Path::Segment *segment;
        if (segmentCount < SEGMENT_BUFFER_SIZE)
        {
            segment = segments + segmentCount;
        }
        else
        {
            // Allocate an "extra" node.
            extraSegments.append(Path::Segment());
            segment = &extraSegments.last();
        }

        zapPtr(segment);
        segment->range = range;

        // There is now one more segment in the map.
        segmentCount++;

        return segment;
    }

    /**
     * Build the segment array by parsing the path. when the path is modified,
     * the existing map is invalidated and needs to be remapped.
     */
    void parse()
    {
        // Already been here?
        if (segmentCount > 0) return;

        segmentCount = 0;
        extraSegments.clear();

        if (path.isEmpty())
        {
            // There always has to be at least one segment.
            allocSegment(&emptyPath);

            DE_ASSERT(segmentCount > 0);
            return;
        }

        Char const *segBegin = path.constData();
        Char const *segEnd   = path.constData() + path.length() - 1;

        // Skip over any trailing delimiters.
        for (int i = path.length();
            segEnd->unicode() && *segEnd == separator && i-- > 0;
            --segEnd) {}

        // Scan the path for segments, in reverse order.
        Char const *from;
        for (;;)
        {
            if (segEnd < segBegin) break; // E.g., path is "/"

            // Find the start of the next segment.
            for (from = segEnd; from > segBegin && !(*from == separator); from--)
            {}

            int startIndex = (*from == separator? from + 1 : from) - path.constData();
            int length = (segEnd - path.constData()) - startIndex + 1;
            allocSegment(QStringRef(&path, startIndex, length));

            // Are there no more parent directories?
            if (from == segBegin) break;

            // So far so good. Move one directory level upwards.
            // The next name ends here.
            segEnd = from - 1;
        }

        // Unix style zero-length root name?
        if (*segBegin == separator)
        {
            allocSegment(&emptyPath);
        }

        DE_ASSERT(segmentCount > 0);
    }
};

Path::Path() : d(new Impl)
{}

Path::Path(String const &path, Char sep)
    : d(new Impl(path, sep))
{}

Path::Path(char const *nullTerminatedCStr, Char sep)
    : d(new Impl(nullTerminatedCStr, sep))
{}

Path::Path(char const *nullTerminatedCStr)
    : d(new Impl(nullTerminatedCStr, '/'))
{}

Path::Path(Path const &other)
    : d(new Impl(other.d->path, other.d->separator))
{}

Path::Path(Path &&moved)
    : d(std::move(moved.d))
{}

Path &Path::operator=(char const *pathUtf8)
{
    return *this = Path(pathUtf8);
}

Path &Path::operator=(Path const &other)
{
    d.reset(new Impl(other.d->path, other.d->separator));
    return *this;
}

Path &Path::operator=(Path &&moved)
{
    d = std::move(moved.d);
    return *this;
}

Path Path::operator+(String const &str) const
{
    return Path(d->path + str, d->separator.first());
}

Path Path::operator+(char const *nullTerminatedCStr) const
{
    return Path(d->path + nullTerminatedCStr, d->separator.first());
}

int Path::segmentCount() const
{
    d->parse();
    return d->segmentCount;
}

Path::Segment const &Path::segment(int index) const
{
    return reverseSegment(segmentCount() - 1 - index);
}

Path::Segment const &Path::reverseSegment(int reverseIndex) const
{
    d->parse();

    if (reverseIndex < 0 || reverseIndex >= d->segmentCount)
    {
        /// @throw OutOfBoundsError  Attempt to reference a nonexistent segment.
        throw OutOfBoundsError("Path::reverseSegment",
                               stringf("Reverse index %i is out of bounds", reverseIndex));
    }

    // Is this in the static buffer?
    if (reverseIndex < SEGMENT_BUFFER_SIZE)
    {
        return d->segments[reverseIndex];
    }

    // No - an extra segment.
    return d->extraSegments[reverseIndex - SEGMENT_BUFFER_SIZE];
}

Path Path::subPath(Rangei const &range) const
{
    if (range.isEmpty())
    {
        return Path("", d->separator.first());
    }
    Path sub(String(segment(range.start)), d->separator.first());
    for (int i = range.start + 1; i < range.end; ++i)
    {
        sub = sub / segment(i);
    }
    return sub;
}

Path Path::beginningOmitted(int omittedSegmentCount) const
{
    return subPath({omittedSegmentCount, segmentCount()});
}

Path Path::endOmitted(int omittedSegmentCount) const
{
    return subPath({0, segmentCount() - omittedSegmentCount});
}

bool Path::operator == (Path const &other) const
{
    if (this == &other) return true;

    if (segmentCount() != other.segmentCount()) return false;

    // If the hashes are different, the segments can't be the same.
    for (int i = 0; i < d->segmentCount; ++i)
    {
        if (segment(i).hash() != other.segment(i).hash())
            return false;
    }

    // Probably the same, but we have to make sure by comparing
    // the textual segments.
    if (d->separator == other.d->separator)
    {
        // The same separators, do one string-based test.
        return !d->path.compareWithoutCase(other.d->path);
    }
    else
    {
        // Do a string-based test for each segment separately.
        for (int i = 0; i < d->segmentCount; ++i)
        {
            if (segment(i) != other.segment(i)) return false;
        }
        return true;
    }
}

bool Path::operator < (Path const &other) const
{
    if (d->separator == other.d->separator)
    {
        // The same separators, do one string-based test.
        return d->path.compareWithoutCase(other.d->path) < 0;
    }
    else
    {
        // Do a string-based test for each segment separately.
        for (int i = 0; i < d->segmentCount; ++i)
        {
            if (!(segment(i) < other.segment(i))) return false;
        }
        return true;
    }
}

Path Path::operator/(const Path &other) const
{
    // Unify the separators.
    String otherPath = other.d->path;
    if (other.d->separator != d->separator)
    {
        otherPath.replace(other.d->separator, d->separator);
    }

    const Char sep = d->separator.first();
    return Path(d->path.concatenatePath(otherPath, sep), sep);
}

Path Path::operator/(const String &other) const
{
    return *this / Path(other);
}

Path Path::operator/(const char *otherNullTerminatedUtf8) const
{
    return *this / Path(otherNullTerminatedUtf8, '/');
}

String Path::toString() const
{
    return d->path;
}

const char *Path::c_str() const
{
    return d->path.c_str();
}

Path::operator const char *() const
{
    return d->path;
}

bool Path::isEmpty() const
{
    return d->path.isEmpty();
}

bool Path::isAbsolute() const
{
    return !isEmpty() && !firstSegment().size();
}

int Path::length() const
{
    return d->path.size();
}

dsize Path::size() const
{
    return length();
}

Char Path::first() const
{
    return d->path.first();
}

Char Path::last() const
{
    return d->path.last();
}

Path &Path::clear()
{
    d->path.clear();
    d->clearSegments();
    return *this;
}

Path &Path::operator = (String const &newPath)
{
    set(newPath, '/');
    return *this;
}

Path &Path::set(String const &newPath, Char sep)
{
    d->path = newPath; // implicitly shared
    d->separator = String(1, sep);
    d->clearSegments();
    return *this;
}

Path Path::withSeparators(Char sep) const
{
    const Char curSep = d->separator.first();
    if (sep == curSep) return *this;

    String modPath = d->path;
    modPath.replace(d->separator, String(1, sep));
    return Path(modPath, sep);
}

Char Path::separator() const
{
    return d->separator.first();
}

void Path::addTerminatingSeparator()
{
    if (!isEmpty())
    {
        if (last() != d->separator.first())
        {
            d->path.append(d->separator);
            d->clearSegments();
        }
    }
}

String Path::fileName() const
{
    if (last() == d->separator.first()) return "";
    return lastSegment().toRange();
}

Block Path::toUtf8() const
{
    return d->path.toUtf8();
}

void Path::operator >> (Writer &to) const
{
    to << d->path << duint16(d->separator.first());
}

void Path::operator << (Reader &from)
{
    clear();

    Block b;
    duint16 sep;
    from >> b >> sep;
    set(String::fromUtf8(b), Char(sep));
}

String Path::normalizeString(String const &text, Char replaceWith)
{
    String result = text;
    if (replaceWith != '/')
    {
        result.replace('/', replaceWith);
    }
    if (replaceWith != '\\')
    {
        result.replace('\\', replaceWith);
    }
    return result;
}

Path Path::normalize(String const &text, Char replaceWith)
{
    return Path(normalizeString(text, replaceWith), replaceWith);
}

Path PathRef::toPath() const
{
    if (!segmentCount()) return Path(); // Empty.

    String composed = segment(0).toRange();
    for (int i = 1; i < segmentCount(); ++i)
    {
        composed += path().separator();
        composed += segment(i);
    }
    return Path(composed, path().separator());
}

} // namespace de

#if 0
#ifdef _DEBUG
#include <QDebug>

using namespace de;

static int Path_UnitTest()
{
    try
    {
        // Test emptiness.
        {
            Path p;
            DE_ASSERT(p == Path(""));
            DE_ASSERT(p.isEmpty());
            DE_ASSERT(p.segmentCount() == 1);
        }

        // Equality and copying.
        {
            Path a("some/thing");
            Path b("/other/thing");

            DE_ASSERT(a != b);

            Path c = a;
            DE_ASSERT(c == a);
            DE_ASSERT(c.segment(1).toString() == "thing");
            DE_ASSERT(c.reverseSegment(1).toString() == "some");

            b = a;
            DE_ASSERT(b == a);
            DE_ASSERT(b.segment(1).toString() == "thing");
            DE_ASSERT(b.reverseSegment(1).toString() == "some");
        }

        // Swapping.
        {
            Path a("a/b/c");
            Path b("d/e");

            DE_ASSERT(a.segmentCount() == 3);
            DE_ASSERT(a.reverseSegment(1).toString() == "b");

            std::swap(a, b);

            DE_ASSERT(a.segmentCount() == 2);
            DE_ASSERT(a.reverseSegment(1).toString() == "d");
            DE_ASSERT(b.segmentCount() == 3);
            DE_ASSERT(b.reverseSegment(1).toString() == "b");
        }

        // Test a Windows style path with a drive plus file path.
        {
            Path p("c:/something.ext");
            DE_ASSERT(p.segmentCount() == 2);

            DE_ASSERT(p.reverseSegment(0).length() == 13);
            DE_ASSERT(p.reverseSegment(0).toString() == "something.ext");

            DE_ASSERT(p.reverseSegment(1).length() == 2);
            DE_ASSERT(p.reverseSegment(1).toString() == "c:");
        }

        // Test a Unix style path with a zero-length root node name.
        {
            Path p("/something.ext");
            DE_ASSERT(p.segmentCount() == 2);

            DE_ASSERT(p.reverseSegment(0).length() == 13);
            DE_ASSERT(p.reverseSegment(0).toString() == "something.ext");

            DE_ASSERT(p.reverseSegment(1).length() == 0);
            DE_ASSERT(p.reverseSegment(1).toString() == "");
        }

        // Test a relative directory.
        {
            Path p("some/dir/structure/");
            DE_ASSERT(p.segmentCount() == 3);

            DE_ASSERT(p.reverseSegment(0).length() == 9);
            DE_ASSERT(p.reverseSegment(0).toString() == "structure");

            DE_ASSERT(p.reverseSegment(1).length() == 3);
            DE_ASSERT(p.reverseSegment(1).toString() == "dir");

            DE_ASSERT(p.reverseSegment(2).length() == 4);
            DE_ASSERT(p.reverseSegment(2).toString() == "some");
        }

        // Test the extra segments.
        {
            Path p("/30/29/28/27/26/25/24/23/22/21/20/19/18/17/16/15/14/13/12/11/10/9/8/7/6/5/4/3/2/1/0");
            DE_ASSERT(p.segmentCount() == 32);

            DE_ASSERT(p.reverseSegment(0).toString()  == "0");
            DE_ASSERT(p.reverseSegment(23).toString() == "23");
            DE_ASSERT(p.reverseSegment(24).toString() == "24");
            DE_ASSERT(p.reverseSegment(30).toString() == "30");
        }

        // Test separators.
        {
            Path a("a.b.c.d", '.');
            Path b("con-variable", '-');

            DE_ASSERT(a.segmentCount() == 4);
            DE_ASSERT(a.segment(1).toString() == "b");

            DE_ASSERT(b.segmentCount() == 2);
            DE_ASSERT(b.segment(0).toString() == "con");
            DE_ASSERT(b.segment(1).toString() == "variable");
        }

        // Test fileName().
        {
            Path p;
            Path a("hello");
            Path b("hello/world");
            Path c("hello/world/");
            Path d("hello/world/  ");

            /*
            qDebug() << p << "=>" << p.fileName();
            qDebug() << a << "=>" << a.fileName();
            qDebug() << b << "=>" << b.fileName();
            qDebug() << c << "=>" << c.fileName();
            qDebug() << d << "=>" << d.fileName();
            */

            DE_ASSERT(p.fileName() == String(p).fileName());
            DE_ASSERT(a.fileName() == String(a).fileName());
            DE_ASSERT(b.fileName() == String(b).fileName());
            DE_ASSERT(c.fileName() == String(c).fileName());
            DE_ASSERT(d.fileName() == String(d).fileName());
        }
    }
    catch (Error const &er)
    {
        qWarning() << er.asText();
        return false;
    }
    return true;
}

static int testResult = Path_UnitTest();

#endif // _DEBUG
#endif
