/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/satellite-module.h"
#include "ns3/applications-module.h"
#include "ns3/cbr-helper.h"

using namespace ns3;

/**
* \ingroup satellite
*
* \brief  An example to test ARQ functionality
*
*         execute command -> ./waf --run "sat-arq-example --PrintHelp"
*/

NS_LOG_COMPONENT_DEFINE ("sat-arq-fwd-example");


int
main (int argc, char *argv[])
{
  uint32_t beamId = 1;
  uint32_t endUsersPerUt (3);
  uint32_t utsPerBeam (3);
  uint32_t packetSize (128);
  Time interval (Seconds(0.3));
  Time simLength (Seconds(100.0));
  Time appStartTime = Seconds(0.1);

  // read command line parameters given by user
  CommandLine cmd;
  cmd.AddValue("endUsersPerUt", "Number of end users per UT", endUsersPerUt);
  cmd.AddValue("utsPerBeam", "Number of UTs per spot-beam", utsPerBeam);
  cmd.Parse (argc, argv);

  // Configure error model
  double errorRate (0.10);
  Config::SetDefault ("ns3::SatPhyRxCarrierConf::ConstantErrorRatio", DoubleValue(errorRate));
  Config::SetDefault ("ns3::SatUtHelper::FwdLinkErrorModel", EnumValue (SatPhyRxCarrierConf::EM_CONSTANT));
  Config::SetDefault ("ns3::SatGwHelper::RtnLinkErrorModel", EnumValue (SatPhyRxCarrierConf::EM_NONE));
  Config::SetDefault ("ns3::SatUtHelper::EnableRtnLinkArq", BooleanValue(true));

  // Enable ARQ
  Config::SetDefault ("ns3::SatUtHelper::EnableRtnLinkArq", BooleanValue(false));
  Config::SetDefault ("ns3::SatUtHelper::EnableFwdLinkArq", BooleanValue(true));

  // RTN link ARQ attributes
  Config::SetDefault ("ns3::SatGenericStreamEncapsulatorArq::MaxNoOfRetransmissions", UintegerValue(2));
  Config::SetDefault ("ns3::SatGenericStreamEncapsulatorArq::WindowSize", UintegerValue(20));
  Config::SetDefault ("ns3::SatGenericStreamEncapsulatorArq::RetransmissionTimer", TimeValue(Seconds (0.6)));
  Config::SetDefault ("ns3::SatGenericStreamEncapsulatorArq::RxWaitingTime", TimeValue(Seconds (1.8)));

  // Create reference system, two options:
  // - "Scenario72"
  // - "Scenario98"
  std::string scenarioName = "Scenario72";
  //std::string scenarioName = "Scenario98";

  Ptr<SatHelper> helper = CreateObject<SatHelper> (scenarioName);

  // create user defined scenario
  SatBeamUserInfo beamInfo = SatBeamUserInfo (utsPerBeam,endUsersPerUt);
  std::map<uint32_t, SatBeamUserInfo > beamMap;
  beamMap[beamId] = beamInfo;
  helper->SetBeamUserInfo (beamMap);
  helper->EnablePacketTrace ();

  helper->CreateScenario (SatHelper::USER_DEFINED);

  // enable info logs
  //LogComponentEnable ("CbrApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  //LogComponentEnable ("sat-cbr-user-defined-example", LOG_LEVEL_INFO);

  // get users
  NodeContainer utUsers = helper->GetUtUsers();
  NodeContainer gwUsers = helper->GetGwUsers();

  // >>> Start of actual test using Full scenario >>>

  // port used for packet delivering
  uint16_t port = 9; // Discard port (RFC 863)

  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (helper->GetUserAddress (utUsers.Get (0)), port)));

  ApplicationContainer gwApps;
  ApplicationContainer utApps;

  CbrKpiHelper kpiHelper (KpiHelper::KPI_FWD);

  //---- Start CBR application definitions

  NS_LOG_INFO("Creating CBR applications and sinks");

  Time startDelay = appStartTime;

  if (utUsers.GetN () > 0)
    {
      // create application on UT user
      PacketSinkHelper cbrSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress(helper->GetUserAddress(utUsers.Get(0)), port));
      CbrHelper cbrHelper ("ns3::UdpSocketFactory", InetSocketAddress(helper->GetUserAddress(utUsers.Get(0)), port));
      cbrHelper.SetAttribute("Interval", TimeValue (interval));
      cbrHelper.SetAttribute("PacketSize", UintegerValue (packetSize));

      // Cbr and Sink applications creation. CBR to UT users and sinks to GW users.
      for ( uint32_t i = 0; i < utUsers.GetN (); i++)
        {
          // CBR sends packets to GW user no 4.
          cbrHelper.SetAttribute("Remote", AddressValue(Address (InetSocketAddress (helper->GetUserAddress (utUsers.Get (i)), port))));
          cbrSinkHelper.SetAttribute("Local", AddressValue(Address (InetSocketAddress (helper->GetUserAddress (utUsers.Get (i)), port))));

          gwApps.Add(cbrHelper.Install (gwUsers.Get (4)));
          utApps.Add(cbrSinkHelper.Install (utUsers.Get (i)));

          startDelay += Seconds (0.001);

          // Set start and end times
          gwApps.Get(i)->SetStartTime (Seconds (0.1));
          gwApps.Get(i)->SetStopTime (simLength);
          utApps.Get(i)->SetStartTime (startDelay);
          utApps.Get(i)->SetStopTime (simLength);
        }

      // Add the created applications to CbrKpiHelper
      kpiHelper.AddSink (utApps);
      kpiHelper.AddSender (gwApps);
    }
  //---- Stop CBR application definitions


  NS_LOG_INFO("--- Cbr-user-defined-example ---");
  NS_LOG_INFO("  Packet size in bytes: " << packetSize);
  NS_LOG_INFO("  Packet sending interval: " << interval.GetSeconds ());
  NS_LOG_INFO("  Simulation length: " << simLength.GetSeconds ());
  NS_LOG_INFO("  Number of UTs: " << utsPerBeam);
  NS_LOG_INFO("  Number of end users per UT: " << endUsersPerUt);
  NS_LOG_INFO("  ");


  Simulator::Stop (simLength);
  Simulator::Run ();

  kpiHelper.Print ();

  Simulator::Destroy ();

  return 0;
}