/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


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

/**
  \file    BOVConverter.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef BOVCONVERTER_H
#define BOVCONVERTER_H

#include "RAWConverter.h"

class BOVConverter : public RAWConverter {
public:
  BOVConverter();
  virtual ~BOVConverter() {}

  virtual bool ConvertToRAW(const std::string& strSourceFilename,
                            const std::string& strTempDir,
                            bool bNoUserInteraction,
                            uint64_t& iHeaderSkip, unsigned& iComponentSize,
                            uint64_t& iComponentCount, bool& bConvertEndianess,
                            bool& bSigned, bool& bIsFloat,
                            Vec3ui64& vVolumeSize,
                            Vec3f& vVolumeAspect, std::string& strTitle,
                            std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile);

  virtual bool ConvertToNative(const std::string& strRawFilename,
                               const std::string& strTargetFilename,
                               uint64_t iHeaderSkip, unsigned iComponentSize,
                               uint64_t iComponentCount, bool bSigned,
                               bool bFloatingPoint,  Vec3ui64 vVolumeSize,
                               Vec3f vVolumeAspect,
                               bool bNoUserInteraction,
                               const bool bQuantizeTo8Bit);

  virtual bool CanExportData() const { return true; }
  virtual bool CanImportData() const { return true; }

};
#endif // BOVCONVERTER_H
