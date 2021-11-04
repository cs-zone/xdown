/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Brian Kendall <brian@briankendall.net>
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

#include "macutilities.h"

#import <Cocoa/Cocoa.h>
#include <objc/message.h>

#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QString>
#include <QtMac>

namespace MacUtils
{
    QPixmap pixmapForExtension(const QString &ext, const QSize &size)
    {
        @autoreleasepool {
            NSImage *image = [[NSWorkspace sharedWorkspace] iconForFileType:ext.toNSString()];
            if (image) {
                NSRect rect = NSMakeRect(0, 0, size.width(), size.height());
                CGImageRef cgImage = [image CGImageForProposedRect:&rect context:nil hints:nil];
                return QtMac::fromCGImageRef(cgImage);
            }

            return QPixmap();
        }
    }

    void overrideDockClickHandler(bool (*dockClickHandler)(id, SEL, ...))
    {
        NSApplication *appInst = [NSApplication sharedApplication];

        if (!appInst)
            return;

        Class delClass = [[appInst delegate] class];
        SEL shouldHandle = sel_registerName("applicationShouldHandleReopen:hasVisibleWindows:");

        if (class_getInstanceMethod(delClass, shouldHandle)) {
            if (class_replaceMethod(delClass, shouldHandle, (IMP)dockClickHandler, "B@:"))
                qDebug("Registered dock click handler (replaced original method)");
            else
                qWarning("Failed to replace method for dock click handler");
        }
        else {
            if (class_addMethod(delClass, shouldHandle, (IMP)dockClickHandler, "B@:"))
                qDebug("Registered dock click handler");
            else
                qWarning("Failed to register dock click handler");
        }
    }

    void displayNotification(const QString &title, const QString &message)
    {
        @autoreleasepool {
            NSUserNotification *notification = [[NSUserNotification alloc] init];
            notification.title = title.toNSString();
            notification.informativeText = message.toNSString();
            notification.soundName = NSUserNotificationDefaultSoundName;

            [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
        }
    }

    void openFiles(const QSet<QString> &pathsList, bool isHttp)
    {
        @autoreleasepool {
            if(isHttp) {
                for (const auto &path : pathsList) {
                    if(path.length() < 1 ) continue;
                    NSURL *pathURL = [NSURL fileURLWithPath: path.toNSString()];
                    if(pathURL != NULL) {
                        [[NSWorkspace  sharedWorkspace] openURL:pathURL];
                        break;
                    }
                }
            } else {
                NSMutableArray *pathURLs = [NSMutableArray arrayWithCapacity:pathsList.size()];
                for (const auto &path : pathsList)
                    [pathURLs addObject:[NSURL fileURLWithPath:path.toNSString()]];
                [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:pathURLs];
            }
        }
    }
}
