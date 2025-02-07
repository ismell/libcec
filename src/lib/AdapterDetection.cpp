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

#include "AdapterDetection.h"
#include "platform/os-dependent.h"
#include "util/StdString.h"

#if !defined(__WINDOWS__)
#include <dirent.h>
#include <libudev.h>
#include <poll.h>
#else
#include <setupapi.h>

// the virtual COM port only shows up when requesting devices with the raw device guid!
static GUID USB_RAW_GUID =  { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };
#endif

#define CEC_VID 0x2548
#define CEC_PID 0x1001

using namespace CEC;
using namespace std;

#if !defined(__WINDOWS__)
bool TranslateComPort(CStdString &strString)
{
  CStdString strTmp(strString);
  strTmp.MakeReverse();
  int iSlash = strTmp.Find('/');
  if (iSlash >= 0)
  {
    strTmp = strTmp.Left(iSlash);
    strTmp.MakeReverse();
    strString.Format("%s/%s:1.0/tty", strString.c_str(), strTmp.c_str());
    return true;
  }

  return false;
}

bool FindComPort(CStdString &strLocation)
{
  CStdString strPort = strLocation;
  bool bReturn(!strPort.IsEmpty());
  CStdString strConfigLocation(strLocation);
  if (TranslateComPort(strConfigLocation))
  {
    DIR *dir;
    struct dirent *dirent;
    if((dir = opendir(strConfigLocation.c_str())) == NULL)
      return bReturn;

    while ((dirent = readdir(dir)) != NULL)
    {
      if(strcmp((char*)dirent->d_name, "." ) != 0 && strcmp((char*)dirent->d_name, ".." ) != 0)
      {
        strPort.Format("/dev/%s", dirent->d_name);
        if (!strPort.IsEmpty())
        {
          strLocation = strPort;
          bReturn = true;
          break;
        }
      }
    }
    closedir(dir);
  }

  return bReturn;
}
#endif

int CAdapterDetection::FindAdapters(vector<cec_adapter> &deviceList, const char *strDevicePath /* = NULL */)
{
  int iFound(0);

#if !defined(__WINDOWS__)
  struct udev *udev;
  if (!(udev = udev_new()))
    return -1;

  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(dev_list_entry, devices)
  {
    const char *strPath;
    strPath = udev_list_entry_get_name(dev_list_entry);

    dev = udev_device_new_from_syspath(udev, strPath);
    if (!dev)
      continue;

    dev = udev_device_get_parent(udev_device_get_parent(dev));
    if (!dev)
      continue;
    if (!udev_device_get_sysattr_value(dev,"idVendor") || !udev_device_get_sysattr_value(dev, "idProduct"))
    {
      udev_device_unref(dev);
      continue;
    }

    int iVendor, iProduct;
    sscanf(udev_device_get_sysattr_value(dev, "idVendor"), "%x", &iVendor);
    sscanf(udev_device_get_sysattr_value(dev, "idProduct"), "%x", &iProduct);
    if (iVendor == CEC_VID && iProduct == CEC_PID)
    {
      CStdString strPath(udev_device_get_syspath(dev));
      if (strDevicePath && strcmp(strPath.c_str(), strDevicePath))
        continue;

      CStdString strComm(strPath);
      if (FindComPort(strComm))
      {
        cec_adapter foundDev;
        foundDev.path = strPath;
        foundDev.comm = strComm;
        deviceList.push_back(foundDev);
        ++iFound;
      }
    }
    udev_device_unref(dev);
  }

  udev_enumerate_unref(enumerate);
  udev_unref(udev);
#else
  HDEVINFO hDevHandle;
  DWORD    required = 0, iMemberIndex = 0;
  int      nBufferSize = 0;

  SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
  deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  SP_DEVINFO_DATA devInfoData;
  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

  if ((hDevHandle = SetupDiGetClassDevs(&USB_RAW_GUID, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)) == INVALID_HANDLE_VALUE)
    return iFound;

  BOOL bResult = true;
  TCHAR *buffer = NULL;
  PSP_DEVICE_INTERFACE_DETAIL_DATA devicedetailData;
  while(bResult)
  {
    bResult = SetupDiEnumDeviceInfo(hDevHandle, iMemberIndex, &devInfoData);

    if (bResult)
      bResult = SetupDiEnumDeviceInterfaces(hDevHandle, 0, &USB_RAW_GUID, iMemberIndex, &deviceInterfaceData);

    if(!bResult)
    {
      SetupDiDestroyDeviceInfoList(hDevHandle);
      delete []buffer;
      buffer = NULL;
      return iFound;
    }

    iMemberIndex++;
    BOOL bDetailResult = false;
    {
      // As per MSDN, Get the required buffer size. Call SetupDiGetDeviceInterfaceDetail with a 
      // NULL DeviceInterfaceDetailData pointer, a DeviceInterfaceDetailDataSize of zero, 
      // and a valid RequiredSize variable. In response to such a call, this function returns 
      // the required buffer size at RequiredSize and fails with GetLastError returning 
      // ERROR_INSUFFICIENT_BUFFER. 
      // Allocate an appropriately sized buffer and call the function again to get the interface details. 

      SetupDiGetDeviceInterfaceDetail(hDevHandle, &deviceInterfaceData, NULL, 0, &required, NULL);

      buffer = new TCHAR[required];
      devicedetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) buffer;
      devicedetailData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
      nBufferSize = required;
    }

    bDetailResult = SetupDiGetDeviceInterfaceDetail(hDevHandle, &deviceInterfaceData, devicedetailData, nBufferSize , &required, NULL);
    if(!bDetailResult)
      continue;

    if (strDevicePath && strcmp(strDevicePath, devicedetailData->DevicePath) != 0)
      continue;

    CStdString strVendorId;
    CStdString strProductId;
    CStdString strTmp(devicedetailData->DevicePath);
    strVendorId = strTmp.substr(strTmp.Find("vid_") + 4, 4);
    strProductId = strTmp.substr(strTmp.Find("pid_") + 4, 4);
    if (strTmp.Find("&mi_") >= 0 && strTmp.Find("&mi_00") < 0)
      continue;

    int iVendor, iProduct;
    sscanf(strVendorId, "%x", &iVendor);
    sscanf(strProductId, "%x", &iProduct);
    if (iVendor != CEC_VID || iProduct != CEC_PID)
      continue;

    HKEY hDeviceKey = SetupDiOpenDevRegKey(hDevHandle, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
    if (!hDeviceKey)
      continue;

    TCHAR strPortName[256];
    strPortName[0] = _T('\0');
    DWORD dwSize = sizeof(strPortName);
    DWORD dwType = 0;

    /* search the registry */
    if ((RegQueryValueEx(hDeviceKey, _T("PortName"), NULL, &dwType, reinterpret_cast<LPBYTE>(strPortName), &dwSize) == ERROR_SUCCESS) && (dwType == REG_SZ))
    {
      if (_tcslen(strPortName) > 3 && _tcsnicmp(strPortName, _T("COM"), 3) == 0 &&
        _ttoi(&(strPortName[3])) > 0)
      {
        cec_adapter foundDev;
        foundDev.path = devicedetailData->DevicePath;
        foundDev.comm = strPortName;
        deviceList.push_back(foundDev);
        ++iFound;
      }
    }

    RegCloseKey(hDeviceKey);
  }
#endif

  return iFound;
}
