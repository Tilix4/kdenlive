/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *                                                                         *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "speedjob.hpp"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "jobmanager.h"
#include "kdenlivesettings.h"
#include "project/clipstabilize.h"
#include "ui_scenecutdialog_ui.h"

#include <QInputDialog>
#include <QScopedPointer>
#include <klocalizedstring.h>

#include <mlt++/Mlt.h>

SpeedJob::SpeedJob(const QString &binId, double speed, const QString &destUrl)
    : MeltJob(binId, SPEEDJOB, false, -1, -1)
    , m_speed(speed)
    , m_destUrl(destUrl)
{
    m_requiresFilter = false;
}

const QString SpeedJob::getDescription() const
{
    return i18n("Change clip speed");
}

void SpeedJob::configureConsumer()
{
    m_consumer.reset(new Mlt::Consumer(*m_profile.get(), "xml", m_destUrl.toUtf8().constData()));
    m_consumer->set("terminate_on_pause", 1);
    m_consumer->set("title", "Speed Change");
    m_consumer->set("real_time", -KdenliveSettings::mltthreads());
}

void SpeedJob::configureProducer()
{
    if (!qFuzzyCompare(m_speed, 1.0)) {
        QString resource = m_producer->get("resource");
        m_producer.reset(new Mlt::Producer(*m_profile.get(), "timewarp", QStringLiteral("%1:%2").arg(m_speed).arg(resource).toUtf8().constData()));
    }
}

void SpeedJob::configureFilter() {}

// static
int SpeedJob::prepareJob(std::shared_ptr<JobManager> ptr, const std::vector<QString> &binIds, int parentId, QString undoString)
{
    // Show config dialog
    bool ok;
    int speed = QInputDialog::getInt(QApplication::activeWindow(), i18n("Clip Speed"), i18n("Percentage"), 100, -100000, 100000, 1, &ok);
    if (!ok) {
        return -1;
    }
    std::unordered_map<QString, QString> destinations; // keys are binIds, values are path to target files
    for (const auto &binId : binIds) {
        auto binClip = pCore->projectItemModel()->getClipByBinID(binId);
        // Filter several clips, destination points to a folder
        QString mltfile = QFileInfo(binClip->url()).absoluteFilePath() + QStringLiteral(".mlt");
        destinations[binId] = mltfile;
    }
    // Now we have to create the jobs objects. This is trickier than usual, since the parameters are differents for each job (each clip has its own
    // destination). We have to construct a lambda that does that.

    auto createFn = [ dest = std::move(destinations), fSpeed = speed / 100.0 ](const QString &id)
    {
        return std::make_shared<SpeedJob>(id, fSpeed, dest.at(id));
    };

    // We are now all set to create the job. Note that we pass all the parameters directly through the lambda, hence there are no extra parameters to the
    // function
    using local_createFn_t = std::function<std::shared_ptr<SpeedJob>(const QString &)>;
    return ptr->startJob<SpeedJob>(binIds, parentId, std::move(undoString), local_createFn_t(std::move(createFn)));
}

bool SpeedJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    m_resultConsumed = true;
    if (!m_successful) {
        return false;
    }

    auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);

    // We store the stabilized clips in a sub folder with this name
    const QString folderName(i18n("Speed Change"));

    QString folderId = QStringLiteral("-1");
    bool found = false;
    // We first try to see if it exists
    auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
    for (int i = 0; i < containingFolder->childCount(); ++i) {
        auto currentItem = std::static_pointer_cast<AbstractProjectItem>(containingFolder->child(i));
        if (currentItem->itemType() == AbstractProjectItem::FolderItem && currentItem->name() == folderName) {
            found = true;
            folderId = currentItem->clipId();
            break;
        }
    }

    if (!found) {
        // if it was not found, we create it
        pCore->projectItemModel()->requestAddFolder(folderId, folderName, binClip->parent()->clipId(), undo, redo);
    }

    auto id = ClipCreator::createClipFromFile(m_destUrl, folderId, pCore->projectItemModel(), undo, redo);
    return id != QStringLiteral("-1");
}
