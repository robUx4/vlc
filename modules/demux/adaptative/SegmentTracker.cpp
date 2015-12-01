/*
 * SegmentTracker.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "SegmentTracker.hpp"
#include "playlist/AbstractPlaylist.hpp"
#include "playlist/BaseRepresentation.h"
#include "playlist/BaseAdaptationSet.h"
#include "playlist/Segment.h"
#include "playlist/SegmentChunk.hpp"
#include "logic/AbstractAdaptationLogic.h"

using namespace adaptative;
using namespace adaptative::logic;
using namespace adaptative::playlist;

SegmentTrackerEvent::SegmentTrackerEvent(SegmentChunk *s)
{
    type = DISCONTINUITY;
    u.discontinuity.sc = s;
}

SegmentTrackerEvent::SegmentTrackerEvent(BaseRepresentation *prev, BaseRepresentation *next)
{
    type = SWITCHING;
    u.switching.prev = prev;
    u.switching.next = next;
}

SegmentTrackerEvent::SegmentTrackerEvent(const StreamFormat *fmt)
{
    type = FORMATCHANGE;
    u.format.f = fmt;
}

SegmentTracker::SegmentTracker(AbstractAdaptationLogic *logic_, BaseAdaptationSet *adaptSet)
{
    first = true;
    count = 0;
    initializing = true;
    index_sent = false;
    init_sent = false;
    curRepresentation = NULL;
    setAdaptationLogic(logic_);
    adaptationSet = adaptSet;
    format = StreamFormat::UNSUPPORTED;
}

SegmentTracker::~SegmentTracker()
{
    reset();
}

void SegmentTracker::setAdaptationLogic(AbstractAdaptationLogic *logic_)
{
    logic = logic_;
    registerListener(logic);
}

void SegmentTracker::reset()
{
    notify(SegmentTrackerEvent(curRepresentation, NULL));
    curRepresentation = NULL;
    init_sent = false;
    index_sent = false;
    initializing = true;
    format = StreamFormat::UNSUPPORTED;
}

SegmentChunk * SegmentTracker::getNextChunk(bool switch_allowed, HTTPConnectionManager *connManager)
{
    BaseRepresentation *rep = NULL, *prevRep = NULL;
    ISegment *segment;

    if(!adaptationSet)
        return NULL;

    /* Ensure we don't keep chaining init/index without data */
    if( initializing )
    {
        if( curRepresentation )
            switch_allowed = false;
        else
            switch_allowed = true;
    }

    if( !switch_allowed ||
       (curRepresentation && curRepresentation->getSwitchPolicy() == SegmentInformation::SWITCH_UNAVAILABLE) )
        rep = curRepresentation;
    else
        rep = logic->getNextRepresentation(adaptationSet, curRepresentation);

    if ( rep == NULL )
            return NULL;


    if(rep != curRepresentation)
    {
        notify(SegmentTrackerEvent(curRepresentation, rep));
        prevRep = curRepresentation;
        curRepresentation = rep;
        init_sent = false;
        index_sent = false;
        initializing = true;
    }

    bool b_updated = false;
    /* Ensure ephemere content is updated/loaded */
    if(rep->needsUpdate())
        b_updated = rep->runLocalUpdates(getPlaybackTime(), count, false);

    if(prevRep && !rep->consistentSegmentNumber())
    {
        /* Convert our segment number */
        count = rep->translateSegmentNumber(count, prevRep);
    }
    else if(first && rep->getPlaylist()->isLive())
    {
        count = rep->getLiveStartSegmentNumber(count);
        first = false;
    }

    if(b_updated)
    {
        if(!rep->consistentSegmentNumber())
            curRepresentation->pruneBySegmentNumber(count);
        curRepresentation->scheduleNextUpdate(count);
    }

    if(!init_sent)
    {
        init_sent = true;
        segment = rep->getSegment(BaseRepresentation::INFOTYPE_INIT);
        if(segment)
            return segment->toChunk(count, rep, connManager);
    }

    if(!index_sent)
    {
        index_sent = true;
        segment = rep->getSegment(BaseRepresentation::INFOTYPE_INDEX);
        if(segment)
            return segment->toChunk(count, rep, connManager);
    }

    bool b_gap = false;
    segment = rep->getNextSegment(BaseRepresentation::INFOTYPE_MEDIA, count, &count, &b_gap);
    if(!segment)
    {
        reset();
        return NULL;
    }

    if(initializing)
    {
        b_gap = false;
        /* stop initializing after 1st chunk */
        initializing = false;
    }

    SegmentChunk *chunk = segment->toChunk(count, rep, connManager);

    /* We need to check segment/chunk format changes, as we can't rely on representation's (HLS)*/
    if(chunk && format != chunk->getStreamFormat())
    {
        format = chunk->getStreamFormat();
        notify(SegmentTrackerEvent(&format));
    }

    /* Handle both implicit and explicit discontinuities */
    if( (b_gap && count) || (chunk && chunk->discontinuity) )
    {
        notify(SegmentTrackerEvent(chunk));
    }

    if(chunk)
        count++;

    return chunk;
}

bool SegmentTracker::setPositionByTime(mtime_t time, bool restarted, bool tryonly)
{
    uint64_t segnumber;
    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);

    if(rep &&
       rep->getSegmentNumberByTime(time, &segnumber))
    {
        if(!tryonly)
            setPositionByNumber(segnumber, restarted);
        return true;
    }
    return false;
}

void SegmentTracker::setPositionByNumber(uint64_t segnumber, bool restarted)
{
    if(restarted)
    {
        initializing = true;
        index_sent = false;
        init_sent = false;
    }
    count = segnumber;
}

mtime_t SegmentTracker::getPlaybackTime() const
{
    if(curRepresentation)
        return curRepresentation->getPlaybackTimeBySegmentNumber(count);
    else
        return 0;
}

mtime_t SegmentTracker::getMinAheadTime() const
{
    BaseRepresentation *rep = curRepresentation;
    if(!rep)
        rep = logic->getNextRepresentation(adaptationSet, NULL);
    if(rep)
        return rep->getMinAheadTime(count);
    return 0;
}

void SegmentTracker::registerListener(SegmentTrackerListenerInterface *listener)
{
    listeners.push_back(listener);
}

void SegmentTracker::updateSelected()
{
    if(curRepresentation && curRepresentation->needsUpdate())
    {
        curRepresentation->runLocalUpdates(getPlaybackTime(), count, true);
        curRepresentation->scheduleNextUpdate(count);
    }
}

void SegmentTracker::notify(const SegmentTrackerEvent &event)
{
    std::list<SegmentTrackerListenerInterface *>::const_iterator it;
    for(it=listeners.begin();it != listeners.end(); ++it)
        (*it)->trackerEvent(event);
}
