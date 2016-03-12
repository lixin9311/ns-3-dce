#include "ns3/dce-module.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
//#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/data-rate.h"
#include "ns3/process-delay-model.h"
#include <cstdarg>
#include <cstdlib>
#include <map>

using namespace ns3;

// Helper function to create a DCE program by command line
// Arguments ends by NULL, like execl(3)
// Usage:
//    DceApplicationHelper dce =
//     dceCreateProgram ("iperf", "-s", "-P", "1", NULL);
static DceApplicationHelper dceCreateProgram (const char *binary, ...)
{
  DceApplicationHelper dce;
  dce.SetStackSize (1 << 20);
  dce.SetBinary (binary);
  dce.ResetArguments ();
  dce.ResetEnvironment ();

  va_list va;
  va_start (va, binary);
  for ( ; ; )
  {
    const char *v = va_arg(va, const char *);
    if (v == NULL)
      break;
    dce.AddArgument (v);
  }
  va_end (va);

  return dce;

}


// ===========================================================================
//
//   Topology
//
//
//         node 0                node 1                     node n
//   +----------------+                               +----------------+
//   |                |                               | btpecho client |
//   | btpecho server |                               |       or       |
//   |                |                               |      ping6     |
//   +----------------+    +----------------+         +----------------+
//   |   cargeo6 MR   |    |   cargeo6 MR   |   ...   |   cargeo6 MR   |
//   |       or       |    |       or       |         |       or       |
//   |  IPv6 & olsrd  |    |  IPv6 & olsrd  |         |  IPv6 & olsrd  |
//   +----------------+    +----------------+         +----------------+
//   |   linux Ether  |    |   linux Ether  |         |   linux Ether  |
//   +----------------+    +----------------+         +----------------+
//   |     Wi-Fi      |    |     Wi-Fi      |         |     Wi-Fi      |
//   +----------------+    +----------------+         +----------------+
//           |        500m         |                          |
//   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -Wi-Fi Ad-Hoc
//
//
// ===========================================================================

std::string GetPhyModeFromBandWidth(double bw) {
    std::map<int, std::string> rate_map;
    rate_map[1 ] = "DsssRate1Mbps"    ;
    rate_map[2 ] = "DsssRate2Mbps"    ;
    rate_map[5 ] = "DsssRate5_5Mbps"  ;
    rate_map[6 ] = "ErpOfdmRate6Mbps" ;
    rate_map[9 ] = "ErpOfdmRate9Mbps" ;
    rate_map[11] = "DsssRate11Mbps"   ;
    rate_map[12] = "ErpOfdmRate12Mbps";
    rate_map[18] = "ErpOfdmRate18Mbps";
    rate_map[24] = "ErpOfdmRate24Mbps";
    rate_map[36] = "ErpOfdmRate36Mbps";
    rate_map[48] = "ErpOfdmRate48Mbps";
    rate_map[54] = "ErpOfdmRate54Mbps";
    return rate_map[int(bw)];
}

int main (int argc, char *argv[])
{
  std::string count = "4";
  std::string interval = "1.0";
  double bw = 6;
  double rss = -80;
  double distance = 100; // 500 maybe too long for multi-hop
  double delay_terminal = 0.000200;
  double delay_intermediate = 0.000540;
  int size = 64;
  int hop = 1;
  bool ping = false;
  bool pcap = false;
  std::string pcap_prefix = "gpsd";
  CommandLine cmd;

  cmd.AddValue ("bw", "Wifi bandwidth. choose from 1,2,5.5,6,9,11,12,18,24,36,48,54", bw);
  cmd.AddValue ("rss", "received signal strength in DBi", rss);
  cmd.AddValue ("distance", "distance between each nodes, in m", distance);
  cmd.AddValue ("count", "btpecho packet count", count);
  cmd.AddValue ("interval", "btpecho packet interval in s (0.2~ )", interval);
  cmd.AddValue ("size", "btpecho packet size in byte (8~1390)", size);
  cmd.AddValue ("ping", "use ping6 instead of btpecho", ping);
  cmd.AddValue ("hop", "number of hops", hop);
  cmd.AddValue ("dt", "processing delay of terminal nodes, in s", delay_terminal);
  cmd.AddValue ("di", "processing delay of intermediate nodes, in s", delay_intermediate);
  cmd.AddValue ("pcap", "generate pcap for all nodes", pcap);
  cmd.AddValue ("prefix", "prefix for pcap file", pcap_prefix);

  cmd.Parse (argc, argv);

  // Phy mode, see also YansWifiPhy::Configure80211g() in yans-wifi-phy.cc
  // https://www.nsnam.org/doxygen/classns3_1_1_yans_wifi_phy.html
  std::string phyMode = GetPhyModeFromBandWidth(bw);
  if (phyMode == "")
  {
      std::cerr << "Bandwidth illegal" << std::endl;
      return -1;
  }

  if (size < 8 || size > 1390)
  {
      std::cerr << "Packet size must between 8 and 1390" << std::endl;
      return -1;
  }

  if (hop < 1)
  {
      std::cerr << "Hop may not less than 1 (2 nodes)" << std::endl;
      return -1;
  }

  // Ping6's -s is payload size
  if (ping == true) {
      size -= 8;
  }

  char size_str[128];
  sprintf (size_str, "%d", size);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer nodes;
  nodes.Create (hop + 1);

  DceManagerHelper dceManager;
  dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));
  LinuxStackHelper stack;
  stack.Install (nodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (-10));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  dceManager.SetDelayModel ("ns3::RandomProcessDelayModel",
                            "Variable", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  dceManager.Install (nodes.Get(0));
  dceManager.Install (nodes.Get(hop));

  dceManager.SetDelayModel ("ns3::RandomProcessDelayModel",
                            "Variable", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  for (int i = 1; i < hop; i++)
  {
      dceManager.Install (nodes.Get (i));
  }

  DceApplicationHelper dce;
  ApplicationContainer apps;

	dce = dceCreateProgram ("read_test", "/dev/ttyGPS", "/dev/urandom", NULL);
	apps = dce.Install (nodes.Get (0));
	apps.Start (Seconds (0.1));

  dce = dceCreateProgram ("read_test", "/dev/ttyGPS", "/dev/urandom", NULL);
  // dce = dceCreateProgram ("gpsd", "-N", "-n", "-D", "5", "/dev/ttyGPS", NULL);
  apps = dce.Install (nodes.Get (1));
  apps.Start (Seconds (2));

  // Wait for enough time for DAD
  apps.Start (Seconds (5));
  //apps.Stop (Seconds (20));

  if (pcap)
  {
      wifiPhy.EnablePcapAll (pcap_prefix, false);
  }

  // A rough estimation total emulation time to get rid of dry-run of itsnet
  double tot_time = strtod (count.c_str (), NULL) * strtod (interval.c_str (), NULL);
  Simulator::Stop (Seconds (tot_time + 100. ));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
