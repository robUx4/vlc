/*
 * Segment.h
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

#ifndef SEGMENT_H_
#define SEGMENT_H_

#include <string>
#include <sstream>
#include <vector>
#include "ICanonicalUrl.hpp"
#include "../http/Chunk.h"
#include "../tools/Properties.hpp"

namespace adaptative
{
    namespace playlist
    {
        class BaseRepresentation;
        class SubSegment;

        using namespace http;

        class ISegment : public ICanonicalUrl
        {
            public:
                ISegment(const ICanonicalUrl *parent);
                virtual ~ISegment(){}
                /**
                 *  @return true if the segment should be dropped after being read.
                 *          That is basically true when using an Url, and false
                 *          when using an UrlTemplate
                 */
                virtual Chunk*                          toChunk         (size_t, BaseRepresentation * = NULL);
                virtual void                            setByteRange    (size_t start, size_t end);
                virtual size_t                          getOffset       () const;
                virtual std::vector<ISegment*>          subSegments     () = 0;
                virtual void                            addSubSegment   (SubSegment *) = 0;
                virtual void                            debug           (vlc_object_t *,int = 0) const;
                virtual bool                            contains        (size_t byte) const;
                int                                     getClassId      () const;
                Property<mtime_t>       startTime;
                Property<mtime_t>       duration;

                static const int CLASSID_ISEGMENT = 0;

            protected:
                size_t                  startByte;
                size_t                  endByte;
                std::string             debugName;
                int                     classId;

                class SegmentChunk : public Chunk
                {
                    public:
                        SegmentChunk(ISegment *segment, const std::string &url);
                        void setRepresentation(BaseRepresentation *);
                        virtual void onDownload(void *, size_t); // reimpl

                    protected:
                        ISegment *segment;
                        BaseRepresentation *rep;
                };

                virtual Chunk * getChunk(const std::string &);
                virtual void onChunkDownload(void *, size_t, Chunk *, BaseRepresentation *);
        };

        class Segment : public ISegment
        {
            public:
                Segment( ICanonicalUrl *parent );
                ~Segment();
                virtual void setSourceUrl( const std::string &url );
                virtual Url getUrlSegment() const; /* impl */
                virtual Chunk* toChunk(size_t, BaseRepresentation * = NULL);
                virtual std::vector<ISegment*> subSegments();
                virtual void debug(vlc_object_t *,int = 0) const;
                virtual void addSubSegment(SubSegment *);
                static const int CLASSID_SEGMENT = 1;

            protected:
                std::vector<SubSegment *> subsegments;
                std::string sourceUrl;
                int size;
        };

        class InitSegment : public Segment
        {
            public:
                InitSegment( ICanonicalUrl *parent );
                static const int CLASSID_INITSEGMENT = 2;
        };

        class IndexSegment : public Segment
        {
            public:
                IndexSegment( ICanonicalUrl *parent );
                static const int CLASSID_INDEXSEGMENT = 3;
        };

        class SubSegment : public ISegment
        {
            public:
                SubSegment(ISegment *, size_t start, size_t end);
                virtual Url getUrlSegment() const; /* impl */
                virtual std::vector<ISegment*> subSegments();
                virtual void addSubSegment(SubSegment *);
                static const int CLASSID_SUBSEGMENT = 4;
            private:
                ISegment *parent;
        };
    }
}

#endif /* SEGMENT_H_ */
