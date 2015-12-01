/*
 * MPD.h
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

#ifndef MPD_H_
#define MPD_H_

#include "../adaptative/playlist/AbstractPlaylist.hpp"
#include "../adaptative/StreamFormat.hpp"
#include "Profile.hpp"

namespace dash
{
    namespace mpd
    {
        using namespace adaptative::playlist;
        using namespace adaptative;

        class ProgramInformation;

        class MPD : public AbstractPlaylist
        {
            public:
                MPD(stream_t *, Profile);
                virtual ~MPD();

                Profile                         getProfile() const;
                virtual bool                    isLive() const;
                virtual void                    debug();

                static StreamFormat             mimeToFormat(const std::string &);

                Property<ProgramInformation *>      programInfo;

            private:
                Profile                             profile;
        };
    }
}
#endif /* MPD_H_ */
