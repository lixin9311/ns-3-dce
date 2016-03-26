/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 INRIA
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
 * Author: Frederic Urbani <frederic.urbani@inria.fr>
 *
 */
#include "dce-node-context.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "utils.h"
#include <string.h>
#include "process.h"
#include "dce-manager.h"
#include <ctime>
#include <cmath>
#include "ns3/mobility-model.h"

inline unsigned char gps_checksum(char* str, int size) {
  char check = 0;
  for (int i = 0; i < size; i++) {
    check = char(check ^ str[i]);
  }
  return check;
}

NS_LOG_COMPONENT_DEFINE ("DceNodeContext");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DceNodeContext);

TypeId
DceNodeContext::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DceNodeContext").SetParent<Object> ()
    .AddConstructor<DceNodeContext> ();

  return tid;
}
TypeId
DceNodeContext::GetInstanceTypeId (void) const
{
  return DceNodeContext::GetTypeId ();
}
DceNodeContext::DceNodeContext ()
{
  m_randomCtx = CreateObject<NormalRandomVariable> ();
  m_randomCtx->SetAttribute ("Mean", DoubleValue (0));
  m_randomCtx->SetAttribute ("Variance", DoubleValue (2 ^ 32 - 1));
  m_rndBuffer = m_randomCtx->GetInteger ();
  m_rndOffset = 0;
}

DceNodeContext::~DceNodeContext ()
{
}

int
DceNodeContext::UName (struct utsname *buf)
{
  Thread *current = Current ();
  NS_ASSERT (current != 0);
  DceManager *manager = current->process->manager;
  NS_ASSERT (manager != 0);
  StringValue release, version;
  manager->GetAttribute("UnameStringRelease", release);
  manager->GetAttribute("UnameStringVersion", version);

  if (0 == m_sysName.length ())
    {
      uint32_t nodeId = UtilsGetNodeId ();
      Ptr<Node> node = NodeList::GetNode (nodeId);
      NS_ASSERT (node != 0);
      std::string nodeName = Names::FindName (node);
      std::ostringstream oss;

      if (nodeName.length () == 0)
        {
          oss << "NODE_" << nodeId;
          nodeName = oss.str ();

          oss.str ("");
          oss.clear ();
        }
      m_sysName = nodeName + "'s OS";
      m_nodeName = nodeName;

      oss << nodeId;
      m_hardId = oss.str ();
    }
  memset (buf, 0, sizeof (struct utsname));

  const std::size_t maxLen = sizeof( ((struct utsname*) 0)->sysname);

  memcpy (buf->sysname, m_sysName.c_str (), std::min ( m_sysName.length (), maxLen));
  memcpy (buf->nodename, m_nodeName.c_str (), std::min ( m_nodeName.length (), maxLen));
  memcpy (buf->release, release.Get ().c_str (), std::min ( release.Get ().length (), maxLen));
  memcpy (buf->version, version.Get ().c_str (), std::min ( version.Get ().length (), maxLen));
  memcpy (buf->machine, m_hardId.c_str (), std::min ( m_hardId.length (), maxLen));

  return 0;
}

Ptr<DceNodeContext>
DceNodeContext::GetNodeContext ()
{
  Thread *current = Current ();
  NS_LOG_FUNCTION (current);
  NS_ASSERT (current != 0);
  DceManager *manager = current->process->manager;
  NS_ASSERT (manager != 0);

  return manager->GetObject<DceNodeContext> ();
}

int
DceNodeContext::RandomRead (void *buf, size_t count)
{
  uint8_t *crsr = (uint8_t*)buf;
  for (uint32_t i = 0; i < count; i++)
    {
      *crsr++ = GetNextRnd ();
    }
  return count;
}

int
DceNodeContext::GPSttyRead (void *buf, size_t count)
{
  int size = 0;
  char* cbuf = (char*)buf;
  uint32_t nodeId = UtilsGetNodeId ();
  Ptr<Node> node = NodeList::GetNode (nodeId);
  NS_ASSERT (node != 0);
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  NS_ASSERT (mobility != 0);
  Vector current = mobility->GetPosition();
  pos center, cart, geo;
  char NorS, EorW;
  double lat, lon;

  center.x = 0;
  center.y = 0;
  LocalArea lc(&center);
  cart.x = current.x;
  cart.y = current.y;
  lc.Cart2Geo(&cart, &geo);
  EorW = geo.x < 0 ? 'W' : 'E';
  NorS = geo.y < 0 ? 'S' : 'N';
  lat = ((geo.y - fmod(geo.y , 1.0)) * 100.0) + (fmod(geo.y , 1.0) * 60.0);
  lon = ((geo.x - fmod(geo.x , 1.0)) * 100.0) + (fmod(geo.x , 1.0) * 60.0);

  Time sim_time = UtilsSimulationTimeToTime(Now());
  time_t rawtime = (time_t)sim_time.GetSeconds();
  struct tm *ptm;
  char date[7], time_smh[7], time_hms[7];
  ptm = gmtime(&rawtime);

  strftime(date,7,"%d%m%y", ptm);
  strftime(time_smh,7,"%S%M%H", ptm);
  strftime(time_hms,7,"%H%M%S", ptm);
  char* tmp = (char*)malloc(512);
  unsigned char checksum;
  int n;

  tmp = (char*)memset(tmp, 0, 512);
  n = sprintf(tmp, "GPRMC,%s,A,%09.4lf,%c,%010.4lf,%c,000.5,054.7,%s,,,A", time_smh, lat, NorS, lon, EorW, date, EorW);
  NS_ASSERT (n > 0);
  checksum = gps_checksum(tmp, n);
  n = sprintf(cbuf, "$%s*%02X\r\n", tmp, checksum);
  cbuf += n;
  size += n;

  tmp = (char*)memset(tmp, 0, 512);
  n = sprintf(tmp, "GPGGA,%s,%09.4lf,%c,%010.4lf,%c,1,08,0.9,0.0,M,46.9,M,,", time_hms, lat, NorS, lon, EorW);
  NS_ASSERT (n > 0);
  checksum = gps_checksum(tmp, n);
  n = sprintf(cbuf, "$%s*%02X\r\n", tmp, checksum);
  free(tmp);
  size += n;
  return size;
}

uint8_t
DceNodeContext::GetNextRnd ()
{
  uint8_t v = ((uint8_t *)  &m_rndBuffer) [ m_rndOffset++ ];
  if (m_rndOffset >= 4)
    {
      m_rndOffset = 0;
      m_rndBuffer = m_randomCtx->GetInteger ();
    }
  return v;
}

} // namespace ns3
