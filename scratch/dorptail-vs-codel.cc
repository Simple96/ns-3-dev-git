/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 NITK Surathkal
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
 * Author: Shefali Gupta <shefaligups11@ogmail.com>
 *         Jendaipou Palmei <jendaipoupalmei@gmail.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/internet-module.h"
#include "ns3/tcp-header.h"
#include "ns3/traffic-control-module.h"
#include  <string>

// Dumbbell topology with 7 senders and 1 receiver
// is used for this example. On successful completion,
// the Congestion window and Queue size traces get stored
// in MixTraffic/ directory, inside cwndTraces and
// queueTraces sub-directories, respectively.

using namespace ns3;

std::string dir = "MixTraffic/";
int last_time_dropped = 0;

void
CheckQueueSize (Ptr<QueueDisc> queue,std::string queue_disc_type)
{
  double qSize = queue->GetCurrentSize ().GetValue ();
  // check queue size every 1/10 of a second
  Simulator::Schedule (Seconds (0.1), &CheckQueueSize, queue, queue_disc_type);

  std::ofstream fPlotQueue (dir + queue_disc_type + "/queueTraces/queue.plotme", std::ios::out | std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();
}

static void
TraceDroppedPackets(QueueDiscContainer queueDiscs)
{
    QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();
    std::cout << st.nTotalDroppedPackets - last_time_dropped << std::endl;
    last_time_dropped = st.nTotalDroppedPackets;
}

static void
CwndTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd / 1446.0 << std::endl;
}

static void
TraceCwnd (std::string queue_disc_type)
{
  for (uint8_t i = 0; i < 5; i++)
    {
      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (dir + queue_disc_type + "/cwndTraces/S1-" + std::to_string (i + 1) + ".plotme");
      Config::ConnectWithoutContext ("/NodeList/" + std::to_string (i) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback (&CwndTrace,stream));
    }
}

static void
RttTrace (Ptr<OutputStreamWrapper> stream, Time oldval, Time newval)
{
    /*
    if (firstRtt)
    {
        *rttStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
        firstRtt = false;
    }
     */
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

static void
TraceRtt (std::string queue_disc_type)
{
    for (uint8_t i = 0; i < 5; i++)
    {
        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (dir + queue_disc_type + "/RttTraces/S1-" + std::to_string (i + 1) + ".plotme");
        Config::ConnectWithoutContext ("/NodeList/" + std::to_string (i) + "/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeBoundCallback (&RttTrace,stream));
    }
/*
    std::ofstream ascii;
    Ptr<OutputStreamWrapper> ascii_wrap;
    ascii.open ((prefix_file_name + "-ascii").c_str ());
    ascii_wrap = new OutputStreamWrapper ((prefix_file_name + "-ascii").c_str (),
                                          std::ios::out);
    stack.EnableAsciiIpv4All (ascii_wrap);
    Simulator::Schedule (Seconds (0.01), &TraceRtt, prefix_file_name + "-rtt.data");

    AsciiTraceHelper ascii;
    rttStream = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
    Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));
*/
}
/*
static void
RtoTracer (Time oldval, Time newval)
{
    if (firstRto)
    {
        *rtoStream->GetStream () << "0.0 " << oldval.GetSeconds () << std::endl;
        firstRto = false;
    }
    *rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}
*/

void experiment (std::string queue_disc_type)
{
  // Set the simulation stop time in seconds
  double stopTime = 101;
  std::string queue_disc = std::string ("ns3::") + queue_disc_type;

  std::string bottleneckBandwidth = "1Mbps";
  std::string bottleneckDelay = "50ms";

  std::string accessBandwidth = "1000Mbps";
  std::string accessDelay = "5ms";

  std::string prefix_file_name = "VariantsComparison";

  // Create sender
  NodeContainer tcpSender;
  tcpSender.Create (5);

  NodeContainer udpSender;
  udpSender.Create (2);
  
  // Create gateway
  NodeContainer gateway;
  gateway.Create (2);

  // Create sink
  NodeContainer sink;
  sink.Create (1);

  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 20));
  Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue (Seconds (0)));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocketBase::LimitedTransmit", BooleanValue (false));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault ("ns3::TcpSocketBase::WindowScaling", BooleanValue (true));
  Config::SetDefault (queue_disc + "::MaxSize", QueueSizeValue (QueueSize ("200p")));

  InternetStackHelper internet;
  internet.InstallAll ();

  TrafficControlHelper tchPfifo;
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue ("1000p"));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc (queue_disc);

  PointToPointHelper accessLink;
  accessLink.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  accessLink.SetChannelAttribute ("Delay", StringValue (accessDelay));

  // Configure the senders and sinks net devices
  // and the channels between the senders/sinks and the gateways
  NetDeviceContainer devices [5];
  for (uint8_t i = 0; i < 5; i++)
    {
      devices [i] = accessLink.Install (tcpSender.Get (i), gateway.Get (0));
      tchPfifo.Install (devices [i]);
    }

  NetDeviceContainer devices_sink;
  devices_sink = accessLink.Install (gateway.Get (1), sink.Get (0));
  tchPfifo.Install (devices_sink);

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  NetDeviceContainer devices_gateway;
  devices_gateway = bottleneckLink.Install (gateway.Get (0), gateway.Get (1));
  // Install QueueDisc at gateway
  QueueDiscContainer queueDiscs = tch.Install (devices_gateway);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces [5];
  Ipv4InterfaceContainer interfaces_sink;
  Ipv4InterfaceContainer interfaces_gateway;
  Ipv4InterfaceContainer udpinterfaces [2];

  NetDeviceContainer udpdevices [2];

  for (uint8_t i = 0; i < 5; i++)
    {
      address.NewNetwork ();
      interfaces [i] = address.Assign (devices [i]);
    }

  for (uint8_t i = 0; i < 2; i++)
    {
      udpdevices [i] = accessLink.Install (udpSender.Get (i), gateway.Get (0));
      address.NewNetwork ();
      udpinterfaces [i] = address.Assign (udpdevices [i]);
    }

  address.NewNetwork ();
  interfaces_gateway = address.Assign (devices_gateway);

  address.NewNetwork ();
  interfaces_sink = address.Assign (devices_sink);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  uint16_t port1 = 50001;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  Address sinkLocalAddress1 (InetSocketAddress (Ipv4Address::GetAny (), port1));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  PacketSinkHelper sinkHelper1 ("ns3::UdpSocketFactory", sinkLocalAddress1);

  AddressValue remoteAddress (InetSocketAddress (interfaces_sink.GetAddress (1), port));
  AddressValue remoteAddress1 (InetSocketAddress (interfaces_sink.GetAddress (1), port1));

  BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
  ftp.SetAttribute ("Remote", remoteAddress);
  ftp.SetAttribute ("SendSize", UintegerValue (1000));

  ApplicationContainer sourceApp = ftp.Install (tcpSender);
  sourceApp.Start (Seconds (0));
  sourceApp.Stop (Seconds (stopTime - 1));

  sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  ApplicationContainer sinkApp = sinkHelper.Install (sink);
  sinkApp.Start (Seconds (0));
  sinkApp.Stop (Seconds (stopTime));

  OnOffHelper clientHelper6 ("ns3::UdpSocketFactory", Address ());
  clientHelper6.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper6.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper6.SetAttribute ("DataRate", DataRateValue (DataRate ("0.4Mb/s")));
  clientHelper6.SetAttribute ("PacketSize", UintegerValue (1000));

  ApplicationContainer clientApps6;
  clientHelper6.SetAttribute ("Remote", remoteAddress1);
  clientApps6.Add (clientHelper6.Install (udpSender.Get (0)));
  clientApps6.Start (Seconds (0));
  clientApps6.Stop (Seconds (stopTime - 1));

  OnOffHelper clientHelper7 ("ns3::UdpSocketFactory", Address ());
  clientHelper7.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper7.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper7.SetAttribute ("DataRate", DataRateValue (DataRate ("0.4Mb/s")));
  clientHelper7.SetAttribute ("PacketSize", UintegerValue (1000));

  ApplicationContainer clientApps7;
  clientHelper7.SetAttribute ("Remote", remoteAddress1);
  clientApps7.Add (clientHelper7.Install (udpSender.Get (1)));
  clientApps7.Add (clientHelper7.Install (udpSender.Get (1)));
  clientApps7.Start (Seconds (0));
  clientApps7.Stop (Seconds (stopTime - 1));

  sinkHelper1.SetAttribute ("Protocol", TypeIdValue (UdpSocketFactory::GetTypeId ()));
  ApplicationContainer sinkApp1 = sinkHelper1.Install (sink);
  sinkApp1.Start (Seconds (0));
  sinkApp1.Stop (Seconds (stopTime));

  Ptr<QueueDisc> queue = queueDiscs.Get (0);
  Simulator::ScheduleNow (&CheckQueueSize, queue,queue_disc_type);

  std::string dirToSave = "mkdir -p " + dir + queue_disc_type;
  if (system ((dirToSave + "/cwndTraces/").c_str ()) == -1
      || system ((dirToSave + "/queueTraces/").c_str ()) == -1
      || system ((dirToSave + "/RttTraces/").c_str ()) == -1 )
    {
      exit (1);
    }

  //for (int i = 0; i < 100; i++)
  Simulator::Schedule (Seconds (0.01), &TraceCwnd,queue_disc_type);
  //for (int i = 0; i < 20; i++)
  Simulator::Schedule (Seconds (0.01), &TraceRtt,queue_disc_type);
  for (int i = 0; i < 100; i++)
    Simulator::Schedule (Seconds (i * 1.0), &TraceDroppedPackets,queueDiscs);

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();
    QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();
    std::cout << "*** codel stats from Node 2 queue ***" << std::endl;
    std::cout << st.nTotalDroppedPackets<< std::endl;
    std::cout << "\t " << st.GetNDroppedPackets (CoDelQueueDisc::TARGET_EXCEEDED_DROP)
              << " drops due to prob mark" << std::endl;
  Simulator::Destroy ();
}

int main (int argc, char **argv)
{
  std::cout << "Simulation with FIFO QueueDisc: Start\n" << std::flush;
  experiment ("PfifoFastQueueDisc");
  std::cout << "Simulation with FIFO QueueDisc: End\n" << std::flush;
  std::cout << "------------------------------------------------\n";
  std::cout << "Simulation with CoDel QueueDisc: Start\n";
  experiment ("CoDelQueueDisc");
  std::cout << "Simulation with CoDel QueueDisc: End\n";

  return 0;
}
