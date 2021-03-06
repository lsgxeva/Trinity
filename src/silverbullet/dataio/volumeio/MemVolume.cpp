#include <sstream>

#include "silverbullet/dataio/volumeio/MemVolume.h"
#include "silverbullet/dataio/volumeio/VolumeError.h"
#include "silverbullet/io/FileTools.h"


namespace DataIO {
  namespace VolumeIO {

    MemVolume::MemVolume(VolumeMetadataPtr data) :
    Volume(data)
    {
    }
    
    void MemVolume::create() {
      uint64_t iLength = m_metadata->getSize().volume()*m_metadata->getComponents()*m_metadata->getType().m_bytes;
      assert(size_t(iLength) == iLength);
      m_volume.resize(size_t(iLength));
      m_dataState = DS_CanReadWrite;
    }
    
    uint64_t MemVolume::positionToIndex(Core::Math::Vec3ui64 pos, uint64_t c) const {
      const Core::Math::Vec3ui64& s = m_metadata->getSize();
      const uint64_t compCount = m_metadata->getComponents();
      
      return c+compCount*(pos.x+pos.y*s.x+pos.z*s.x*s.y);
    }
    
  }
}

/*
 The MIT License
 
 Copyright (c) 2014 HPC Group, Univeristy Duisburg-Essen
 
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 */
