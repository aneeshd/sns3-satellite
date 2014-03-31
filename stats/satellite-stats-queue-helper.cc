/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Magister Solutions
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
 * Author: Budiarto Herman <budiarto.herman@magister.fi>
 *
 */

#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/fatal-error.h>
#include <ns3/enum.h>
#include <ns3/string.h>
#include <ns3/boolean.h>

#include <ns3/node-container.h>
#include <ns3/mac48-address.h>
#include <ns3/net-device.h>
#include <ns3/satellite-net-device.h>
#include <ns3/satellite-llc.h>
#include <ns3/satellite-phy.h>
#include <ns3/satellite-phy-rx.h>

#include <ns3/satellite-helper.h>
#include <ns3/satellite-id-mapper.h>
#include <ns3/singleton.h>

#include <ns3/data-collection-object.h>
#include <ns3/unit-conversion-collector.h>
#include <ns3/distribution-collector.h>
#include <ns3/scalar-collector.h>
#include <ns3/multi-file-aggregator.h>
#include <ns3/gnuplot-aggregator.h>

#include "satellite-stats-queue-helper.h"

NS_LOG_COMPONENT_DEFINE ("SatStatsQueueHelper");


namespace ns3 {


// BASE CLASS /////////////////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsQueueHelper);

std::string // static
SatStatsQueueHelper::GetUnitTypeName (SatStatsQueueHelper::UnitType_t unitType)
{
  switch (unitType)
    {
    case SatStatsQueueHelper::UNIT_BYTES:
      return "UNIT_BYTES";
    case SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS:
      return "UNIT_NUMBER_OF_PACKETS";
    default:
      NS_FATAL_ERROR ("SatStatsQueueHelper - Invalid unit type");
      break;
    }

  NS_FATAL_ERROR ("SatStatsQueueHelper - Invalid unit type");
  return "";
}


SatStatsQueueHelper::SatStatsQueueHelper (Ptr<const SatHelper> satHelper)
  : SatStatsHelper (satHelper),
    m_pollInterval (MilliSeconds (10)),
    m_unitType (SatStatsQueueHelper::UNIT_BYTES),
    m_shortLabel (""),
    m_longLabel (""),
    m_bytesMinValue (0.0),
    m_bytesMaxValue (0.0),
    m_bytesBinLength (0.0),
    m_packetsMinValue (0.0),
    m_packetsMaxValue (0.0),
    m_packetsBinLength (0.0)
{
  NS_LOG_FUNCTION (this << satHelper);
}


SatStatsQueueHelper::~SatStatsQueueHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsQueueHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsQueueHelper")
    .SetParent<SatStatsHelper> ()
    .AddAttribute ("PollInterval",
                   "",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&SatStatsQueueHelper::SetPollInterval,
                                     &SatStatsQueueHelper::GetPollInterval),
                   MakeTimeChecker ())
    .AddAttribute ("BytesMinValue",
                   "Configure the MinValue attribute of the histogram, PDF, "
                   "and CDF output in bytes unit.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&SatStatsQueueHelper::SetBytesMinValue,
                                       &SatStatsQueueHelper::GetBytesMinValue),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("BytesMaxValue",
                   "Configure the MaxValue attribute of the histogram, PDF, "
                   "and CDF output in bytes unit.",
                   DoubleValue (1000.0),
                   MakeDoubleAccessor (&SatStatsQueueHelper::SetBytesMaxValue,
                                       &SatStatsQueueHelper::GetBytesMaxValue),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("BytesBinLength",
                   "Configure the BinLength attribute of the histogram, PDF, "
                   "and CDF output in bytes unit.",
                   DoubleValue (20.0),
                   MakeDoubleAccessor (&SatStatsQueueHelper::SetBytesBinLength,
                                       &SatStatsQueueHelper::GetBytesBinLength),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PacketsMinValue",
                   "Configure the MinValue attribute of the histogram, PDF, "
                   "and CDF output in packets unit.",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&SatStatsQueueHelper::SetPacketsMinValue,
                                       &SatStatsQueueHelper::GetPacketsMinValue),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PacketsMaxValue",
                   "Configure the MaxValue attribute of the histogram, PDF, "
                   "and CDF output in packets unit.",
                   DoubleValue (50.0),
                   MakeDoubleAccessor (&SatStatsQueueHelper::SetPacketsMaxValue,
                                       &SatStatsQueueHelper::GetPacketsMaxValue),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PacketsBinLength",
                   "Configure the BinLength attribute of the histogram, PDF, "
                   "and CDF output in packets unit.",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&SatStatsQueueHelper::SetPacketsBinLength,
                                       &SatStatsQueueHelper::GetPacketsBinLength),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}


void
SatStatsQueueHelper::SetUnitType (SatStatsQueueHelper::UnitType_t unitType)
{
  NS_LOG_FUNCTION (this << GetUnitTypeName (unitType));
  m_unitType = unitType;

  // Update presentation-based attributes.
  if (unitType == SatStatsQueueHelper::UNIT_BYTES)
    {
      m_shortLabel = "size_bytes";
      m_longLabel = "Queue size (in bytes)";
    }
  else if (unitType == SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS)
    {
      m_shortLabel = "num_packets";
      m_longLabel = "Queue size (in number of packets)";
    }
  else
    {
      NS_FATAL_ERROR ("SatStatsQueueHelper - Invalid unit type");
    }
}


SatStatsQueueHelper::UnitType_t
SatStatsQueueHelper::GetUnitType () const
{
  return m_unitType;
}


void
SatStatsQueueHelper::SetPollInterval (Time pollInterval)
{
  NS_LOG_FUNCTION (this << pollInterval.GetSeconds ());
  m_pollInterval = pollInterval;
}


Time
SatStatsQueueHelper::GetPollInterval () const
{
  return m_pollInterval;
}


void
SatStatsQueueHelper::SetBytesMinValue (double minValue)
{
  NS_LOG_FUNCTION (this << minValue);
  m_bytesMinValue = minValue;
}


double
SatStatsQueueHelper::GetBytesMinValue () const
{
  return m_bytesMinValue;
}


void
SatStatsQueueHelper::SetBytesMaxValue (double maxValue)
{
  NS_LOG_FUNCTION (this << maxValue);
  m_bytesMaxValue = maxValue;
}


double
SatStatsQueueHelper::GetBytesMaxValue () const
{
  return m_bytesMaxValue;
}


void
SatStatsQueueHelper::SetBytesBinLength (double binLength)
{
  NS_LOG_FUNCTION (this << binLength);
  m_bytesBinLength = binLength;
}


double
SatStatsQueueHelper::GetBytesBinLength () const
{
  return m_bytesBinLength;
}


void
SatStatsQueueHelper::SetPacketsMinValue (double minValue)
{
  NS_LOG_FUNCTION (this << minValue);
  m_packetsMinValue = minValue;
}


double
SatStatsQueueHelper::GetPacketsMinValue () const
{
  return m_packetsMinValue;
}


void
SatStatsQueueHelper::SetPacketsMaxValue (double maxValue)
{
  NS_LOG_FUNCTION (this << maxValue);
  m_packetsMaxValue = maxValue;
}


double
SatStatsQueueHelper::GetPacketsMaxValue () const
{
  return m_packetsMaxValue;
}


void
SatStatsQueueHelper::SetPacketsBinLength (double binLength)
{
  NS_LOG_FUNCTION (this << binLength);
  m_packetsBinLength = binLength;
}


double
SatStatsQueueHelper::GetPacketsBinLength () const
{
  return m_packetsBinLength;
}


double
SatStatsQueueHelper::GetMinValue () const
{
  if (m_unitType == SatStatsQueueHelper::UNIT_BYTES)
    {
      return m_bytesMinValue;
    }
  else if (m_unitType == SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS)
    {
      return m_packetsMinValue;
    }
  else
    {
      NS_FATAL_ERROR ("SatStatsQueueHelper - Invalid unit type");
      return 0.0;
    }
}


double
SatStatsQueueHelper::GetMaxValue () const
{
  if (m_unitType == SatStatsQueueHelper::UNIT_BYTES)
    {
      return m_bytesMaxValue;
    }
  else if (m_unitType == SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS)
    {
      return m_packetsMaxValue;
    }
  else
    {
      NS_FATAL_ERROR ("SatStatsQueueHelper - Invalid unit type");
      return 0.0;
    }
}


double
SatStatsQueueHelper::GetBinLength () const
{
  if (m_unitType == SatStatsQueueHelper::UNIT_BYTES)
    {
      return m_bytesBinLength;
    }
  else if (m_unitType == SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS)
    {
      return m_packetsBinLength;
    }
  else
    {
      NS_FATAL_ERROR ("SatStatsQueueHelper - Invalid unit type");
      return 0.0;
    }
}


void
SatStatsQueueHelper::DoInstall ()
{
  NS_LOG_FUNCTION (this);

  switch (GetOutputType ())
    {
    case SatStatsHelper::OUTPUT_NONE:
      NS_FATAL_ERROR (GetOutputTypeName (GetOutputType ()) << " is not a valid output type for this statistics.");
      break;

    case SatStatsHelper::OUTPUT_SCALAR_FILE:
      {
        // Setup aggregator.
        m_aggregator = CreateAggregator ("ns3::MultiFileAggregator",
                                         "OutputFileName", StringValue (GetName () + ".txt"),
                                         "MultiFileMode", BooleanValue (false));

        // Setup collectors.
        m_terminalCollectors.SetType ("ns3::ScalarCollector");
        m_terminalCollectors.SetAttribute ("InputDataType",
                                           EnumValue (ScalarCollector::INPUT_DATA_TYPE_UINTEGER));
        m_terminalCollectors.SetAttribute ("OutputType",
                                           EnumValue (ScalarCollector::OUTPUT_TYPE_AVERAGE_PER_SAMPLE));
        CreateCollectorPerIdentifier (m_terminalCollectors);
        m_terminalCollectors.ConnectToAggregator ("Output",
                                                  m_aggregator,
                                                  &MultiFileAggregator::Write1d);
        break;
      }

    case SatStatsHelper::OUTPUT_SCATTER_FILE:
      {
        // Setup aggregator.
        const std::string heading = "% time_sec " + m_shortLabel;
        m_aggregator = CreateAggregator ("ns3::MultiFileAggregator",
                                         "OutputFileName", StringValue (GetName ()),
                                         "GeneralHeading", StringValue (heading));

        // Setup collectors.
        m_terminalCollectors.SetType ("ns3::UnitConversionCollector");
        m_terminalCollectors.SetAttribute ("ConversionType",
                                           EnumValue (UnitConversionCollector::TRANSPARENT));
        CreateCollectorPerIdentifier (m_terminalCollectors);
        m_terminalCollectors.ConnectToAggregator ("OutputTimeValue",
                                                  m_aggregator,
                                                  &MultiFileAggregator::Write2d);
        break;
      }

    case SatStatsHelper::OUTPUT_HISTOGRAM_FILE:
    case SatStatsHelper::OUTPUT_PDF_FILE:
    case SatStatsHelper::OUTPUT_CDF_FILE:
      {
        // Setup aggregator.
        const std::string heading = "% " + m_shortLabel + " freq";
        m_aggregator = CreateAggregator ("ns3::MultiFileAggregator",
                                         "OutputFileName", StringValue (GetName ()),
                                         "GeneralHeading", StringValue (heading));

        // Setup collectors.
        m_terminalCollectors.SetType ("ns3::DistributionCollector");
        DistributionCollector::OutputType_t outputType
          = DistributionCollector::OUTPUT_TYPE_HISTOGRAM;
        if (GetOutputType () == SatStatsHelper::OUTPUT_PDF_FILE)
          {
            outputType = DistributionCollector::OUTPUT_TYPE_PROBABILITY;
          }
        else if (GetOutputType () == SatStatsHelper::OUTPUT_CDF_FILE)
          {
            outputType = DistributionCollector::OUTPUT_TYPE_CUMULATIVE;
          }
        m_terminalCollectors.SetAttribute ("OutputType", EnumValue (outputType));
        m_terminalCollectors.SetAttribute ("MinValue", DoubleValue (GetMinValue ()));
        m_terminalCollectors.SetAttribute ("MaxValue", DoubleValue (GetMaxValue ()));
        m_terminalCollectors.SetAttribute ("BinLength", DoubleValue (GetBinLength ()));
        CreateCollectorPerIdentifier (m_terminalCollectors);
        m_terminalCollectors.ConnectToAggregator ("Output",
                                                  m_aggregator,
                                                  &MultiFileAggregator::Write2d);
        m_terminalCollectors.ConnectToAggregator ("OutputString",
                                                  m_aggregator,
                                                  &MultiFileAggregator::AddContextHeading);
        break;
      }

    case SatStatsHelper::OUTPUT_SCALAR_PLOT:
      /// \todo Add support for boxes in Gnuplot.
      NS_FATAL_ERROR (GetOutputTypeName (GetOutputType ()) << " is not a valid output type for this statistics.");
      break;

    case SatStatsHelper::OUTPUT_SCATTER_PLOT:
      {
        // Setup aggregator.
        Ptr<GnuplotAggregator> plotAggregator = CreateObject<GnuplotAggregator> (GetName ());
        //plot->SetTitle ("");
        plotAggregator->SetLegend ("Time (in seconds)", m_longLabel);
        plotAggregator->Set2dDatasetDefaultStyle (Gnuplot2dDataset::LINES);
        m_aggregator = plotAggregator;

        // Setup collectors.
        m_terminalCollectors.SetType ("ns3::UnitConversionCollector");
        m_terminalCollectors.SetAttribute ("ConversionType",
                                           EnumValue (UnitConversionCollector::TRANSPARENT));
        CreateCollectorPerIdentifier (m_terminalCollectors);
        for (CollectorMap::Iterator it = m_terminalCollectors.Begin ();
             it != m_terminalCollectors.End (); ++it)
          {
            const std::string context = it->second->GetName ();
            plotAggregator->Add2dDataset (context, context);
          }
        m_terminalCollectors.ConnectToAggregator ("OutputTimeValue",
                                                  m_aggregator,
                                                  &GnuplotAggregator::Write2d);
        break;
      }

    case SatStatsHelper::OUTPUT_HISTOGRAM_PLOT:
    case SatStatsHelper::OUTPUT_PDF_PLOT:
    case SatStatsHelper::OUTPUT_CDF_PLOT:
      {
        // Setup aggregator.
        Ptr<GnuplotAggregator> plotAggregator = CreateObject<GnuplotAggregator> (GetName ());
        //plot->SetTitle ("");
        plotAggregator->SetLegend (m_longLabel, "Frequency");
        plotAggregator->Set2dDatasetDefaultStyle (Gnuplot2dDataset::LINES);
        m_aggregator = plotAggregator;

        // Setup collectors.
        m_terminalCollectors.SetType ("ns3::DistributionCollector");
        DistributionCollector::OutputType_t outputType
          = DistributionCollector::OUTPUT_TYPE_HISTOGRAM;
        if (GetOutputType () == SatStatsHelper::OUTPUT_PDF_PLOT)
          {
            outputType = DistributionCollector::OUTPUT_TYPE_PROBABILITY;
          }
        else if (GetOutputType () == SatStatsHelper::OUTPUT_CDF_PLOT)
          {
            outputType = DistributionCollector::OUTPUT_TYPE_CUMULATIVE;
          }
        m_terminalCollectors.SetAttribute ("OutputType", EnumValue (outputType));
        m_terminalCollectors.SetAttribute ("MinValue", DoubleValue (GetMinValue ()));
        m_terminalCollectors.SetAttribute ("MaxValue", DoubleValue (GetMaxValue ()));
        m_terminalCollectors.SetAttribute ("BinLength", DoubleValue (GetBinLength ()));
        CreateCollectorPerIdentifier (m_terminalCollectors);
        for (CollectorMap::Iterator it = m_terminalCollectors.Begin ();
             it != m_terminalCollectors.End (); ++it)
          {
            const std::string context = it->second->GetName ();
            plotAggregator->Add2dDataset (context, context);
          }
        m_terminalCollectors.ConnectToAggregator ("Output",
                                                  m_aggregator,
                                                  &GnuplotAggregator::Write2d);
        break;
      }

    default:
      NS_FATAL_ERROR ("SatStatsRtnQueueBytesHelper - Invalid output type");
      break;
    }

  // Identify the list of source of queue events.
  EnlistSource ();

  // Schedule the first polling session.
  Simulator::Schedule (m_pollInterval, &SatStatsQueueHelper::Poll, this);

} // end of `void DoInstall ();`


void
SatStatsQueueHelper::EnlistSource ()
{
  NS_LOG_FUNCTION (this);

  // The method below is supposed to be implemented by the child class.
  DoEnlistSource ();
}


void
SatStatsQueueHelper::Poll ()
{
  NS_LOG_FUNCTION (this);

  // The method below is supposed to be implemented by the child class.
  DoPoll ();

  // Schedule the next polling session.
  Simulator::Schedule (m_pollInterval, &SatStatsQueueHelper::Poll, this);
}


void
SatStatsQueueHelper::PushToCollector (uint32_t identifier, uint32_t value)
{
  //NS_LOG_FUNCTION (this << identifier << value);

  // Find the collector with the right identifier.
  Ptr<DataCollectionObject> collector = m_terminalCollectors.Get (identifier);
  NS_ASSERT_MSG (collector != 0,
                 "Unable to find collector with identifier " << identifier);

  switch (GetOutputType ())
    {
    case OUTPUT_SCALAR_FILE:
    case OUTPUT_SCALAR_PLOT:
      {
        Ptr<ScalarCollector> c = collector->GetObject<ScalarCollector> ();
        NS_ASSERT (c != 0);
        c->TraceSinkUinteger32 (0, value);
        break;
      }

    case OUTPUT_SCATTER_FILE:
    case OUTPUT_SCATTER_PLOT:
      {
        Ptr<UnitConversionCollector> c = collector->GetObject<UnitConversionCollector> ();
        NS_ASSERT (c != 0);
        c->TraceSinkUinteger32 (0, value);
        break;
      }

    case OUTPUT_HISTOGRAM_FILE:
    case OUTPUT_HISTOGRAM_PLOT:
    case OUTPUT_PDF_FILE:
    case OUTPUT_PDF_PLOT:
    case OUTPUT_CDF_FILE:
    case OUTPUT_CDF_PLOT:
      {
        Ptr<DistributionCollector> c = collector->GetObject<DistributionCollector> ();
        NS_ASSERT (c != 0);
        c->TraceSinkUinteger32 (0, value);
        break;
      }

    default:
      NS_FATAL_ERROR (GetOutputTypeName (GetOutputType ()) << " is not a valid output type for this statistics.");
      break;

    } // end of `switch (GetOutputType ())`

} // end of `void PushToCollector (uint32_t, uint32_t)`


// FORWARD LINK ///////////////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsFwdQueueHelper);

SatStatsFwdQueueHelper::SatStatsFwdQueueHelper (Ptr<const SatHelper> satHelper)
  : SatStatsQueueHelper (satHelper)
{
  NS_LOG_FUNCTION (this << satHelper);
}


SatStatsFwdQueueHelper::~SatStatsFwdQueueHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsFwdQueueHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsFwdQueueHelper")
    .SetParent<SatStatsQueueHelper> ()
  ;
  return tid;
}


void
SatStatsFwdQueueHelper::DoEnlistSource ()
{
  NS_LOG_FUNCTION (this);

  const SatIdMapper * satIdMapper = Singleton<SatIdMapper>::Get ();

  NodeContainer gws = GetSatHelper ()->GetBeamHelper ()->GetGwNodes ();
  for (NodeContainer::Iterator it1 = gws.Begin(); it1 != gws.End (); ++it1)
    {
      NetDeviceContainer devs = GetGwSatNetDevice (*it1);

      for (NetDeviceContainer::Iterator itDev = devs.Begin ();
           itDev != devs.End (); ++itDev)
        {
          Ptr<SatNetDevice> satDev = (*itDev)->GetObject<SatNetDevice> ();
          NS_ASSERT (satDev != 0);

          // Get the beam ID of this device.
          Ptr<SatPhy> satPhy = satDev->GetPhy ();
          NS_ASSERT (satPhy != 0);
          Ptr<SatPhyRx> satPhyRx = satPhy->GetPhyRx ();
          NS_ASSERT (satPhyRx != 0);
          const uint32_t beamId = satPhyRx->GetBeamId ();
          NS_LOG_DEBUG (this << " enlisting UT from beam ID " << beamId);

          // Go through the UTs of this beam.
          ListOfUt_t listOfUt;
          NodeContainer uts = GetSatHelper ()->GetBeamHelper ()->GetUtNodes (beamId);
          for (NodeContainer::Iterator it2 = uts.Begin();
               it2 != uts.End (); ++it2)
            {
              const Address addr = satIdMapper->GetUtMacWithNode (*it2);
              const Mac48Address mac48Addr = Mac48Address::ConvertFrom (addr);

              if (addr.IsInvalid ())
                {
                  NS_LOG_WARN (this << " Node " << (*it2)->GetId ()
                                    << " is not a valid UT");
                }
              else
                {
                  const uint32_t identifier = GetIdentifierForUt (*it2);
                  listOfUt.push_back (std::make_pair (mac48Addr, identifier));
                }
            }

          // Add an entry to the LLC list.
          Ptr<SatLlc> satLlc = satDev->GetLlc ();
          NS_ASSERT (satLlc != 0);
          m_llc.push_back (std::make_pair (satLlc, listOfUt));

        } // end of `for (NetDeviceContainer::Iterator itDev = devs)`

    } // end of `for (it1 = gws.Begin(); it1 != gws.End (); ++it1)`

} // end of `void DoInstall ();`


void
SatStatsFwdQueueHelper::DoPoll ()
{
  //NS_LOG_FUNCTION (this);

  // Go through the LLC list.
  std::list<std::pair<Ptr<SatLlc>, ListOfUt_t> >::const_iterator it1;
  for (it1 = m_llc.begin (); it1 != m_llc.end (); ++it1)
    {
      for (ListOfUt_t::const_iterator it2 = it1->second.begin ();
           it2 != it1->second.end (); ++it2)
        {
          const Mac48Address addr = it2->first;
          const uint32_t identifier = it2->second;
          if (GetUnitType () == SatStatsQueueHelper::UNIT_BYTES)
            {
              PushToCollector (identifier,
                               it1->first->GetNBytesInQueue (addr));
            }
          else
            {
              NS_ASSERT (GetUnitType () == SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS);
              PushToCollector (identifier,
                               it1->first->GetNPacketsInQueue (addr));
            }
        }
    }
}


// FORWARD LINK IN BYTES //////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsFwdQueueBytesHelper);

SatStatsFwdQueueBytesHelper::SatStatsFwdQueueBytesHelper (Ptr<const SatHelper> satHelper)
  : SatStatsFwdQueueHelper (satHelper)
{
  NS_LOG_FUNCTION (this << satHelper);
  SetUnitType (SatStatsQueueHelper::UNIT_BYTES);
}


SatStatsFwdQueueBytesHelper::~SatStatsFwdQueueBytesHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsFwdQueueBytesHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsFwdQueueBytesHelper")
    .SetParent<SatStatsFwdQueueHelper> ()
  ;
  return tid;
}


// FORWARD LINK IN PACKETS ////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsFwdQueuePacketsHelper);

SatStatsFwdQueuePacketsHelper::SatStatsFwdQueuePacketsHelper (Ptr<const SatHelper> satHelper)
  : SatStatsFwdQueueHelper (satHelper)
{
  NS_LOG_FUNCTION (this << satHelper);
  SetUnitType (SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS);
}


SatStatsFwdQueuePacketsHelper::~SatStatsFwdQueuePacketsHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsFwdQueuePacketsHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsFwdQueuePacketsHelper")
    .SetParent<SatStatsFwdQueueHelper> ()
  ;
  return tid;
}


// RETURN LINK ////////////////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsRtnQueueHelper);

SatStatsRtnQueueHelper::SatStatsRtnQueueHelper (Ptr<const SatHelper> satHelper)
  : SatStatsQueueHelper (satHelper)
{
  NS_LOG_FUNCTION (this << satHelper);
}


SatStatsRtnQueueHelper::~SatStatsRtnQueueHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsRtnQueueHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsRtnQueueHelper")
    .SetParent<SatStatsQueueHelper> ()
  ;
  return tid;
}


void
SatStatsRtnQueueHelper::DoEnlistSource ()
{
  NS_LOG_FUNCTION (this);

  NodeContainer uts = GetSatHelper ()->GetBeamHelper ()->GetUtNodes ();
  for (NodeContainer::Iterator it = uts.Begin(); it != uts.End (); ++it)
    {
      const uint32_t identifier = GetIdentifierForUt (*it);
      Ptr<NetDevice> dev = GetUtSatNetDevice (*it);
      Ptr<SatNetDevice> satDev = dev->GetObject<SatNetDevice> ();
      NS_ASSERT (satDev != 0);
      Ptr<SatLlc> satLlc = satDev->GetLlc ();
      NS_ASSERT (satLlc != 0);
      m_llc.push_back (std::make_pair (satLlc, identifier));
    }

} // end of `void DoInstall ();`


void
SatStatsRtnQueueHelper::DoPoll ()
{
  //NS_LOG_FUNCTION (this);

  // Go through the LLC list.
  std::list<std::pair<Ptr<SatLlc>, uint32_t> >::const_iterator it;
  for (it = m_llc.begin (); it != m_llc.end (); ++it)
    {
      if (GetUnitType () == SatStatsQueueHelper::UNIT_BYTES)
        {
          PushToCollector (it->second, it->first->GetNBytesInQueue ());
        }
      else
        {
          NS_ASSERT (GetUnitType () == SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS);
          PushToCollector (it->second, it->first->GetNPacketsInQueue ());
        }
    }
}

// RETURN LINK IN BYTES ///////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsRtnQueueBytesHelper);

SatStatsRtnQueueBytesHelper::SatStatsRtnQueueBytesHelper (Ptr<const SatHelper> satHelper)
  : SatStatsRtnQueueHelper (satHelper)
{
  NS_LOG_FUNCTION (this << satHelper);
  SetUnitType (SatStatsQueueHelper::UNIT_BYTES);
}


SatStatsRtnQueueBytesHelper::~SatStatsRtnQueueBytesHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsRtnQueueBytesHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsRtnQueueBytesHelper")
    .SetParent<SatStatsRtnQueueHelper> ()
  ;
  return tid;
}


// RETURN LINK IN PACKETS /////////////////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (SatStatsRtnQueuePacketsHelper);

SatStatsRtnQueuePacketsHelper::SatStatsRtnQueuePacketsHelper (Ptr<const SatHelper> satHelper)
  : SatStatsRtnQueueHelper (satHelper)
{
  NS_LOG_FUNCTION (this << satHelper);
  SetUnitType (SatStatsQueueHelper::UNIT_NUMBER_OF_PACKETS);
}


SatStatsRtnQueuePacketsHelper::~SatStatsRtnQueuePacketsHelper ()
{
  NS_LOG_FUNCTION (this);
}


TypeId // static
SatStatsRtnQueuePacketsHelper::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::SatStatsRtnQueuePacketsHelper")
    .SetParent<SatStatsRtnQueueHelper> ()
  ;
  return tid;
}


} // end of namespace ns3
