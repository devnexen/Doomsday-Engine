/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Writer"
#include "de/String"
#include "de/Block"
#include "de/ISerializable"
#include "de/IOStream"
#include "de/FixedByteArray"
#include "de/ByteRefArray"
#include "de/ByteArrayFile"
#include "de/data/byteorder.h"
#include <QScopedPointer>

namespace de {

struct Writer::Instance
{
    const ByteOrder& convert;
    IByteArray* destination;
    IOStream* stream;
    IByteArray::Offset offset;
    const IByteArray::Offset fixedOffset;

    Instance(const ByteOrder& order, IByteArray* dest, IByteArray::Offset off)
        : convert(order), destination(dest), stream(0), offset(off), fixedOffset(0) {}

    Instance(const ByteOrder& order, IOStream* str)
        : convert(order), destination(0), stream(str), offset(0), fixedOffset(0)
    {
        destination = dynamic_cast<IByteArray*>(str);
        if(destination)
        {
            // The object that implements the stream may also implement a byte
            // array interface -- that provides us more freedom to write, so
            // prefer to use it instead.
            stream = 0;
        }
    }

    Instance(const Instance& other, const ByteOrder& order)
        : convert    (order            ),
          destination(other.destination),
          stream     (other.stream     ),
          offset     (other.offset     ),
          fixedOffset(other.fixedOffset)
    {}

    void write(const IByteArray::Byte* ptr, dsize size)
    {
        if(destination)
        {
            destination->set(fixedOffset + offset, ptr, size);
            offset += size;
        }
        else if(stream)
        {
            *stream << ByteRefArray(ptr, size);
        }
    }
};

Writer::Writer(IByteArray& destination, const ByteOrder& byteOrder, IByteArray::Offset offset)
    : d(new Instance(byteOrder, &destination, offset))
{}

Writer::Writer(IByteArray& destination, IByteArray::Offset offset)
    : d(new Instance(littleEndianByteOrder, &destination, offset))
{}

Writer::Writer(IOStream& stream, const ByteOrder& byteOrder)
    : d(new Instance(byteOrder, &stream))
{}

Writer::Writer(ByteArrayFile& destination, const ByteOrder& byteOrder, IByteArray::Offset offset)
    : d(new Instance(byteOrder, static_cast<IByteArray*>(&destination), offset))
{}

Writer::Writer(const Writer& other, const ByteOrder& byteOrder)
    : d(new Instance(*other.d, byteOrder))
{}

Writer::~Writer()
{
    delete d;
}

Writer& Writer::operator << (const char& byte)
{
    d->write(reinterpret_cast<const IByteArray::Byte*>(&byte), 1);
    return *this;
}

Writer& Writer::operator << (const dchar& byte)
{
    d->write(reinterpret_cast<const IByteArray::Byte*>(&byte), 1);
    return *this;
}

Writer& Writer::operator << (const duchar& byte)
{
    d->write(&byte, 1);
    return *this;
}

Writer& Writer::operator << (const dint16& word)
{
    return *this << static_cast<duint16>(word);
}   

Writer& Writer::operator << (const duint16& word)
{
    duint16 netWord;
    d->convert.nativeToForeign(word, netWord);
    d->write(reinterpret_cast<IByteArray::Byte*>(&netWord), 2);
    return *this;
}

Writer& Writer::operator << (const dint32& dword)
{
    return *this << static_cast<duint32>(dword);
}   

Writer& Writer::operator << (const duint32& dword)
{
    duint32 netDword;
    d->convert.nativeToForeign(dword, netDword);
    d->write(reinterpret_cast<IByteArray::Byte*>(&netDword), 4);
    return *this;
}

Writer& Writer::operator << (const dint64& qword)
{
    return *this << static_cast<duint64>(qword);
}   

Writer& Writer::operator << (const duint64& qword)
{
    duint64 netQword;
    d->convert.nativeToForeign(qword, netQword);
    d->write(reinterpret_cast<IByteArray::Byte*>(&netQword), 8);
    return *this;
}

Writer& Writer::operator << (const dfloat& value)
{
    return *this << *reinterpret_cast<const duint32*>(&value);
}

Writer& Writer::operator << (const ddouble& value)
{
    return *this << *reinterpret_cast<const duint64*>(&value);
}

Writer& Writer::operator << (const String& text)
{
    Block bytes = text.toUtf8();

    // First write the length of the text.
    duint size = bytes.size();
    *this << size;

    d->write(bytes.data(), size);
    return *this;
}

Writer& Writer::operator << (const IByteArray& byteArray)
{
    // First write the length of the array.
    return *this << duint32(byteArray.size()) << FixedByteArray(byteArray);
}

Writer& Writer::operator << (const FixedByteArray& fixedByteArray)
{
    /**
     * @note  A copy of @a fixedByteArray is done because there is no certainty
     * that the source data actually exists anywhere. The object implementing
     * IByteArray could be generating it on the fly.
     */
    
    // Read the entire contents of the array.
    const dsize size = fixedByteArray.size();
    QScopedPointer<IByteArray::Byte> data(new IByteArray::Byte[size]);
    fixedByteArray.get(0, data.data(), size);
    d->write(data.data(), size);
    return *this;
}

Writer& Writer::operator << (const Block& block)
{
    // First write the length of the block.
    duint size = block.size();
    *this << size;

    d->write(block.data(), size);
    return *this;
}

Writer& Writer::operator << (const IWritable& writable)
{
    writable >> *this;
    return *this;
}

const IByteArray* Writer::destination() const
{
    return d->destination;
}

IByteArray* Writer::destination()
{
    return d->destination;
}

IByteArray::Offset Writer::offset() const
{
    return d->offset;
}

void Writer::setOffset(IByteArray::Offset offset)
{
    if(d->stream)
    {
        throw SeekError("Writer::setOffset", "Cannot change offset when writing to a stream");
    }

    d->offset = offset;
}

const ByteOrder& Writer::byteOrder() const
{
    return d->convert;
}

void Writer::seek(dint count)
{
    if(d->stream)
    {
        throw SeekError("Writer::seek", "Cannot seek when writing to a stream");
    }

    if(dint(d->fixedOffset + d->offset) + count < 0)
    {
        throw IByteArray::OffsetError("Writer::seek", "Seek past beginning of destination");
    }
    d->offset += count;
}

} // namespace de
