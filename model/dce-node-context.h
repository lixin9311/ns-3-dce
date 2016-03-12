/* -*-  Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
 * Author:  Frederic Urbani <frederic.urbani@inria.fr>
 */

#ifndef DCE_NODE_CONTEXT_H
#define DCE_NODE_CONTEXT_H

#define _USE_MATH_DEFINES
#define RADIUS 6378137.0
#include <string>
#include <cmath>
#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/traced-callback.h"
#include "ns3/simulator.h"
#include <sys/utsname.h>
#include "ns3/random-variable-stream.h"

extern "C" struct Libc;

namespace ns3 {

struct Process;
struct Thread;

struct pos {
	double x, y; // x -> lon, y -> lat
};

class LocalArea : public Object {
	public:
	pos center;
	double dlon, dlat;
	int Geo2Cart(pos* geo, pos* cart) {
  	cart->x = (geo->x - this->center.x) * this->dlon;
  	cart->y = (geo->y - this->center.y) * this->dlat;
  	return 0;
  }
	int Cart2Geo(pos* cart, pos* geo) {
  	geo->x = this->center.x + (cart->x / this->dlon);
  	geo->y = this->center.y + (cart->y / this->dlat);
  	return 0;
  }
	LocalArea(pos* geo) {
		this->init(geo);
	}
	int init(pos* geo) {
  	this->center = *geo;
  	this->dlat = RADIUS * M_PI / 180.0;
  	this->dlon = RADIUS * cos(geo->y / 180.0 * M_PI) * M_PI / 180.0;
  	return 0;
  }
};

/**
 * \brief Manages data attached to a Node usable by DCE such as uname result random context ...
  */
class DceNodeContext : public Object
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  DceNodeContext ();
  virtual ~DceNodeContext ();

  int UName (struct utsname *buf);

  static Ptr<DceNodeContext> GetNodeContext ();

  int RandomRead (void *buf, size_t count);

  int GPSttyRead (void *buf, size_t count);

private:
  inline uint8_t GetNextRnd ();

  std::string m_sysName;
  std::string m_nodeName;
  std::string m_release;
  std::string m_version;
  std::string m_hardId;
  Ptr<RandomVariableStream> m_randomCtx;
  uint32_t m_rndBuffer;
  uint8_t m_rndOffset;
};

} // namespace ns3

#endif /* DCE_NODE_CONTEXT_H */
