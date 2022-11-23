/*
BSD 3-Clause License

Copyright (c) 2021, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <vector>
#include <unordered_map>
#include <iostream>
#include "types.h"

namespace pcm {

constexpr auto SPR_PCU_BOX_TYPE = 4U;
constexpr auto SPR_MDF_BOX_TYPE = 11U;

class UncorePMUDiscovery
{
public:
    enum accessTypeEnum {
        MSR = 0,
        MMIO = 1,
        PCICFG = 2,
        unknownAccessType = 255
    };
    static const char * accessTypeStr(const uint64 t)
    {
        switch (t)
        {
            case MSR:
                return "MSR";
            case MMIO:
                return "MMIO";
            case PCICFG:
                return "PCICFG";
        }
        return "unknown";
    };
protected:
    struct GlobalPMU
    {
        uint64 type:8;
        uint64 stride:8;
        uint64 maxUnits:10;
        uint64 __reserved1:36;
        uint64 accessType:2;
        uint64 globalCtrlAddr;
        uint64 statusOffset:8;
        uint64 numStatus:16;
        uint64 __reserved2:40;
        void print() const
        {
            std::cout << "global PMU " <<
            " of type " << std::dec << type <<
            " globalCtrl: 0x" << std::hex << globalCtrlAddr <<
            " with access type " << std::dec << accessTypeStr(accessType) <<
            " stride: " << std::dec <<  stride
            << "\n";
        }
    };
    struct BoxPMU
    {
        uint64 numRegs : 8;
        uint64 ctrlOffset : 8;
        uint64 bitWidth : 8;
        uint64 ctrOffset : 8;
        uint64 statusOffset : 8;
        uint64 __reserved1 : 22;
        uint64 accessType : 2;
        uint64 boxCtrlAddr;
        uint64 boxType : 16;
        uint64 boxID : 16;
        uint64 __reserved2 : 32;
        void print() const
        {
           std::cout << "unit PMU " <<
                " of type " << std::dec <<  boxType <<
                " ID " << boxID <<
                " box ctrl: 0x" << std::hex << boxCtrlAddr <<
                " width " <<  std::dec << bitWidth <<
                " with access type " << accessTypeStr(accessType) <<
                " numRegs " << numRegs <<
                " ctrlOffset " << ctrlOffset <<
                " ctrOffset " << ctrOffset <<
                "\n";
        }
    };
    typedef std::vector<BoxPMU> BoxPMUs;
    typedef std::unordered_map<size_t, BoxPMUs> BoxPMUMap; // boxType -> BoxPMUs
    std::vector<BoxPMUMap> boxPMUs;
    std::vector<GlobalPMU> globalPMUs;

    bool validBox(const size_t boxType, const size_t socket, const size_t pos)
    {
        return socket < boxPMUs.size() && pos < boxPMUs[socket][boxType].size();
    }
    size_t registerStep(const size_t boxType, const size_t socket, const size_t pos)
    {
        const auto width = boxPMUs[socket][boxType][pos].bitWidth;
        switch (boxPMUs[socket][boxType][pos].accessType)
        {
        case MSR:
            if (width <= 64)
            {
                return 1;
            }
            break;
        case PCICFG:
        case MMIO:
            if (width <= 8)
            {
                return 1;
            }
            else if (width <= 16)
            {
                return 2;
            }
            else if (width <= 32)
            {
                return 4;
            }
            else if (width <= 64)
            {
                return 8;
            }
            break;
        }
        return 0;
    }
public:
    UncorePMUDiscovery();

    size_t getNumBoxes(const size_t boxType, const size_t socket)
    {
        if (socket < boxPMUs.size())
        {
            return boxPMUs[socket][boxType].size();
        }
        return 0;
    }

    uint64 getBoxCtlAddr(const size_t boxType, const size_t socket, const size_t pos)
    {
        if (validBox(boxType, socket, pos))
        {
            return boxPMUs[socket][boxType][pos].boxCtrlAddr;
        }
        return 0;
    }

    uint64 getBoxCtlAddr(const size_t boxType, const size_t socket, const size_t pos, const size_t c)
    {
        if (validBox(boxType, socket, pos) && c < boxPMUs[socket][boxType][pos].numRegs)
        {
            return boxPMUs[socket][boxType][pos].boxCtrlAddr + boxPMUs[socket][boxType][pos].ctrlOffset + c * registerStep(boxType, socket, pos);
        }
        return 0;
    }

    uint64 getBoxCtrAddr(const size_t boxType, const size_t socket, const size_t pos, const size_t c)
    {
        if (validBox(boxType, socket, pos) && c < boxPMUs[socket][boxType][pos].numRegs)
        {
            return boxPMUs[socket][boxType][pos].boxCtrlAddr + boxPMUs[socket][boxType][pos].ctrOffset + c * registerStep(boxType, socket, pos);
        }
        return 0;
    }

    accessTypeEnum getBoxAccessType(const size_t boxType, const size_t socket, const size_t pos)
    {
        if (validBox(boxType, socket, pos))
        {
            return static_cast<accessTypeEnum>(boxPMUs[socket][boxType][pos].accessType);
        }
        return unknownAccessType;
    }

    uint64 getBoxNumRegs(const size_t boxType, const size_t socket, const size_t pos)
    {
        if (validBox(boxType, socket, pos))
        {
            return boxPMUs[socket][boxType][pos].numRegs;
        }
        return 0;
    }
};

} // namespace pcm