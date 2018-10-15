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
#include "clipmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "macros.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include <QDebug>
#include <mlt++/MltProducer.h>
#include <utility>

// this can be deleted
#include "bin/model/markerlistmodel.hpp"
#include "gentime.h"
#include <effects/effectsrepository.hpp>

ClipModel::ClipModel(std::shared_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod, const QString &binClipId, int id,
                     PlaylistState::ClipState state, double speed)
    : MoveableItem<Mlt::Producer>(parent, id)
    , m_producer(std::move(prod))
    , m_effectStack(EffectStackModel::construct(m_producer, {ObjectType::TimelineClip, m_id}, parent->m_undoStack))
    , m_binClipId(binClipId)
    , forceThumbReload(false)
    , m_currentState(state)
    , m_speed(speed)
{
    m_producer->set("kdenlive:id", binClipId.toUtf8().constData());
    m_producer->set("_kdenlive_cid", m_id);
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    m_canBeVideo = binClip->hasVideo();
    m_canBeAudio = binClip->hasAudio();
    m_clipType = binClip->clipType();
    if (binClip) {
        m_endlessResize = !binClip->hasLimitedDuration();
    } else {
        m_endlessResize = false;
    }
}

int ClipModel::construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, int id, PlaylistState::ClipState state)
{
    id = (id == -1 ? TimelineModel::getNextId() : id);
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binClipId);

    // We refine the state according to what the clip can actually produce
    std::pair<bool, bool> videoAudio = stateToBool(state);
    videoAudio.first = videoAudio.first && binClip->hasVideo();
    videoAudio.second = videoAudio.second && binClip->hasAudio();
    state = stateFromBool(videoAudio);

    std::shared_ptr<Mlt::Producer> cutProducer = binClip->getTimelineProducer(id, state, 1.);
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, cutProducer, binClipId, id, state));
    clip->setClipState_lambda(state)();
    parent->registerClip(clip);
    return id;
}

int ClipModel::construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, std::shared_ptr<Mlt::Producer> producer,
                         PlaylistState::ClipState state)
{

    // we hand the producer to the bin clip, and in return we get a cut to a good master producer
    // We might not be able to use directly the producer that we receive as an argument, because it cannot share the same master producer with any other
    // clipModel (due to a mlt limitation, see ProjectClip doc)

    int id = TimelineModel::getNextId();
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binClipId);

    // We refine the state according to what the clip can actually produce
    std::pair<bool, bool> videoAudio = stateToBool(state);
    videoAudio.first = videoAudio.first && binClip->hasVideo();
    videoAudio.second = videoAudio.second && binClip->hasAudio();
    state = stateFromBool(videoAudio);

    double speed = 1.0;
    if (QString::fromUtf8(producer->get("mlt_service")) == QLatin1String("timewarp")) {
        speed = producer->get_double("warp_speed");
    }
    auto result = binClip->giveMasterAndGetTimelineProducer(id, producer, state);
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, result.first, binClipId, id, state, speed));
    clip->m_effectStack->importEffects(producer, result.second);
    clip->setClipState_lambda(state)();
    parent->registerClip(clip);
    return id;
}

void ClipModel::registerClipToBin()
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    if (!binClip) {
        qDebug() << "Error : Bin clip for id: " << m_binClipId << " NOT AVAILABLE!!!";
    }
    qDebug() << "REGISTRATION " << m_id << "ptr count" << m_parent.use_count();
    binClip->registerTimelineClip(m_parent, m_id);
}

void ClipModel::deregisterClipToBin()
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    binClip->deregisterTimelineClip(m_id);
}

ClipModel::~ClipModel() {}

bool ClipModel::requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    // qDebug() << "RESIZE CLIP" << m_id << "target size=" << size << "right=" << right << "endless=" << m_endlessResize << "length" <<
    // m_producer->get_length();
    if (!m_endlessResize && (size <= 0 || size > m_producer->get_length())) {
        return false;
    }
    int delta = getPlaytime() - size;
    if (delta == 0) {
        return true;
    }
    int in = m_producer->get_in();
    int out = m_producer->get_out();
    int old_in = in, old_out = out;
    // check if there is enough space on the chosen side
    if (!right && in + delta < 0 && !m_endlessResize) {
        return false;
    }
    if (!m_endlessResize && right && out - delta >= m_producer->get_length()) {
        return false;
    }
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }
    // qDebug() << "Resize facts delta =" << delta << "old in" << old_in << "old_out" << old_out << "in" << in << "out" << out;
    std::function<bool(void)> track_operation = []() { return true; };
    std::function<bool(void)> track_reverse = []() { return true; };
    int outPoint = out;
    int inPoint = in;
    if (m_endlessResize) {
        outPoint = out - in;
        inPoint = 0;
    }
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            track_operation = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, inPoint, outPoint, right);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    } else {
        // Ensure producer is long enough
        if (m_endlessResize && outPoint > m_producer->parent().get_length()) {
            m_producer->set("length", outPoint + 1);
        }
    }
    Fun operation = [this, inPoint, outPoint, track_operation]() {
        if (track_operation()) {
            m_producer->set_in_and_out(inPoint, outPoint);
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        if (m_currentTrackId != -1) {
            QVector<int> roles{TimelineModel::DurationRole};
            if (!right) {
                roles.push_back(TimelineModel::StartRole);                roles.push_back(TimelineModel::InPointRole);
            } else {
                roles.push_back(TimelineModel::OutPointRole);
            }
            if (auto ptr = m_parent.lock()) {
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                //TODO: integrate in undo
                ptr->dataChanged(ix, ix, roles);
                track_reverse = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, old_in, old_out, right);
            }
        }
        Fun reverse = [this, old_in, old_out, track_reverse]() {
            if (track_reverse()) {
                m_producer->set_in_and_out(old_in, old_out);
                return true;
            }
            return false;
        };
        qDebug() << "// ADJUSTING EFFECT LENGTH, LOGUNDO " << logUndo << ", " << old_in << "/" << inPoint << ", " << m_producer->get_playtime();
        if (logUndo) {
            // adjustEffectLength(right, old_in, inPoint, oldDuration, m_producer->get_playtime(), reverse, operation, logUndo);
        }
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

const QString ClipModel::getProperty(const QString &name) const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return QString::fromUtf8(service()->parent().get(name.toUtf8().constData()));
    }
    return QString::fromUtf8(service()->get(name.toUtf8().constData()));
}

int ClipModel::getIntProperty(const QString &name) const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return service()->parent().get_int(name.toUtf8().constData());
    }
    return service()->get_int(name.toUtf8().constData());
}

QSize ClipModel::getFrameSize() const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return QSize(service()->parent().get_int("meta.media.width"), service()->parent().get_int("meta.media.height"));
    }
    return QSize(service()->get_int("meta.media.width"), service()->get_int("meta.media.height"));
}

double ClipModel::getDoubleProperty(const QString &name) const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return service()->parent().get_double(name.toUtf8().constData());
    }
    return service()->get_double(name.toUtf8().constData());
}

Mlt::Producer *ClipModel::service() const
{
    READ_LOCK();
    return m_producer.get();
}

int ClipModel::getPlaytime() const
{
    READ_LOCK();
    return m_producer->get_playtime();
}

void ClipModel::setTimelineEffectsEnabled(bool enabled)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->setEffectStackEnabled(enabled);
}

bool ClipModel::addEffect(const QString &effectId)
{
    QWriteLocker locker(&m_lock);
    if (EffectsRepository::get()->getType(effectId) == EffectType::Audio) {
        if (m_currentState == PlaylistState::VideoOnly) {
            return false;
        }
    } else if (m_currentState == PlaylistState::AudioOnly) {
        return false;
    }
    m_effectStack->appendEffect(effectId);
    return true;
}

bool ClipModel::copyEffect(std::shared_ptr<EffectStackModel> stackModel, int rowId)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->copyEffect(stackModel->getEffectStackRow(rowId));
    return true;
}

bool ClipModel::importEffects(std::shared_ptr<EffectStackModel> stackModel)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->importEffects(stackModel);
    return true;
}

bool ClipModel::importEffects(std::weak_ptr<Mlt::Service> service)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->importEffects(service);
    return true;
}

bool ClipModel::removeFade(bool fromStart)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->removeFade(fromStart);
    return true;
}

bool ClipModel::adjustEffectLength(bool adjustFromEnd, int oldIn, int newIn, int oldDuration, int duration, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    return m_effectStack->adjustStackLength(adjustFromEnd, oldIn, oldDuration, newIn, duration, undo, redo, logUndo);
}

bool ClipModel::adjustEffectLength(const QString &effectName, int duration, int originalDuration, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    qDebug() << ".... ADJUSTING FADE LENGTH: " << duration << " / " << effectName;
    Fun operation = [this, duration, effectName]() {
        return m_effectStack->adjustFadeLength(duration, effectName == QLatin1String("fadein") || effectName == QLatin1String("fade_to_black"), audioEnabled(),
                                               !isAudioOnly());
    };
    if (operation() && originalDuration > 0) {
        Fun reverse = [this, originalDuration, effectName]() {
            return m_effectStack->adjustFadeLength(originalDuration, effectName == QLatin1String("fadein") || effectName == QLatin1String("fade_to_black"),
                                                   audioEnabled(), !isAudioOnly());
        };
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return true;
}

bool ClipModel::audioEnabled() const
{
    READ_LOCK();
    return stateToBool(m_currentState).second;
}

bool ClipModel::isAudioOnly() const
{
    READ_LOCK();
    return m_currentState == PlaylistState::AudioOnly;
}

void ClipModel::refreshProducerFromBin(PlaylistState::ClipState state, double speed)
{
    // We require that the producer is not in the track when we refresh the producer, because otherwise the modification will not be propagated. Remove the clip
    // first, refresh, and then replant.
    Q_ASSERT(m_currentTrackId == -1);
    QWriteLocker locker(&m_lock);
    int in = getIn();
    int out = getOut();
    qDebug() << "refresh " << speed << m_speed << in << out;
    if (!qFuzzyCompare(speed, m_speed) && !qFuzzyCompare(speed, 0.)) {
        in = in * m_speed / speed;
        out = in + getPlaytime() - 1;
        // prevent going out of the clip's range
        out = std::min(out, int(double(m_producer->get_length()) * m_speed / speed) - 1);
        m_speed = speed;
        qDebug() << "changing speed" << in << out << m_speed;
    }
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    std::shared_ptr<Mlt::Producer> binProducer = binClip->getTimelineProducer(m_id, state, m_speed);
    m_producer = std::move(binProducer);
    m_producer->set_in_and_out(in, out);
    // replant effect stack in updated service
    m_effectStack->resetService(m_producer);
    m_producer->set("kdenlive:id", binClip->AbstractProjectItem::clipId().toUtf8().constData());
    m_producer->set("_kdenlive_cid", m_id);
    m_endlessResize = !binClip->hasLimitedDuration();
}

void ClipModel::refreshProducerFromBin()
{
    refreshProducerFromBin(m_currentState);
}

bool ClipModel::useTimewarpProducer(double speed, Fun &undo, Fun &redo)
{
    if (m_endlessResize) {
        // no timewarp for endless producers
        return false;
    }
    if (qFuzzyCompare(speed, m_speed)) {
        // nothing to do
        return true;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    double previousSpeed = getSpeed();
    int oldDuration = getPlaytime();
    int newDuration = int(double(oldDuration) * previousSpeed / speed);
    int oldOut = getOut();
    int oldIn = getIn();
    auto operation = useTimewarpProducer_lambda(speed);
    auto reverse = useTimewarpProducer_lambda(previousSpeed);
    if (oldOut >= newDuration) {
        // in that case, we are going to shrink the clip when changing the producer. We must undo that when reloading the old producer
        reverse = [reverse, oldIn, oldOut, this]() {
            bool res = reverse();
            if (res) {
                setInOut(oldIn, oldOut);
            }
            return res;
        };
    }
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        bool res = requestResize(newDuration, true, local_undo, local_redo, true);
        if (!res) {
            local_undo();
            return false;
        }
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    qDebug() << "tw: operation fail";
    return false;
}

Fun ClipModel::useTimewarpProducer_lambda(double speed)
{
    QWriteLocker locker(&m_lock);
    return [speed, this]() {
        qDebug() << "timeWarp producer" << speed;
        refreshProducerFromBin(m_currentState, speed);
        if (auto ptr = m_parent.lock()) {
            QModelIndex ix = ptr->makeClipIndexFromID(m_id);
            ptr->notifyChange(ix, ix, TimelineModel::SpeedRole);
        }
        return true;
    };
}

QVariant ClipModel::getAudioWaveform()
{
    READ_LOCK();
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    if (binClip) {
        return QVariant::fromValue(binClip->audioFrameCache);
    }
    return QVariant();
}

const QString &ClipModel::binId() const
{
    return m_binClipId;
}

std::shared_ptr<MarkerListModel> ClipModel::getMarkerModel() const
{
    READ_LOCK();
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->getMarkerModel();
}

int ClipModel::fadeIn() const
{
    return m_effectStack->getFadePosition(true);
}

int ClipModel::fadeOut() const
{
    return m_effectStack->getFadePosition(false);
}

double ClipModel::getSpeed() const
{
    return m_speed;
}

KeyframeModel *ClipModel::getKeyframeModel()
{
    return m_effectStack->getEffectKeyframeModel();
}

bool ClipModel::showKeyframes() const
{
    READ_LOCK();
    return !service()->get_int("kdenlive:hide_keyframes");
}

void ClipModel::setShowKeyframes(bool show)
{
    QWriteLocker locker(&m_lock);
    service()->set("kdenlive:hide_keyframes", (int)!show);
}

Fun ClipModel::setClipState_lambda(PlaylistState::ClipState state)
{
    QWriteLocker locker(&m_lock);
    return [this, state]() {
        if (auto ptr = m_parent.lock()) {
            switch (state) {
            case PlaylistState::Disabled:
                m_producer->set("set.test_audio", 1);
                m_producer->set("set.test_image", 1);
                break;
            case PlaylistState::VideoOnly:
                m_producer->set("set.test_image", 0);
                break;
            case PlaylistState::AudioOnly:
                m_producer->set("set.test_audio", 0);
                break;
            default:
                // error
                break;
            }
            m_currentState = state;
            if (ptr->isClip(m_id)) { // if this is false, the clip is being created. Don't update model in that case
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                ptr->dataChanged(ix, ix, {TimelineModel::StatusRole});
            }
            return true;
        }
        return false;
    };
}

bool ClipModel::setClipState(PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    if (state == PlaylistState::VideoOnly && !canBeVideo()) {
        return false;
    }
    if (state == PlaylistState::AudioOnly && !canBeAudio()) {
        return false;
    }
    if (state == m_currentState) {
        return true;
    }
    auto old_state = m_currentState;
    auto operation = setClipState_lambda(state);
    if (operation()) {
        auto reverse = setClipState_lambda(old_state);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

PlaylistState::ClipState ClipModel::clipState() const
{
    READ_LOCK();
    return m_currentState;
}

ClipType::ProducerType ClipModel::clipType() const
{
    READ_LOCK();
    return m_clipType;
}

void ClipModel::passTimelineProperties(std::shared_ptr<ClipModel> other)
{
    READ_LOCK();
    Mlt::Properties source(m_producer->get_properties());
    Mlt::Properties dest(other->service()->get_properties());
    dest.pass_list(source, "kdenlive:hide_keyframes,kdenlive:activeeffect");
}

bool ClipModel::canBeVideo() const
{
    return m_canBeVideo;
}

bool ClipModel::canBeAudio() const
{
    return m_canBeAudio;
}

const QString ClipModel::effectNames() const
{
    READ_LOCK();
    return m_effectStack->effectNames();
}
