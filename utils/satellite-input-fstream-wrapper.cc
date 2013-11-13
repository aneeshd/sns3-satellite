/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Modified by: Frans Laakso <frans.laakso@magister.fi>
 */

#include "satellite-input-fstream-wrapper.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include <fstream>

NS_LOG_COMPONENT_DEFINE ("SatInputFileStreamWrapper");

namespace ns3 {

SatInputFileStreamWrapper::SatInputFileStreamWrapper (std::string filename, std::ios::openmode filemode)
  : m_destroyable (true)
{
  NS_LOG_FUNCTION (this << filename << filemode);

  std::ifstream* ifs = new std::ifstream (filename.c_str (), filemode);

  if (!ifs->is_open ())
    {
      // script might be launched by test.py, try a different base path
      delete ifs;
      filename = "../../" + filename;
      ifs = new std::ifstream (filename.c_str (), filemode);

      NS_ABORT_MSG_UNLESS (ifs->is_open (), "SatOutputStreamWrapper::OutputStreamWrapper():  " <<
                           "Unable to Open " << filename << " for mode " << filemode);
    }

  m_ifstream = ifs;
}

SatInputFileStreamWrapper::~SatInputFileStreamWrapper ()
{
  NS_LOG_FUNCTION (this);

  if (m_destroyable)
    {
      delete m_ifstream;
    }

  m_ifstream = 0;
}

std::ifstream *
SatInputFileStreamWrapper::GetStream (void)
{
  NS_LOG_FUNCTION (this);

  return m_ifstream;
}

} // namespace ns3