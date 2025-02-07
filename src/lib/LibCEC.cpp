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

#include "LibCEC.h"

#include "AdapterCommunication.h"
#include "AdapterDetection.h"
#include "CECProcessor.h"
#include "util/StdString.h"
#include "platform/timeutils.h"

using namespace std;
using namespace CEC;

CLibCEC::CLibCEC(const char *strDeviceName, cec_logical_address iLogicalAddress /* = CECDEVICE_PLAYBACKDEVICE1 */, uint16_t iPhysicalAddress /* = CEC_DEFAULT_PHYSICAL_ADDRESS */) :
    m_iCurrentButton(CEC_USER_CONTROL_CODE_UNKNOWN),
    m_buttontime(0)
{
  m_comm = new CAdapterCommunication(this);
  m_cec = new CCECProcessor(this, m_comm, strDeviceName, iLogicalAddress, iPhysicalAddress);
}

CLibCEC::~CLibCEC(void)
{
  Close();
  delete m_cec;
  m_cec = NULL;

  delete m_comm;
  m_comm = NULL;
}

bool CLibCEC::Open(const char *strPort, uint64_t iTimeoutMs /* = 10000 */)
{
  if (!m_comm)
  {
    AddLog(CEC_LOG_ERROR, "no comm port");
    return false;
  }

  if (m_comm->IsOpen())
  {
    AddLog(CEC_LOG_ERROR, "connection already open");
    return false;
  }

  if (!m_comm->Open(strPort, 38400, iTimeoutMs))
  {
    AddLog(CEC_LOG_ERROR, "could not open a connection");
    return false;
  }

  if (!m_cec->Start())
  {
    AddLog(CEC_LOG_ERROR, "could not start CEC communications");
    return false;
  }

  return true;
}

void CLibCEC::Close(void)
{
  if (m_cec)
    m_cec->StopThread();
  if (m_comm)
    m_comm->Close();
}

int CLibCEC::FindAdapters(std::vector<cec_adapter> &deviceList, const char *strDevicePath /* = NULL */)
{
  CStdString strDebug;
  if (strDevicePath)
    strDebug.Format("trying to autodetect the com port for device path '%s'", strDevicePath);
  else
    strDebug.Format("trying to autodetect all CEC adapters");
  AddLog(CEC_LOG_DEBUG, strDebug);

  return CAdapterDetection::FindAdapters(deviceList, strDevicePath);
}

bool CLibCEC::PingAdapter(void)
{
  return m_comm ? m_comm->PingAdapter() : false;
}

bool CLibCEC::StartBootloader(void)
{
  return m_comm ? m_comm->StartBootloader() : false;
}

int CLibCEC::GetMinVersion(void)
{
  return CEC_MIN_VERSION;
}

int CLibCEC::GetLibVersion(void)
{
  return CEC_LIB_VERSION;
}

bool CLibCEC::GetNextLogMessage(cec_log_message *message)
{
  return m_logBuffer.Pop(*message);
}

bool CLibCEC::GetNextKeypress(cec_keypress *key)
{
  return m_keyBuffer.Pop(*key);
}

bool CLibCEC::GetNextCommand(cec_command *command)
{
  return m_commandBuffer.Pop(*command);
}

bool CLibCEC::Transmit(const cec_frame &data, bool bWaitForAck /* = true */)
{
  return m_cec ? m_cec->Transmit(data, bWaitForAck) : false;
}

bool CLibCEC::SetLogicalAddress(cec_logical_address iLogicalAddress)
{
  return m_cec ? m_cec->SetLogicalAddress(iLogicalAddress) : false;
}

bool CLibCEC::PowerOnDevices(cec_logical_address address /* = CECDEVICE_TV */)
{
  return m_cec ? m_cec->PowerOnDevices(address) : false;
}

bool CLibCEC::StandbyDevices(cec_logical_address address /* = CECDEVICE_BROADCAST */)
{
  return m_cec ? m_cec->StandbyDevices(address) : false;
}

bool CLibCEC::SetActiveView(void)
{
  return m_cec ? m_cec->SetActiveView() : false;
}

bool CLibCEC::SetInactiveView(void)
{
  return m_cec ? m_cec->SetInactiveView() : false;
}

void CLibCEC::AddLog(cec_log_level level, const string &strMessage)
{
  cec_log_message message;
  message.level = level;
  message.message.assign(strMessage.c_str());
  m_logBuffer.Push(message);
}

void CLibCEC::AddKey(void)
{
  if (m_iCurrentButton != CEC_USER_CONTROL_CODE_UNKNOWN)
  {
    cec_keypress key;
    key.duration = (unsigned int) (GetTimeMs() - m_buttontime);
    key.keycode = m_iCurrentButton;
    m_keyBuffer.Push(key);
    m_iCurrentButton = CEC_USER_CONTROL_CODE_UNKNOWN;
    m_buttontime = 0;
  }
}

void CLibCEC::AddCommand(cec_logical_address source, cec_logical_address destination, cec_opcode opcode, cec_frame *parameters)
{
  cec_command command;
  command.source       = source;
  command.destination  = destination;
  command.opcode       = opcode;
  if (parameters)
    command.parameters = *parameters;
  if (m_commandBuffer.Push(command))
  {
    CStdString strDebug;
    strDebug.Format("stored command '%d' in the command buffer. buffer size = %d", opcode, m_commandBuffer.Size());
    AddLog(CEC_LOG_DEBUG, strDebug);
  }
  else
  {
    AddLog(CEC_LOG_WARNING, "command buffer is full");
  }
}

void CLibCEC::CheckKeypressTimeout(void)
{
  if (m_iCurrentButton != CEC_USER_CONTROL_CODE_UNKNOWN && GetTimeMs() - m_buttontime > CEC_BUTTON_TIMEOUT)
  {
    AddKey();
    m_iCurrentButton = CEC_USER_CONTROL_CODE_UNKNOWN;
  }
}

void CLibCEC::SetCurrentButton(cec_user_control_code iButtonCode)
{
  m_iCurrentButton = iButtonCode;
  m_buttontime = GetTimeMs();
}

DECLSPEC void * CECCreate(const char *strDeviceName, CEC::cec_logical_address iLogicalAddress /*= CEC::CECDEVICE_PLAYBACKDEVICE1 */, uint16_t iPhysicalAddress /* = CEC_DEFAULT_PHYSICAL_ADDRESS */)
{
  return static_cast< void* > (new CLibCEC(strDeviceName, iLogicalAddress, iPhysicalAddress));
}
