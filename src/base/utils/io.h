/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Mike Tzou (Chocobo1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#pragma once

#include <iterator>
#include <memory>

class QByteArray;
class QFileDevice;

namespace Utils
{
    namespace IO
    {
        // A wrapper class that satisfy LegacyOutputIterator requirement
        class FileDeviceOutputIterator
            : public std::iterator<std::output_iterator_tag, void, void, void, void>
        {
        public:
            explicit FileDeviceOutputIterator(QFileDevice &device, const int bufferSize = (4 * 1024));
            FileDeviceOutputIterator(const FileDeviceOutputIterator &other) = default;
            ~FileDeviceOutputIterator();

            // mimic std::ostream_iterator behavior
            FileDeviceOutputIterator &operator=(char c);
            // TODO: make these `constexpr` in C++17
            FileDeviceOutputIterator &operator*();
            FileDeviceOutputIterator &operator++();
            FileDeviceOutputIterator &operator++(int);

        private:
            QFileDevice *m_device;
            std::shared_ptr<QByteArray> m_buffer;
            int m_bufferSize;
        };
    }
}
