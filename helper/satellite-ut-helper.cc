/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Magister Solutions Ltd
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
 * Author: Sami Rantanen <sami.rantanen@magister.fi>
 */

#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/enum.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/callback.h"
#include "ns3/config.h"
#include "ns3/nstime.h"
#include "../model/satellite-utils.h"
#include "../model/satellite-channel.h"
#include "../model/satellite-mobility-observer.h"
#include "../model/satellite-gw-llc.h"
#include "../model/satellite-ut-llc.h"
#include "../model/satellite-ut-mac.h"
#include "../model/satellite-net-device.h"
#include "../model/satellite-ut-phy.h"
#include "../model/satellite-phy-tx.h"
#include "../model/satellite-phy-rx.h"
#include "../model/satellite-phy-rx-carrier-conf.h"
#include "../model/satellite-base-encapsulator.h"
#include "../model/satellite-generic-stream-encapsulator.h"
#include "../model/satellite-generic-stream-encapsulator-arq.h"
#include "../model/satellite-return-link-encapsulator.h"
#include "../model/satellite-return-link-encapsulator-arq.h"
#include "../model/satellite-net-device.h"
#include "../model/satellite-node-info.h"
#include "../model/satellite-enums.h"
#include "../model/satellite-request-manager.h"
#include "../model/satellite-queue.h"
#include "../model/satellite-ut-scheduler.h"
#include "../model/satellite-channel-estimation-error-container.h"
#include "../model/satellite-packet-classifier.h"
#include "satellite-ut-helper.h"
#include "ns3/singleton.h"
#include "ns3/satellite-id-mapper.h"

NS_LOG_COMPONENT_DEFINE ("SatUtHelper");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SatUtHelper);

TypeId
SatUtHelper::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SatUtHelper")
      .SetParent<Object> ()
      .AddConstructor<SatUtHelper> ()
      .AddAttribute ("FwdLinkErrorModel",
                     "Forward link error model",
                     EnumValue (SatPhyRxCarrierConf::EM_AVI),
                     MakeEnumAccessor (&SatUtHelper::m_errorModel),
                     MakeEnumChecker (SatPhyRxCarrierConf::EM_NONE, "None",
                                      SatPhyRxCarrierConf::EM_CONSTANT, "Constant",
                                      SatPhyRxCarrierConf::EM_AVI, "AVI"))
      .AddAttribute ("DaFwdLinkInterferenceModel",
                     "Forward link interference model for dedicated access",
                     EnumValue (SatPhyRxCarrierConf::IF_CONSTANT),
                     MakeEnumAccessor (&SatUtHelper::m_daInterferenceModel),
                     MakeEnumChecker (SatPhyRxCarrierConf::IF_CONSTANT, "Constant",
                                      SatPhyRxCarrierConf::IF_TRACE, "Trace",
                                      SatPhyRxCarrierConf::IF_PER_PACKET, "PerPacket"))
      .AddAttribute ("LowerLayerServiceConf",
                     "Pointer to lower layer service configuration.",
                     PointerValue (),
                     MakePointerAccessor (&SatUtHelper::m_llsConf),
                     MakePointerChecker<SatLowerLayerServiceConf> ())
       .AddAttribute ("EnableChannelEstimationError",
                      "Enable channel estimation error in forward link receiver at UT.",
                      BooleanValue (true),
                      MakeBooleanAccessor (&SatUtHelper::m_enableChannelEstimationError),
                      MakeBooleanChecker ())
       .AddAttribute ("EnableRtnLinkArq",
                      "Enable ARQ in return link.",
                      BooleanValue (false),
                      MakeBooleanAccessor (&SatUtHelper::m_enableRtnLinkArq),
                      MakeBooleanChecker ())
       .AddAttribute ("EnableFwdLinkArq",
                      "Enable ARQ in forward link.",
                      BooleanValue (false),
                      MakeBooleanAccessor (&SatUtHelper::m_enableFwdLinkArq),
                      MakeBooleanChecker ())
       .AddTraceSource ("Creation",
                        "Creation traces",
                        MakeTraceSourceAccessor (&SatUtHelper::m_creationTrace))
    ;
    return tid;
}

TypeId
SatUtHelper::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);

  return GetTypeId();
}

SatUtHelper::SatUtHelper ()
 : m_carrierBandwidthConverter (),
   m_fwdLinkCarrierCount (),
   m_superframeSeq (),
   m_daInterferenceModel (SatPhyRxCarrierConf::IF_CONSTANT),
   m_errorModel (SatPhyRxCarrierConf::EM_AVI),
   m_linkResults (),
   m_llsConf (),
   m_enableChannelEstimationError (false),
   m_raSettings (),
   m_enableRtnLinkArq (false),
   m_enableFwdLinkArq (false)
{
  NS_LOG_FUNCTION (this);

  // this default constructor should be never called
  NS_FATAL_ERROR ("SatUtHelper::SatUtHelper - Constructor not in use");
}

SatUtHelper::SatUtHelper (CarrierBandwidthConverter carrierBandwidthConverter,
                          uint32_t fwdLinkCarrierCount,
                          Ptr<SatSuperframeSeq> seq,
                          SatMac::ReadCtrlMsgCallback readCb,
                          SatMac::WriteCtrlMsgCallback writeCb,
                          RandomAccessSettings_s randomAccessSettings)
 : m_carrierBandwidthConverter (carrierBandwidthConverter),
   m_fwdLinkCarrierCount (fwdLinkCarrierCount),
   m_superframeSeq (seq),
   m_readCtrlCb (readCb),
   m_writeCtrlCb (writeCb),
   m_daInterferenceModel (SatPhyRxCarrierConf::IF_CONSTANT),
   m_errorModel (SatPhyRxCarrierConf::EM_AVI),
   m_linkResults (),
   m_llsConf (),
   m_enableChannelEstimationError (false),
   m_raSettings (randomAccessSettings),
   m_enableRtnLinkArq (false),
   m_enableFwdLinkArq (false)
{
  NS_LOG_FUNCTION (this << fwdLinkCarrierCount << seq );
  m_deviceFactory.SetTypeId ("ns3::SatNetDevice");
  m_channelFactory.SetTypeId ("ns3::SatChannel");

  m_llsConf = CreateObject<SatLowerLayerServiceConf>  ();
  m_llsConf->CheckAttributes ();

  //LogComponentEnable ("SatUtHelper", LOG_LEVEL_INFO);
}

void
SatUtHelper::Initialize (Ptr<SatLinkResultsDvbS2> lrS2)
{
  NS_LOG_FUNCTION (this);
  /*
   * Forward channel link results (DVB-S2) are created for UTs.
   */
  if (lrS2 && m_errorModel == SatPhyRxCarrierConf::EM_AVI)
    {
      m_linkResults = lrS2;
    }
}

void 
SatUtHelper::SetDeviceAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (this << n1 );

  m_deviceFactory.Set (n1, v1);
}

void 
SatUtHelper::SetChannelAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (this << n1 );

  m_channelFactory.Set (n1, v1);
}

void
SatUtHelper::SetPhyAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (this << n1 );

  Config::SetDefault ("ns3::SatUtPhy::" + n1, v1);
}

NetDeviceContainer 
SatUtHelper::Install (NodeContainer c, uint32_t beamId, Ptr<SatChannel> fCh, Ptr<SatChannel> rCh, Ptr<SatNetDevice> gwNd, Ptr<SatNcc> ncc)
{
  NS_LOG_FUNCTION (this << beamId << fCh << rCh );

  NetDeviceContainer devs;

  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); i++)
  {
    devs.Add (Install (*i, beamId, fCh, rCh, gwNd, ncc));
  }

  return devs;
}

Ptr<NetDevice>
SatUtHelper::Install (Ptr<Node> n, uint32_t beamId, Ptr<SatChannel> fCh, Ptr<SatChannel> rCh, Ptr<SatNetDevice> gwNd, Ptr<SatNcc> ncc)
{
  NS_LOG_FUNCTION (this << n << beamId << fCh << rCh );

  NetDeviceContainer container;

  // Create SatNetDevice
  Ptr<SatNetDevice> dev = m_deviceFactory.Create<SatNetDevice> ();

  // Attach the SatNetDevice to node
  n->AddDevice (dev);

  SatPhy::CreateParam_t params;
  params.m_beamId = beamId;
  params.m_device = dev;
  params.m_txCh = rCh;
  params.m_rxCh = fCh;

  // Create a packet classifier
  Ptr<SatPacketClassifier> classifier = Create<SatPacketClassifier> ();

  /**
   * Channel estimation errors
   */
  Ptr<SatChannelEstimationErrorContainer> cec;
  // Not enabled, create only base class
  if (!m_enableChannelEstimationError)
    {
      cec = Create<SatSimpleChannelEstimationErrorContainer> ();
    }
  // Create SatFwdLinkChannelEstimationErrorContainer
  else
    {
      cec = Create<SatFwdLinkChannelEstimationErrorContainer> ();
    }

  SatPhyRxCarrierConf::RxCarrierCreateParams_s parameters;
  parameters.m_errorModel = m_errorModel;
  parameters.m_daIfModel = m_daInterferenceModel;
  parameters.m_raIfModel = m_raSettings.m_raInterferenceModel;
  parameters.m_converter = m_carrierBandwidthConverter;
  parameters.m_carrierCount = m_fwdLinkCarrierCount;
  parameters.m_cec = cec;
  parameters.m_raCollisionModel = m_raSettings.m_raCollisionModel;

  if (m_raSettings.m_randomAccessModel != SatEnums::RA_MODEL_OFF)
    {
      parameters.m_isRandomAccessEnabled = true;
    }
  else
    {
      parameters.m_isRandomAccessEnabled = false;
    }

  /// TODO get rid of the hard coded 0
  Ptr<SatUtPhy> phy = CreateObject<SatUtPhy> (params,
                                              m_linkResults,
                                              parameters,
                                              m_superframeSeq->GetSuperframeConf (0));

  // Set fading
  phy->SetTxFadingContainer (n->GetObject<SatBaseFading> ());
  phy->SetRxFadingContainer (n->GetObject<SatBaseFading> ());

  Ptr<SatUtMac> mac = CreateObject<SatUtMac> (m_superframeSeq, beamId);
  mac->SetReadCtrlCallback (m_readCtrlCb);
  mac->SetWriteCtrlCallback (m_writeCtrlCb);

  // Set timing advance callback to mac.
  Ptr<SatMobilityObserver> observer = n->GetObject<SatMobilityObserver> ();
  NS_ASSERT (observer != NULL);

  SatUtMac::TimingAdvanceCallback timingCb = MakeCallback (&SatMobilityObserver::GetTimingAdvance, observer);
  mac->SetTimingAdvanceCallback (timingCb);

  // Attach the Mac layer receiver to Phy
  SatPhy::ReceiveCallback recCb = MakeCallback (&SatUtMac::Receive, mac);

  phy->SetAttribute ("ReceiveCb", CallbackValue(recCb));

  // Create Logical Link Control (LLC) layer
  Ptr<SatUtLlc> llc = CreateObject<SatUtLlc> ();

  // Create a request manager and attach it to LLC, and set control message callback to RM
  Ptr<SatRequestManager> rm = CreateObject<SatRequestManager> ();
  rm->SetAttribute ("EvaluationInterval", TimeValue (m_superframeSeq->GetDuration (0)));
  llc->SetRequestManager (rm);
  rm->SetCtrlMsgCallback (MakeCallback (&SatNetDevice::SendControlMsg, dev));

  // Set TBTP callback to UT MAC
  mac->SetAssignedDaResourcesCallback (MakeCallback (&SatRequestManager::AssignedDaResources, rm));

  // Attach the PHY layer to SatNetDevice
  dev->SetPhy (phy);

  // Attach the Mac layer to SatNetDevice
  dev->SetMac (mac);

  // Attach the LLC layer to SatNetDevice
  dev->SetLlc (llc);

  // Attach the packet classifier
  dev->SetPacketClassifier (classifier);

  // Attach the Mac layer C/N0 updates receiver to Phy
  SatPhy::CnoCallback cnoCb = MakeCallback (&SatRequestManager::CnoUpdated, rm);
  phy->SetAttribute ("CnoCb", CallbackValue (cnoCb));

  // Set the device address and pass it to MAC as well
  Mac48Address addr = Mac48Address::Allocate ();
  dev->SetAddress (addr);

  Singleton<SatIdMapper>::Get ()->AttachMacToTraceId (dev->GetAddress ());
  Singleton<SatIdMapper>::Get ()->AttachMacToUtId (dev->GetAddress ());
  Singleton<SatIdMapper>::Get ()->AttachMacToBeamId (dev->GetAddress (),beamId);

  // Create encapsulator and add it to UT's LLC
  Mac48Address gwAddr = Mac48Address::ConvertFrom (gwNd->GetAddress());

  Ptr<SatLlc> gwLlc = gwNd->GetLlc ();

  // --- RETURN LINK ENCAPSULATORS ---

  // Control messages are using flow id (RC index) 0. No need for decapsulator for
  // RC index 0 at the GW, since control messages are terminated already at lower
  // layer.
  Ptr<SatBaseEncapsulator> utEncap = CreateObject<SatBaseEncapsulator> (addr, gwAddr, SatEnums::CONTROL_FID);

  // Create queue event callbacks to MAC and RM
  SatQueue::QueueEventCallback macCb = MakeCallback (&SatUtMac::ReceiveQueueEvent, mac);
  SatQueue::QueueEventCallback rmCb = MakeCallback (&SatRequestManager::ReceiveQueueEvent, rm);

  // Create a queue
  Ptr<SatQueue> queue = CreateObject<SatQueue> (SatEnums::CONTROL_FID);
  queue->AddQueueEventCallback (macCb);
  queue->AddQueueEventCallback (rmCb);
  utEncap->SetQueue (queue);
  llc->AddEncap (addr, utEncap, SatEnums::CONTROL_FID); // Tx

  Ptr<SatBaseEncapsulator> gwDecap;

  for (uint8_t rc = 1; rc < SatEnums::NUM_FIDS; ++rc)
    {
      if (m_enableRtnLinkArq)
        {
          utEncap = CreateObject<SatReturnLinkEncapsulatorArq> (addr, gwAddr, rc);
          gwDecap = CreateObject<SatReturnLinkEncapsulatorArq> (addr, gwAddr, rc);
        }
      else
        {
          utEncap = CreateObject<SatReturnLinkEncapsulator> (addr, gwAddr, rc);
          gwDecap = CreateObject<SatReturnLinkEncapsulator> (addr, gwAddr, rc);
        }

      queue = CreateObject<SatQueue> (rc);
      queue->AddQueueEventCallback (macCb);
      queue->AddQueueEventCallback (rmCb);
      utEncap->SetQueue (queue);
      llc->AddEncap (addr, utEncap, rc); // Tx

      // Create encapsulator and add it to GW's LLC
      gwLlc->AddDecap (addr, gwDecap, rc); // Rx
      gwDecap->SetReceiveCallback (MakeCallback (&SatLlc::ReceiveHigherLayerPdu, gwLlc));
      gwDecap->SetCtrlMsgCallback (MakeCallback (&SatNetDevice::SendControlMsg, gwNd));
    }

  // --- FORWARD LINK ENCAPSULATORS ---

  // Control messages are using flow id (RC index) 0. Forward link is using only one container
  // for control messages, which is configured to send packets to broadcast address.
  Ptr<SatBaseEncapsulator> gwEncap;
  Ptr<SatBaseEncapsulator> utDecap;

  // Create control container for each LLC only once.
  Ptr<SatGwLlc> gatewayLlc = DynamicCast<SatGwLlc> (gwLlc);
  if (gatewayLlc && !gatewayLlc->ControlEncapsulatorCreated ())
    {
      queue = CreateObject<SatQueue> (SatEnums::CONTROL_FID);
      gwEncap = CreateObject<SatBaseEncapsulator> (gwAddr, Mac48Address::GetBroadcast(), SatEnums::CONTROL_FID);
      gwEncap->SetQueue (queue);
      gwLlc->AddEncap (Mac48Address::GetBroadcast(), gwEncap, SatEnums::CONTROL_FID); // Tx
    }

  // User data
  for (uint8_t fid = 1; fid < SatEnums::NUM_FIDS; ++fid)
    {
      if (m_enableFwdLinkArq)
        {
          gwEncap = CreateObject<SatGenericStreamEncapsulatorArq> (gwAddr, addr, fid);
          utDecap = CreateObject<SatGenericStreamEncapsulatorArq> (gwAddr, addr, fid);
        }
      else
        {
          gwEncap = CreateObject<SatGenericStreamEncapsulator> (gwAddr, addr, fid);
          utDecap = CreateObject<SatGenericStreamEncapsulator> (gwAddr, addr, fid);
        }

      queue = CreateObject<SatQueue> (fid);
      gwEncap->SetQueue (queue);
      gwLlc->AddEncap (addr, gwEncap, fid); // Tx

      utDecap->SetReceiveCallback (MakeCallback (&SatLlc::ReceiveHigherLayerPdu, llc));
      llc->AddDecap (addr, utDecap, fid); // Rx
      utDecap->SetCtrlMsgCallback (MakeCallback (&SatNetDevice::SendControlMsg, dev));
    }

  // set serving GW MAC address to RM
  rm->SetGwAddress (gwAddr);

  // Attach the transmit callback to PHY
  mac->SetTransmitCallback (MakeCallback (&SatPhy::SendPdu, phy));

  // Attach the LLC receive callback to SatMac
  mac->SetReceiveCallback (MakeCallback (&SatLlc::Receive, llc));

  // Attach the LLC receive callback to SatMac
  mac->SetControlReceiveCallback (MakeCallback (&SatLlc::ReceiveAck, llc));

  // Attach the device receive callback to SatMac
  llc->SetReceiveCallback (MakeCallback (&SatNetDevice::Receive, dev));

  // Add UT to NCC
  uint32_t raChannel = ncc->AddUt (dev->GetAddress (), m_llsConf, beamId);

  // set RA channel given by NCC to MAC
  mac->SetRaChannel (raChannel);

  phy->Initialize ();
  llc->SetQueueStatisticsCallbacks ();

  // Create UT scheduler for MAC and connect callbacks to LLC
  Ptr<SatUtScheduler> utScheduler = CreateObject<SatUtScheduler> (m_llsConf);
  utScheduler->SetTxOpportunityCallback(MakeCallback (&SatUtLlc::NotifyTxOpportunity, llc));
  utScheduler->SetSchedContextCallback (MakeCallback (&SatLlc::GetSchedulingContexts, llc));
  mac->SetAttribute("Scheduler", PointerValue (utScheduler));

  // Create a node info to all the protocol layers
  Ptr<SatNodeInfo> nodeInfo = Create <SatNodeInfo> (SatEnums::NT_UT, n->GetId (), Mac48Address::ConvertFrom (addr));
  dev->SetNodeInfo (nodeInfo);
  llc->SetNodeInfo (nodeInfo);
  mac->SetNodeInfo (nodeInfo);
  phy->SetNodeInfo (nodeInfo);

  rm->Initialize (m_llsConf);

  if (m_raSettings.m_randomAccessModel != SatEnums::RA_MODEL_OFF)
    {
      Ptr<SatRandomAccessConf> randomAccessConf = CreateObject<SatRandomAccessConf> (m_llsConf,m_superframeSeq);

      /// create RA module with defaults
      Ptr<SatRandomAccess> randomAccess = CreateObject<SatRandomAccess> (randomAccessConf, m_raSettings.m_randomAccessModel);

      /// attach callbacks
      randomAccess->SetAreBuffersEmptyCallback (MakeCallback(&SatLlc::BuffersEmpty, llc));

      /// define which allocation channels should be used with each of the random access models
      /// TODO get rid of the hard coded allocation channel 0
      randomAccess->AddCrdsaAllocationChannel (0);
      randomAccess->AddSlottedAlohaAllocationChannel (0);

      /// attach the RA module
      mac->SetRandomAccess (randomAccess);
    }

  return dev;
}

void
SatUtHelper::EnableCreationTraces(Ptr<OutputStreamWrapper> stream, CallbackBase &cb)
{
  NS_LOG_FUNCTION (this);

  TraceConnect("Creation", "SatUtHelper", cb);
}

} // namespace ns3
