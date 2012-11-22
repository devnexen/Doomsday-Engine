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

#include "de/Reader"
#include "de/String"
#include "de/Block"
#include "de/ISerializable"
#include "de/IIStream"
#include "de/FixedByteArray"
#include "de/ByteRefArray"
#include "de/data/byteorder.h"

#include <QTextStream>
#include <cstring>

namespace de {

struct Reader::Instance
{
    const ByteOrder& convert;

    // Random access source:
    const IByteArray* source;
    IByteArray::Offset offset;
    IByteArray::Offset markOffset;

    // Stream source:
    IIStream* stream;
    const IIStream* constStream;
    dsize numReceivedBytes;
    Block incoming;     ///< Buffer for bytes received so far from the stream.
    bool marking;       ///< @c true, if marking is occurring (mark() called).
    Block markedData;   ///< All read data since the mark was set.

    Instance(const ByteOrder& order, const IByteArray* src, IByteArray::Offset off)
        : convert(order), source(src), offset(off), markOffset(off),
          stream(0), constStream(0), numReceivedBytes(0), marking(false)
    {}

    Instance(const ByteOrder& order, IIStream* str)
        : convert(order), source(0), offset(0), markOffset(0),
          stream(str), constStream(0), numReceivedBytes(0), marking(false)
    {}

    Instance(const ByteOrder& order, const IIStream* str)
        : convert(order), source(0), offset(0), markOffset(0),
          stream(0), constStream(str), numReceivedBytes(0), marking(false)
    {}

    /**
     * Reads bytes from the stream and adds them to the incoming buffer.
     *
     * @param expectedSize  Number of bytes that the reader is expecting to read.
     */
    void update(dsize expectedSize = 0)
    {
        if(incoming.size() >= expectedSize)
        {
            // No need to update yet.
            return;
        }

        if(stream)
        {
            // Modifiable stream: read new bytes, append accumulation.
            Block b;
            *stream >> b;
            incoming += b;
        }
        else if(constStream)
        {
            Block b;
            *constStream >> b;
            // Immutable stream: append only bytes we haven't seen yet.
            b.remove(0, numReceivedBytes);
            incoming += b;
            numReceivedBytes += b.size();
        }
    }

    void readBytes(IByteArray::Byte* ptr, dsize size)
    {
        if(source)
        {
            source->get(offset, ptr, size);
            offset += size;
        }
        else if(stream || constStream)
        {
            update(size);
            if(incoming.size() >= size)
            {
                std::memcpy(ptr, incoming.constData(), size);
                if(marking)
                {
                    // We'll need this for rewinding a bit later.
                    markedData += incoming.left(size);
                }
                incoming.remove(0, size);
            }
            else
            {
                throw IIStream::InputError("Reader::readBytes",
                        QString("Attempted to read %1 bytes from stream while only %2 "
                                "bytes are available").arg(size).arg(incoming.size()));
            }
        }
    }

    void mark()
    {
        if(source)
        {
            markOffset = offset;
        }
        else
        {
            markedData.clear();
            marking = true;
        }
    }

    void rewind()
    {
        if(source)
        {
            offset = markOffset;
        }
        else
        {
            incoming.prepend(markedData);
            markedData.clear();
            marking = false;
        }
    }
};

Reader::Reader(const IByteArray& source, const ByteOrder& byteOrder, IByteArray::Offset offset)
    : d(new Instance(byteOrder, &source, offset))
{}

Reader::Reader(IIStream& stream, const ByteOrder& byteOrder)
    : d(new Instance(byteOrder, &stream))
{}

Reader::Reader(const IIStream& stream, const ByteOrder& byteOrder)
    : d(new Instance(byteOrder, &stream))
{}

Reader& Reader::operator >> (char& byte)
{
    return *this >> reinterpret_cast<duchar&>(byte);
}

Reader& Reader::operator >> (dchar& byte)
{
    return *this >> reinterpret_cast<duchar&>(byte);
}

Reader& Reader::operator >> (duchar& byte)
{
    d->readBytes(&byte, 1);
    return *this;
}

Reader& Reader::operator >> (dint16& word)
{
    return *this >> reinterpret_cast<duint16&>(word);
}

Reader& Reader::operator >> (duint16& word)
{
    d->readBytes(reinterpret_cast<IByteArray::Byte*>(&word), 2);
    d->convert.foreignToNative(word, word);
    return *this;
}

Reader& Reader::operator >> (dint32& dword)
{
    return *this >> reinterpret_cast<duint32&>(dword);
}

Reader& Reader::operator >> (duint32& dword)
{
    d->readBytes(reinterpret_cast<IByteArray::Byte*>(&dword), 4);
    d->convert.foreignToNative(dword, dword);
    return *this;
}

Reader& Reader::operator >> (dint64& qword)
{
    return *this >> reinterpret_cast<duint64&>(qword);
}

Reader& Reader::operator >> (duint64& qword)
{
    d->readBytes(reinterpret_cast<IByteArray::Byte*>(&qword), 8);
    d->convert.foreignToNative(qword, qword);
    return *this;
}

Reader& Reader::operator >> (dfloat& value)
{
    return *this >> *reinterpret_cast<duint32*>(&value);
}

Reader& Reader::operator >> (ddouble& value)
{
    return *this >> *reinterpret_cast<duint64*>(&value);
}

Reader& Reader::operator >> (String& text)
{
    duint size = 0;
    *this >> size;

    Block bytes;
    for(duint i = 0; i < size; ++i)
    {
        IByteArray::Byte ch = 0;
        *this >> ch;
        bytes.append(ch);
    }
    text = String::fromUtf8(bytes);
    
    return *this;
}

Reader& Reader::operator >> (IByteArray& byteArray)
{
    duint size = 0;
    *this >> size;

    /**
     * @note  A temporary copy of the contents of the array is made
     * because the destination byte array is not guaranteed to be
     * a memory buffer where you can copy the contents directly.
     */
    QScopedPointer<IByteArray::Byte> data(new IByteArray::Byte[size]);
    d->readBytes(data.data(), size);
    byteArray.set(0, data.data(), size);
    return *this;
}

Reader& Reader::operator >> (FixedByteArray& fixedByteArray)
{
    /**
     * @note  A temporary copy of the contents of the array is made
     * because the destination byte array is not guaranteed to be
     * a memory buffer where you can copy the contents directly.
     */
    const dsize size = fixedByteArray.size();
    QScopedPointer<IByteArray::Byte> data(new IByteArray::Byte[size]);
    d->readBytes(data.data(), size);
    fixedByteArray.set(0, data.data(), size);
    return *this;
}

Reader& Reader::operator >> (Block& block)
{
    duint size = 0;
    *this >> size;

    block.resize(size);
    d->readBytes(block.data(), size);

    return *this;
}

Reader& Reader::operator >> (IReadable& readable)
{
    readable << *this;
    return *this;
}

Reader& Reader::readUntil(IByteArray& byteArray, IByteArray::Byte delimiter)
{
    int pos = 0;
    IByteArray::Byte b = 0;
    do {
        *this >> b;
        byteArray.set(pos++, &b, 1);
    } while(b != delimiter);
    return *this;
}

const IByteArray* Reader::source() const
{
    return d->source;
}

IByteArray::Offset Reader::offset() const
{
    return d->offset;
}

void Reader::setOffset(IByteArray::Offset offset)
{
    d->offset = offset;
}

void Reader::seek(dint count)
{
    if(!d->source)
    {
        throw SeekError("Reader::seek", "Cannot seek when reading from a stream");
    }

    if(IByteArray::Offset(d->offset + count) >= d->source->size())
    {
        throw IByteArray::OffsetError("Reader::seek", "Seek past bounds of source data");
    }
    d->offset += count;
}

void Reader::mark()
{
    d->mark();
}

void Reader::rewind()
{
    d->rewind();
}

const ByteOrder& Reader::byteOrder() const
{
    return d->convert;
}

} // namespace de
