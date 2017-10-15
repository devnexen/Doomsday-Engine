/** @file remotefile.cpp  Remote file.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RemoteFile"
#include "de/RemoteFeedRelay"
#include "de/App"
#include "de/FileSystem"
#include "de/ScriptSystem"

namespace de {

static String const CACHE_PATH = "/home/cache/remote";

DENG2_PIMPL(RemoteFile)
{
    String remotePath;
    Block remoteMetaId;
    Block buffer;
    RemoteFeedRelay::FileContentsRequest fetching;
    SafePtr<File const> cachedFile;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        if (fetching)
        {
            fetching->cancel();
        }
    }

    String cachePath() const
    {
        String const hex = remoteMetaId.asHexadecimalText();
        return CACHE_PATH / String(hex.last()) / hex;
    }

    void findCachedFile(bool requireExists = true)
    {
        if (cachedFile) return;
        if (self().state() == NotReady)
        {
            throw UnfetchedError("RemoteFile::operator >>",
                                 self().description() + " not downloaded");
        }
        if (!cachedFile)
        {
            cachedFile.reset(FS::tryLocate<File const>(cachePath()));
        }
        if (requireExists && !cachedFile)
        {
            throw InputError("RemoteFile::operator >>",
                             self().description() + " has no locally cached data");
        }
    }
};

RemoteFile::RemoteFile(String const &name, String const &remotePath, Block const &remoteMetaId)
    : ByteArrayFile(name)
    , d(new Impl(this))
{
    objectNamespace().addSuperRecord(ScriptSystem::builtInClass(QStringLiteral("RemoteFile")));
    d->remotePath = remotePath;
    d->remoteMetaId = remoteMetaId;
    setState(NotReady);
}

void RemoteFile::fetchContents()
{
    DENG2_ASSERT(is<RemoteFeed>(originFeed()));

    if (state() != NotReady) return;

    setState(Recovering);

    d->findCachedFile(false /* doesn't have to exist */);
    if (d->cachedFile)
    {
        // There is a cached copy already.
        setState(Ready);
        reinterpret();
        return;
    }

    d->fetching = RemoteFeedRelay::get().fetchFileContents
            (originFeed()->as<RemoteFeed>().repository(),
             d->remotePath,
             [this] (duint64 startOffset, Block const &chunk, duint64 remainingBytes)
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        qDebug() << "[RemoteFile]" << d->remotePath << startOffset
                 << "remaining:" << remainingBytes;

        // Keep received data in a buffer.
        if (d->buffer.size() < remainingBytes)
        {
            d->buffer.resize(remainingBytes);
        }
        d->buffer.set(startOffset, chunk.data(), chunk.size());

        // When fully transferred, the file can be cached locally and interpreted.
        if (remainingBytes == 0)
        {
            qDebug() << "[RemoteFile] Complete contents received" << d->buffer.size();
            d->fetching = nullptr;

            String const fn = d->cachePath();
            Folder &cacheFolder = FS::get().makeFolder(fn.fileNamePath());
            File &data = cacheFolder.replaceFile(fn);
            data << d->buffer;
            data.flush();

            d->buffer.clear();
            d->cachedFile.reset(&data);
            setState(Ready);

            // Now this RemoteFile can become the source of an interpreted file,
            // which replaces the RemoteFile within the parent folder.
            reinterpret();
        }
    });
}

void RemoteFile::get(Offset at, Byte *values, Size count) const
{
    d->findCachedFile();
    if (auto *array = maybeAs<IByteArray>(d->cachedFile.get()))
    {
        array->get(at, values, count);
    }
    else
    {
        DENG2_ASSERT(false);
    }
}

void RemoteFile::set(Offset, Byte const *, Size)
{
    verifyWriteAccess(); // intended to throw exception
}

IIStream const &RemoteFile::operator >> (IByteArray &bytes) const
{
    d->findCachedFile();
    DENG2_ASSERT(d->cachedFile);
    *d->cachedFile >> bytes;
    return *this;
}

String RemoteFile::describe() const
{
    if (isReady())
    {
        return String("\"%1\"").arg(name());
    }
    return String("remote file \"%1\" (%2)")
            .arg(name())
            .arg(  state() == NotReady   ? "not ready"
                 : state() == Recovering ? "downloading"
                                         : "ready");
}

Block RemoteFile::metaId() const
{
    return d->remoteMetaId;
}

} // namespace de
