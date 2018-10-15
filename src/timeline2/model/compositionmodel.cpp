/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
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
#include "compositionmodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include "transitions/transitionsrepository.hpp"
#include "undohelper.hpp"
#include <QDebug>
#include <mlt++/MltTransition.h>
#include <utility>

CompositionModel::CompositionModel(std::weak_ptr<TimelineModel> parent, Mlt::Transition *transition, int id, const QDomElement &transitionXml,
                                   const QString &transitionId)
    : MoveableItem<Mlt::Transition>(std::move(parent), id)
    , AssetParameterModel(transition, transitionXml, transitionId, {ObjectType::TimelineComposition, m_id})
    , a_track(-1)
    , m_duration(0)
{
    m_compositionName = TransitionsRepository::get()->getName(transitionId);
}

int CompositionModel::construct(const std::weak_ptr<TimelineModel> &parent, const QString &transitionId, int id, Mlt::Properties *sourceProperties)
{
    Mlt::Transition *transition = TransitionsRepository::get()->getTransition(transitionId);
    transition->set_in_and_out(0, 0);
    auto xml = TransitionsRepository::get()->getXml(transitionId);
    if (sourceProperties) {
        // Paste parameters from existing source composition
        QDomNodeList params = xml.elementsByTagName(QStringLiteral("parameter"));
        for (int i = 0; i < params.count(); ++i) {
            QDomElement currentParameter = params.item(i).toElement();
            QString paramName = currentParameter.attribute(QStringLiteral("name"));
            QString paramValue = sourceProperties->get(paramName.toUtf8().constData());
            if (!paramValue.isEmpty()) {
                currentParameter.setAttribute(QStringLiteral("value"), paramValue);
            }
        }
    }
    std::shared_ptr<CompositionModel> composition(new CompositionModel(parent, transition, id, xml, transitionId));
    id = composition->m_id;

    if (auto ptr = parent.lock()) {
        ptr->registerComposition(composition);
    } else {
        qDebug() << "Error : construction of composition failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }

    return id;
}

bool CompositionModel::requestResize(int size, bool right, Fun &undo, Fun &redo, bool /*logUndo*/)
{
    QWriteLocker locker(&m_lock);
    if (size <= 0) {
        return false;
    }
    int delta = getPlaytime() - size;
    qDebug() << "compo request resize to " << size <<", ACTUAL SZ: "<<getPlaytime()<<", "<< right << delta;
    int in = getIn();
    int out = in + getPlaytime() - 1;
    int oldDuration = out - in;
    int old_in = in, old_out = out;
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }
    // if the in becomes negative, we add the necessary length in out.
    if (in < 0) {
        out = out - in;
        in = 0;
    }

    std::function<bool(void)> track_operation = []() { return true; };
    std::function<bool(void)> track_reverse = []() { return true; };
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            track_operation = ptr->getTrackById(m_currentTrackId)->requestCompositionResize_lambda(m_id, in, out);
        } else {
            qDebug() << "Error : Moving composition failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    } else {
        // Perform resize only
        setInOut(in, out);
    }
    Fun operation = [in, out, track_operation, this]() {
        if (track_operation()) {
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        auto ptr = m_parent.lock();
        // we send a list of roles to be updated
         QVector<int> roles{TimelineModel::DurationRole};
        if (!right) {
            roles.push_back(TimelineModel::StartRole);
        }
        if (m_currentTrackId != -1 && ptr) {
            QModelIndex ix = ptr->makeCompositionIndexFromID(m_id);
            //TODO: integrate in undo
            ptr->dataChanged(ix, ix, roles);
            track_reverse = ptr->getTrackById(m_currentTrackId)->requestCompositionResize_lambda(m_id, old_in, old_out);
        }
        Fun reverse = [old_in, old_out, track_reverse, this]() {
            if (track_reverse()) {
                return true;
            }
            return false;
        };

        auto kfr = getKeyframeModel();
        if (kfr) {
            // Adjust keyframe length
            kfr->resizeKeyframes(0, oldDuration, 0, out, undo, redo);
            Fun refresh = [kfr, this]() {
                kfr->modelChanged();
                return true;
            };
            refresh();
            UPDATE_UNDO_REDO(refresh, refresh, undo, redo);
        }
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

const QString CompositionModel::getProperty(const QString &name) const
{
    READ_LOCK();
    return QString::fromUtf8(service()->get(name.toUtf8().constData()));
}

Mlt::Transition *CompositionModel::service() const
{
    READ_LOCK();
    return static_cast<Mlt::Transition *>(m_asset.get());
}

Mlt::Properties *CompositionModel::properties()
{
    READ_LOCK();
    return new Mlt::Properties(m_asset.get()->get_properties());
}

int CompositionModel::getPlaytime() const
{
    READ_LOCK();
    return m_duration + 1;
}

int CompositionModel::getATrack() const
{
    READ_LOCK();
    return a_track == -1 ? -1 : service()->get_int("a_track");
}

void CompositionModel::setForceTrack(bool force)
{
    READ_LOCK();
    service()->set("force_track", force ? 1 : 0);
}

int CompositionModel::getForcedTrack() const
{
    QWriteLocker locker(&m_lock);
    return (service()->get_int("force_track") == 0 || a_track == -1) ? -1 : service()->get_int("a_track");
}

void CompositionModel::setATrack(int trackMltPosition, int trackId)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(trackId != getCurrentTrackId()); // can't compose with same track
    a_track = trackMltPosition;
    if (a_track >= 0) {
        service()->set("a_track", trackMltPosition);
    }
    if (m_currentTrackId != -1) {
        emit compositionTrackChanged();
    }
}

KeyframeModel *CompositionModel::getEffectKeyframeModel()
{
    if (getKeyframeModel()) {
        return getKeyframeModel()->getKeyModel();
    }
    return nullptr;
}

bool CompositionModel::showKeyframes() const
{
    READ_LOCK();
    return !service()->get_int("kdenlive:hide_keyframes");
}

void CompositionModel::setShowKeyframes(bool show)
{
    QWriteLocker locker(&m_lock);
    service()->set("kdenlive:hide_keyframes", (int)!show);
}

const QString &CompositionModel::displayName() const
{
    return m_compositionName;
}

void CompositionModel::setInOut(int in, int out)
{
    MoveableItem::setInOut(in, out);
    m_duration = out - in;
    setPosition(in);
}

void CompositionModel::setCurrentTrackId(int tid)
{
    MoveableItem::setCurrentTrackId(tid);
}

int CompositionModel::getOut() const
{
    return getPosition() + m_duration;
}

int CompositionModel::getIn() const
{
    return getPosition();
}
