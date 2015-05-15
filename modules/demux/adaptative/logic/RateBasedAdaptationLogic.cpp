/*
 * RateBasedAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "RateBasedAdaptationLogic.h"
#include "Representationselectors.hpp"

#include "../playlist/BaseRepresentation.h"
#include "../playlist/BasePeriod.h"

using namespace adaptative::logic;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (int w, int h) :
                          AbstractAdaptationLogic   (),
                          bpsAvg(0), bpsSamplecount(0),
                          currentBps(0)
{
    width  = w;
    height = h;
}

BaseRepresentation *RateBasedAdaptationLogic::getCurrentRepresentation(StreamType type, BasePeriod *period) const
{
    if(period == NULL)
        return NULL;

    RepresentationSelector selector;
    BaseRepresentation *rep = selector.select(period, type, currentBps, width, height);
    if ( rep == NULL )
    {
        rep = selector.select(period, type);
        if ( rep == NULL )
            return NULL;
    }
    return rep;
}

void RateBasedAdaptationLogic::updateDownloadRate(size_t size, mtime_t time)
{
    if(unlikely(time == 0))
        return;

    size_t current = size * 8000 / time;

    if (current >= bpsAvg)
        bpsAvg = bpsAvg + (current - bpsAvg) / (bpsSamplecount + 1);
    else
        bpsAvg = bpsAvg - (bpsAvg - current) / (bpsSamplecount + 1);

    bpsSamplecount++;

    if(bpsSamplecount % 5 == 0)
        currentBps = bpsAvg;
}

FixedRateAdaptationLogic::FixedRateAdaptationLogic(size_t bps) :
    AbstractAdaptationLogic()
{
    currentBps = bps;
}

BaseRepresentation *FixedRateAdaptationLogic::getCurrentRepresentation(StreamType type, BasePeriod *period) const
{
    if(period == NULL)
        return NULL;

    RepresentationSelector selector;
    BaseRepresentation *rep = selector.select(period, type, currentBps);
    if ( rep == NULL )
    {
        rep = selector.select(period, type);
        if ( rep == NULL )
            return NULL;
    }
    return rep;
}
