#pragma once
/*
 * This file is part of the libCEC(R) library.
 *
 * libCEC(R) is Copyright (C) 2011 Pulse-Eight Limited.  All rights reserved.
 * libCEC(R) is an original work, containing original code.
 *
 * libCEC(R) is a trademark of Pulse-Eight Limited.
 *
 * This program is dual-licensed; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * Alternatively, you can license this library under a commercial license,
 * please contact Pulse-Eight Licensing for more information.
 *
 * For more information contact:
 * Pulse-Eight Licensing       <license@pulse-eight.com>
 *     http://www.pulse-eight.com/
 *     http://www.pulse-eight.net/
 */

#include "../../include/CECExports.h"
#include "platform/threads.h"

namespace CEC
{
  class CSerialPort;
  class CLibCEC;

  class CAdapterCommunication : CThread
  {
  public:
    CAdapterCommunication(CLibCEC *controller);
    virtual ~CAdapterCommunication();

    bool Open(const char *strPort, uint16_t iBaudRate = 38400, uint64_t iTimeoutMs = 10000);
    bool Read(cec_frame &msg, uint64_t iTimeout = 1000);
    bool Write(const cec_frame &frame);
    bool PingAdapter(void);
    void Close(void);
    bool IsOpen(void) const { return !m_bStop && m_bStarted; }
    std::string GetError(void) const;

    void *Process(void);

    bool StartBootloader(void);
    bool SetAckMask(uint16_t iMask);
    static void PushEscaped(cec_frame &vec, uint8_t byte);
  private:
    void AddData(uint8_t *data, uint8_t iLen);
    bool ReadFromDevice(uint64_t iTimeout);

    CSerialPort *        m_port;
    CLibCEC *            m_controller;
    uint8_t*             m_inbuf;
    int                  m_iInbufSize;
    int                  m_iInbufUsed;
    bool                 m_bStarted;
    bool                 m_bStop;
    CMutex               m_commMutex;
    CMutex               m_bufferMutex;
    CCondition           m_condition;
  };
};
