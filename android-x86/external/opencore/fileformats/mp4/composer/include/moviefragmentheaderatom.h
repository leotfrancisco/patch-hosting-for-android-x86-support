/* ------------------------------------------------------------------
 * Copyright (C) 2008 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef __MovieFragmentHeaderAtom_H__
#define __MovieFragmentHeaderAtom_H__

#include "fullatom.h"
#include "a_atomdefs.h"
#include "atomutils.h"

class PVA_FF_MovieFragmentHeaderAtom : public PVA_FF_FullAtom
{

    public:
        PVA_FF_MovieFragmentHeaderAtom(uint32 sequenceNumber);

        virtual	~PVA_FF_MovieFragmentHeaderAtom();

        uint32 getSequenceNumber();

        virtual bool renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP* fp);

    private:

        uint32	_sequenceNumber;

        virtual void recomputeSize();
};

#endif
