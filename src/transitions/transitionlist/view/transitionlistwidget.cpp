/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#include "transitionlistwidget.hpp"
#include "transitions/transitionlist/model/transitionfilter.hpp"
#include "../model/transitiontreemodel.hpp"
#include "transitions/transitionsrepository.hpp"
#include "effectslist/initeffects.h"
#include "dialogs/profilesdialog.h"

#include <QQmlContext>
#include <kns3/downloaddialog.h>

TransitionListWidget::TransitionListWidget(QWidget *parent)
    : AssetListWidget(parent)
{

    m_model = TransitionTreeModel::construct(true, this);

    m_proxyModel.reset(new TransitionFilter(this));
    m_proxyModel->setSourceModel(m_model.get());
    m_proxyModel->setSortRole(AssetTreeModel::NameRole);
    m_proxyModel->sort(0, Qt::AscendingOrder);

    m_proxy = new TransitionListWidgetProxy(this);
    rootContext()->setContextProperty("assetlist", m_proxy);
    rootContext()->setContextProperty("assetListModel", m_proxyModel.get());
    rootContext()->setContextProperty("isEffectList", false);
    m_assetIconProvider = new AssetIconProvider(false);

    setup();
}

TransitionListWidget::~TransitionListWidget()
{
    delete m_proxy;
    qDebug() << " - - -Deleting transition list widget";
}

QString TransitionListWidget::getMimeType(const QString &assetId) const
{
    if (TransitionsRepository::get()->isComposition(assetId)) {
        return QStringLiteral("kdenlive/composition");
    }
    return QStringLiteral("kdenlive/transition");
}

void TransitionListWidget::updateFavorite(const QModelIndex &index)
{
    m_proxyModel->dataChanged(index, index, QVector<int>());
    m_proxyModel->reloadFilterOnFavorite();
    emit reloadFavorites();
}

void TransitionListWidget::setFilterType(const QString &type)
{
    if (type == "favorites") {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(true, TransitionType::Favorites);
    } else {
        static_cast<TransitionFilter *>(m_proxyModel.get())->setFilterType(false, TransitionType::Favorites);
    }
}

int TransitionListWidget::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec() != 0) {
        entries = dialog->changedEntries();
    }
    for (const KNS3::Entry &entry : entries) {
        if (entry.status() == KNS3::Entry::Installed) {
            qCDebug(KDENLIVE_LOG) << "// Installed files: " << entry.installedFiles();
        }
    }
    delete dialog;
    return entries.size();
}

void TransitionListWidget::downloadNewLumas()
{
    if (getNewStuff(QStringLiteral(":data/kdenlive_wipes.knsrc")) > 0) {
        initEffects::refreshLumas();
        // TODO: refresh currently displayd trans ?
    }
}
