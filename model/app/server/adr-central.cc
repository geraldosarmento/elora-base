#include "ns3/adr-central.h"

namespace ns3 {
namespace lorawan {

////////////////////////////////////////
// LinkAdrRequest commands management //
////////////////////////////////////////

NS_LOG_COMPONENT_DEFINE ("AdrCentral");

NS_OBJECT_ENSURE_REGISTERED (AdrCentral);

TypeId AdrCentral::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AdrCentral")
    .SetGroupName ("lorawan")
    .AddConstructor<AdrCentral> ()
    .SetParent<AdrLorawan> ()    
  ;
  return tid;
}

AdrCentral::AdrCentral ()
{
}

AdrCentral::~AdrCentral ()
{
}

//testar
/*
void
AdrCentral::AdrImplementation(uint8_t* newDataRate,
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
        transmissionPower -= 2;
        steps--;
        NS_LOG_DEBUG("Decreased Ptx by 2");
    }
    while (steps < 0 && transmissionPower < max_transmissionPower)
    {
        transmissionPower += 2;
        steps++;
        NS_LOG_DEBUG("Increased Ptx by 2");
    }

    *newDataRate = currDataRate;
    *newTxPower = transmissionPower;
}

*/

double AdrCentral::ImplementationCore(Ptr<EndDeviceStatus> status)  {
    double m_SNR = 0;
    std::vector<double> curSNR(historyRange);
    double fq, tq = 0;
    
    //std::cout << "ADR Central implementation core activated..." << std::endl;
    EndDeviceStatus::ReceivedPacketList packetList = status->GetReceivedPacketList ();
    auto it = packetList.rbegin ();
        
    for (int i = 0; i < historyRange; i++, it++)    
        curSNR[i] = LoraPhy::RxPowerToSNR (GetReceivedPower (it->second.gwList));

    
    
    fq = GetFirstQuartile(curSNR);
    tq = GetThirdQuartile(curSNR);
    curSNR = RemoveOutliers(curSNR, fq, tq);   
    m_SNR = GetMedian(curSNR);

    return m_SNR;
}

/*

double AdrCentral::ImplementationCore(Ptr<EndDeviceStatus> status)  {
    double m_SNR = 0;
    std::vector<double> curSNR(historyRange);
    double fq, tq = 0;
    
    //std::cout << "ADR Central implementation core activated..." << std::endl;
    EndDeviceStatus::ReceivedPacketList packetList = status->GetReceivedPacketList ();
    auto it = packetList.rbegin ();
        
    for (int i = 0; i < historyRange; i++, it++)    
        curSNR[i] = LoraPhy::RxPowerToSNR (GetReceivedPower (it->second.gwList));

    
    m_SNR = GetMedian(curSNR);

    return m_SNR;
}

*/


////

double AdrCentral::GetMedian(std::vector<double> values) {
    // Sort the vector
    std::sort(values.begin(), values.end());

    int size = values.size();

    // Check if the number of elements is odd
    if (size % 2 != 0) {
      return values[size / 2];
    } else {
        // If the number of elements is even, the median is the average of the two middle elements
      return (values[(size - 1) / 2] + values[size / 2]) / 2.0;
    }
}

double AdrCentral::GetFirstQuartile(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    int size = values.size();

    if (size % 2 != 0) {
        // If the number of elements is odd
        int index = size / 4;
        return values[index];
    } else {
        // If the number of elements is even
        int index = size / 4;
        double lowerValue = values[index - 1];
        double upperValue = values[index];
        return (lowerValue + upperValue) / 2.0;
    }
}

double AdrCentral::GetThirdQuartile(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    int size = values.size();

    if (size % 2 != 0) {
        // If the number of elements is odd
        int index = (3 * size) / 4;
        return values[index];
    } else {
        // If the number of elements is even
        int index = (3 * size) / 4;
        double lowerValue = values[index - 1];
        double upperValue = values[index];
        return (lowerValue + upperValue) / 2.0;
    }
}

std::vector<double> AdrCentral::RemoveOutliers(std::vector<double> values, double q1, double q3) {
    // Calculate the interquartile range (IQR)
    double iqr = q3 - q1;

    // Calculate the lower and upper bounds for outliers
    double lowerBound = q1 - 0.5 * iqr;
    double upperBound = q3 + 0.5 * iqr;

    // Remove outliers from the vector
    values.erase(std::remove_if(values.begin(), values.end(),
                    [lowerBound, upperBound](double x) { return x < lowerBound || x > upperBound; }),
                    values.end());

    return values;
}

}
}

