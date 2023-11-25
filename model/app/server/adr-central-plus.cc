#include "ns3/adr-central-plus.h"

namespace ns3 {
namespace lorawan {

////////////////////////////////////////
// LinkAdrRequest commands management //
////////////////////////////////////////

NS_LOG_COMPONENT_DEFINE ("AdrCentralPlus");

NS_OBJECT_ENSURE_REGISTERED (AdrCentralPlus);

TypeId AdrCentralPlus::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AdrCentralPlus")
    .SetGroupName ("lorawan")
    .AddConstructor<AdrCentralPlus> ()
    .SetParent<AdrLorawan> ()    
  ;
  return tid;
}

AdrCentralPlus::AdrCentralPlus ()
{
}

AdrCentralPlus::~AdrCentralPlus ()
{
}

double AdrCentralPlus::ImplementationCore(Ptr<EndDeviceStatus> status)  {
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


////

double AdrCentralPlus::GetMedian(std::vector<double> values) {
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

double AdrCentralPlus::GetFirstQuartile(std::vector<double> values) {
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

double AdrCentralPlus::GetThirdQuartile(std::vector<double> values) {
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

std::vector<double> AdrCentralPlus::RemoveOutliers(std::vector<double> values, double q1, double q3) {
    // Calculate the interquartile range (IQR)
    double iqr = q3 - q1;

    // Calculate the lower and upper bounds for outliers
    double lowerBound = q1 - 0.7 * iqr;
    double upperBound = q3 + 0.7 * iqr;

    // Remove outliers from the vector
    values.erase(std::remove_if(values.begin(), values.end(),
                    [lowerBound, upperBound](double x) { return x < lowerBound || x > upperBound; }),
                    values.end());

    return values;
}

}
}

