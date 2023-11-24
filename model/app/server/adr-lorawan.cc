/*
 * Copyright (c) 2018 University of Padova
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
 * Author: Matteo Perin <matteo.perin.2@studenti.unipd.it
 *
 * 23/12/2022
 * Modified by: Alessandro Aimi <alessandro.aimi@orange.com>
 *                              <alessandro.aimi@cnam.fr>
 */

#include "adr-lorawan.h"

namespace ns3
{
namespace lorawan
{

////////////////////////////////////////
// LinkAdrRequest commands management //
////////////////////////////////////////

NS_LOG_COMPONENT_DEFINE("AdrLorawan");

NS_OBJECT_ENSURE_REGISTERED(AdrLorawan);

TypeId
AdrLorawan::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AdrLorawan")
            .SetGroupName("lorawan")
            .AddConstructor<AdrLorawan>()
            .SetParent<NetworkControllerComponent>()
            .AddAttribute("MultipleGwCombiningMethod",
                          "Whether to average the received power of gateways or to use the maximum",
                          EnumValue(AdrLorawan::MAXIMUM),
                          MakeEnumAccessor(&AdrLorawan::tpAveraging),
                          MakeEnumChecker(AdrLorawan::AVERAGE,
                                          "avg",
                                          AdrLorawan::MAXIMUM,
                                          "max",
                                          AdrLorawan::MINIMUM,
                                          "min"))
            .AddAttribute("MultiplePacketsCombiningMethod",
                          "Whether to average SNRs from multiple packets or to use the maximum",
                          EnumValue(AdrLorawan::MAXIMUM),
                          MakeEnumAccessor(&AdrLorawan::historyAveraging),
                          MakeEnumChecker(AdrLorawan::AVERAGE,
                                          "avg",
                                          AdrLorawan::MAXIMUM,
                                          "max",
                                          AdrLorawan::MINIMUM,
                                          "min"))
            .AddAttribute("HistoryRange",
                          "Number of packets to use for averaging",
                          IntegerValue(20),
                          MakeIntegerAccessor(&AdrLorawan::historyRange),
                          MakeIntegerChecker<int>(0, 100))
            .AddAttribute("ChangeTransmissionPower",
                          "Whether to toggle the transmission power or not",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrLorawan::m_toggleTxPower),
                          MakeBooleanChecker())
            .AddAttribute("SNRDeviceMargin",
                          "Additional SNR margin needed to decrease SF/TxPower",
                          DoubleValue(10.0),
                          MakeDoubleAccessor(&AdrLorawan::m_deviceMargin),
                          MakeDoubleChecker<double>())
            .AddAttribute("RawSNRFactor",
                          "Factor that imposes a real variation on SNR values",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&AdrLorawan::m_rawSNRFactor),
                          MakeDoubleChecker<double>());
    return tid;
}

AdrLorawan::AdrLorawan()
{
    NS_LOG_FUNCTION(this);
}

AdrLorawan::~AdrLorawan()
{
    NS_LOG_FUNCTION(this);
}

void
AdrLorawan::OnReceivedPacket(Ptr<const Packet> packet,
                               Ptr<EndDeviceStatus> status,
                               Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << packet << networkStatus);

    // We will only act just before reply, when all Gateways will have received
    // the packet, since we need their respective received power.
}

void
AdrLorawan::BeforeSendingReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << status << networkStatus);

    Ptr<Packet> myPacket = status->GetLastPacketReceivedFromDevice()->Copy();
    LorawanMacHeader mHdr;
    myPacket->RemoveHeader(mHdr);
    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    myPacket->RemoveHeader(fHdr);

    // Execute the ADR algotithm only if the request bit is set
    if (fHdr.GetAdr())
    {
        if (int(status->GetReceivedPacketList().size()) < historyRange)
        {
            NS_LOG_ERROR("Not enough packets received by this device ("
                         << status->GetReceivedPacketList().size()
                         << ") for the algorithm to work (need " << historyRange << ")");
        }
        else
        {
            NS_LOG_DEBUG("New ADR request");

            // Get the DR used by the device
            uint8_t currDataRate = status->GetFirstReceiveWindowDataRate();

            // Get the device transmission power (dBm)
            uint8_t transmissionPower = status->GetMac()->GetTransmissionPower();

            // New parameters for the end-device
            uint8_t newDataRate;
            uint8_t newTxPower;

            // ADR Algorithm
            AdrImplementation(&newDataRate, &newTxPower, status);

            // Change the power back to the default if we don't want to change it
            if (!m_toggleTxPower)
            {
                newTxPower = transmissionPower;
            }

            if (newDataRate != currDataRate || newTxPower != transmissionPower)
            {
                // Create a list with mandatory channel indexes
                int channels[] = {0, 1, 2};
                std::list<int> enabledChannels(channels, channels + sizeof(channels) / sizeof(int));

                // Repetitions Setting
                const int rep = 1;

                NS_LOG_DEBUG("Sending LinkAdrReq with DR = " << (unsigned)newDataRate
                                                             << " and TP = " << (unsigned)newTxPower
                                                             << " dBm");

                status->m_reply.frameHeader.AddLinkAdrReq(newDataRate,
                                                          GetTxPowerIndex(newTxPower),
                                                          enabledChannels,
                                                          rep);
                status->m_reply.frameHeader.SetAsDownlink();
                status->m_reply.macHeader.SetFType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);

                status->m_reply.needsReply = true;
            }
            else
            {
                NS_LOG_DEBUG("Skipped request");
            }
        }
    }
    else
    {
        // Do nothing
    }
}

void
AdrLorawan::OnFailedReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << networkStatus);
}

void
AdrLorawan::AdrImplementation(uint8_t* newDataRate,
                                uint8_t* newTxPower,
                                Ptr<EndDeviceStatus> status)
{
    // Compute the maximum or median SNR, based on the boolean value historyAveraging
    double m_SNR = 0;
    
    m_SNR = ImplementationCore(status);

    NS_LOG_DEBUG("m_SNR = " << m_SNR);

    // Get the SF used by the device
    uint8_t currDataRate = status->GetFirstReceiveWindowDataRate();

    NS_LOG_DEBUG("DR = " << (unsigned)currDataRate);

    // Get the device data rate and use it to get the SNR demodulation treshold
    double req_SNR = treshold[currDataRate];

    NS_LOG_DEBUG("Required SNR = " << req_SNR);

    // Get the device transmission power (dBm)
    double transmissionPower = status->GetMac()->GetTransmissionPower();

    NS_LOG_DEBUG("Transmission Power = " << transmissionPower);

    // Compute the SNR margin taking into consideration the SNR of
    // previously received packets
    double margin_SNR = m_SNR - req_SNR - m_deviceMargin;

    NS_LOG_DEBUG("Margin = " << margin_SNR);

    // Number of steps to decrement the SF (thereby increasing the Data Rate)
    // and the TP.
    int steps = std::floor(margin_SNR / 3);

    NS_LOG_DEBUG("steps = " << steps);

    // If the number of steps is positive (margin_SNR is positive, so its
    // decimal value is high) increment the data rate, if there are some
    // leftover steps after reaching the maximum possible data rate
    //(corresponding to the minimum SF) decrement the transmission power as
    // well for the number of steps left.
    // If, on the other hand, the number of steps is negative (margin_SNR is
    // negative, so its decimal value is low) increase the transmission power
    //(note that the SF is not incremented as this particular algorithm
    // expects the node itself to raise its SF whenever necessary).
    while (steps > 0 && currDataRate < max_dataRate)
    {
        currDataRate++;
        steps--;
        NS_LOG_DEBUG("Increased DR by 1");
    }
    while (steps > 0 && transmissionPower > min_transmissionPower)
    {
        transmissionPower -= 3;
        steps--;
        NS_LOG_DEBUG("Decreased Ptx by 3");
    }
    while (steps < 0 && transmissionPower < max_transmissionPower)
    {
        transmissionPower += 3;
        steps++;
        NS_LOG_DEBUG("Increased Ptx by 3");
    }

    *newDataRate = currDataRate;
    *newTxPower = transmissionPower;
}

double AdrLorawan::ImplementationCore(Ptr<EndDeviceStatus> status)  {
    double m_SNR = 0;
    
    //std::cout << "ADR implementation core activated..." << std::endl;
    
    m_SNR = GetMaxSNR(status->GetReceivedPacketList(), historyRange);

    return m_SNR;
}

// Get the maximum received power (it considers the values in dB!)
double
AdrLorawan::GetMinTxFromGateways(EndDeviceStatus::GatewayList gwList)
{
    EndDeviceStatus::GatewayList::iterator it = gwList.begin();
    double min = it->second.rxPower;

    for (; it != gwList.end(); it++)
    {
        if (it->second.rxPower < min)
        {
            min = it->second.rxPower;
        }
    }

    return min;
}

// Get the maximum received power (it considers the values in dB!)
double
AdrLorawan::GetMaxTxFromGateways(EndDeviceStatus::GatewayList gwList)
{
    EndDeviceStatus::GatewayList::iterator it = gwList.begin();
    double max = it->second.rxPower;

    for (; it != gwList.end(); it++)
    {
        if (it->second.rxPower > max)
        {
            max = it->second.rxPower;
        }
    }

    return max;
}

// Get the maximum received power
double
AdrLorawan::GetAverageTxFromGateways(EndDeviceStatus::GatewayList gwList)
{
    double sum = 0;

    for (EndDeviceStatus::GatewayList::iterator it = gwList.begin(); it != gwList.end(); it++)
    {
        NS_LOG_DEBUG("Gateway at " << it->first << " has TP " << it->second.rxPower);
        sum += it->second.rxPower;
    }

    double average = sum / gwList.size();

    NS_LOG_DEBUG("TP (average) = " << average);

    return average;
}

double
AdrLorawan::GetReceivedPower(EndDeviceStatus::GatewayList gwList)
{
    switch (tpAveraging)
    {
    case AdrLorawan::AVERAGE:
        return GetAverageTxFromGateways(gwList);
    case AdrLorawan::MAXIMUM:
        return GetMaxTxFromGateways(gwList);
    case AdrLorawan::MINIMUM:
        return GetMinTxFromGateways(gwList);
    default:
        return -1;
    }
}

// TODO Make this more elegant
double
AdrLorawan::GetMinSNR(const EndDeviceStatus::ReceivedPacketList& packetList, int historyRange)
{
    double m_SNR;

    // Take elements from the list starting at the end
    auto it = packetList.rbegin();
    double min = LoraPhy::RxPowerToSNR(GetReceivedPower(it->second.gwList));

    for (int i = 0; i < historyRange; i++, it++)
    {
        m_SNR = LoraPhy::RxPowerToSNR(GetReceivedPower(it->second.gwList));

        NS_LOG_DEBUG("Received power: " << GetReceivedPower(it->second.gwList));
        NS_LOG_DEBUG("m_SNR = " << m_SNR);

        if (m_SNR < min)
        {
            min = m_SNR;
        }
    }

    NS_LOG_DEBUG("SNR (min) = " << min);

    return min;
}

double
AdrLorawan::GetMaxSNR(const EndDeviceStatus::ReceivedPacketList& packetList, int historyRange)
{
    double m_SNR;

    // Take elements from the list starting at the end
    auto it = packetList.rbegin();
    double max = LoraPhy::RxPowerToSNR(GetReceivedPower(it->second.gwList));

    for (int i = 0; i < historyRange; i++, it++)
    {
        m_SNR = LoraPhy::RxPowerToSNR(GetReceivedPower(it->second.gwList));

        NS_LOG_DEBUG("Received power: " << GetReceivedPower(it->second.gwList));
        NS_LOG_DEBUG("m_SNR = " << m_SNR);

        if (m_SNR > max)
        {
            max = m_SNR;
        }
    }

    NS_LOG_DEBUG("SNR (max) = " << max);

    return max;
}

double
AdrLorawan::GetAverageSNR(const EndDeviceStatus::ReceivedPacketList& packetList, int historyRange)
{
    double sum = 0;
    double m_SNR;

    // Take elements from the list starting at the end
    auto it = packetList.rbegin();
    for (int i = 0; i < historyRange; i++, it++)
    {
        m_SNR = LoraPhy::RxPowerToSNR(GetReceivedPower(it->second.gwList));

        NS_LOG_DEBUG("Received power: " << GetReceivedPower(it->second.gwList));
        NS_LOG_DEBUG("m_SNR = " << m_SNR);

        sum += m_SNR;
    }

    double average = sum / historyRange;

    NS_LOG_DEBUG("SNR (average) = " << average);

    return average;
}

int
AdrLorawan::GetTxPowerIndex(int txPower)
{
    if (txPower >= 14)
    {
        return 0;
    }
    else if (txPower >= 12)
    {
        return 1;
    }
    else if (txPower >= 10)
    {
        return 2;
    }
    else if (txPower >= 8)
    {
        return 3;
    }
    else if (txPower >= 6)
    {
        return 4;
    }
    else if (txPower >= 4)
    {
        return 5;
    }
    else if (txPower >= 2)
    {
        return 6;
    }
    else
    {
        return 7;
    }
}

double AdrLorawan::GetSdSNR(EndDeviceStatus::ReceivedPacketList packetList,
                        int historyRange)
{
  double average = GetAverageSNR(packetList, historyRange);
  double sumOfSquares = 0;

  // Calculate the sum of squares of differences from the mean
  auto it = packetList.rbegin();
  for (int i = 0; i < historyRange; i++, it++)
  {
    double m_SNR = LoraPhy::RxPowerToSNR(GetReceivedPower(it->second.gwList));
    double diff = m_SNR - average;
    sumOfSquares += diff * diff;
  }

  double variance = sumOfSquares / (historyRange);
  double sdSNR = std::sqrt(variance);

  NS_LOG_DEBUG("SNR (standard deviation) = " << sdSNR);

  return sdSNR;
}

double AdrLorawan::VecAVG(const std::vector<double>& values) {
    // Verifica se o vetor está vazio para evitar divisão por zero
    if (values.empty()) {
        std::cerr << "Error: Cannot calculate average of an empty vector." << std::endl;
        return 0.0;  // Retornar 0.0 neste caso, mas você pode escolher outra abordagem
    }

    // Soma todos os elementos do vetor
    double sum = 0.0;
    for (const double& value : values) {
        sum += value;
    }

    // Calcula a média dividindo a soma pelo número de elementos
    double average = sum / static_cast<double>(values.size());

    return average;
}

double AdrLorawan::VecSD(const std::vector<double>& values, double mean) {
    // Verifica se o vetor está vazio para evitar problemas
    if (values.empty()) {
        std::cerr << "Error: Cannot calculate standard deviation of an empty vector." << std::endl;
        return 0.0;  // Retorna 0.0 neste caso, mas você pode escolher outra abordagem
    }

    // Calcula a soma dos quadrados das diferenças entre cada elemento e a média
    double sumOfSquaredDifferences = 0.0;
    for (const double& value : values) {
        double difference = value - mean;
        sumOfSquaredDifferences += difference * difference;
    }

    // Calcula a variância dividindo a soma pelo número de elementos
    double variance = sumOfSquaredDifferences / static_cast<double>(values.size());

    // Calcula o desvio padrão como a raiz quadrada da variância
    double standardDeviation = std::sqrt(variance);

    return standardDeviation;
}

std::vector<double> AdrLorawan::GetRealisticSNR(double mean, int historyRange) {
    std::random_device rd;
    std::mt19937 generator(rd());

    
    std::uniform_real_distribution<double> uniformRawSNRFactor(0.0, m_rawSNRFactor);
    double factor = uniformRawSNRFactor(generator);

    // Usa o valor gerado para m_rawSNRFactor na distribuição uniforme para SNR
    std::uniform_real_distribution<double> uniform(mean - factor, mean + factor);

    std::vector<double> rawSNR;

    for (int i = 0; i < historyRange; ++i) {
        double SNR = uniform(generator);
        rawSNR.push_back(SNR);
    }

    return rawSNR;
}

} // namespace lorawan
} // namespace ns3
