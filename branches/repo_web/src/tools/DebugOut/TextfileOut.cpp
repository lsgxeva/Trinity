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
  \file    TextfileOut.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include "TextfileOut.h"
#include <fstream>
#include <stdarg.h>
#ifdef WIN32
  #include <windows.h>
#endif
#include <ctime>

using namespace std;
using namespace IVDA;

TextfileOut::TextfileOut(std::string strFilename, bool bClearFile) :
  m_strFilename(strFilename)
{
  this->Message(_IVDA_func_, "Starting up");

  if (bClearFile) {
    ofstream fs;
    fs.open(m_strFilename.c_str());
    fs.close();
  }
}

TextfileOut::~TextfileOut() {
  this->Message(_IVDA_func_, "Shutting down\n");
}

void TextfileOut::printf(enum DebugChannel channel, const char* source,
                         const char* buff)
{
  time_t epoch_time;
  time(&epoch_time);

#undef ADDR_NOW
#ifdef DETECTED_OS_WINDOWS
  struct tm now;
  localtime_s(&now, &epoch_time);
#define ADDR_NOW (&now)
#else
  struct tm* now;
  now = localtime(&epoch_time);
#define ADDR_NOW (now)
#endif
  char datetime[64];

  ofstream fs;
  fs.open(m_strFilename.c_str(), ios_base::app);
  if (fs.fail()) return;

  if(strftime(datetime, 64, "(%d.%m.%Y %H:%M:%S)", ADDR_NOW) > 0) {
    fs << datetime << " ";
  }
  fs << ChannelToString(channel) << " (" << source << ") " << buff << std::endl;

  fs.flush();
  fs.close();
}

void TextfileOut::printf(const char *s) const
{
  time_t epoch_time;
  time(&epoch_time);

#undef ADDR_NOW
#ifdef DETECTED_OS_WINDOWS
  struct tm now;
  localtime_s(&now, &epoch_time);
#define ADDR_NOW (&now)
#else
  struct tm* now;
  now = localtime(&epoch_time);
#define ADDR_NOW (now)
#endif
  char datetime[64];

  ofstream fs;
  fs.open(m_strFilename.c_str(), ios_base::app);
  if (fs.fail()) return;

  if(strftime(datetime, 64, "(%d.%m.%Y %H:%M:%S)", ADDR_NOW) > 0) {
    fs << datetime << " " << s << std::endl;
  } else {
    fs << s << std::endl;
  }

  fs.flush();
  fs.close();
}
