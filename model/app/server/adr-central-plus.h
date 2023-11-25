#ifndef ADR_CENTRAL_PLUS_H
#define ADR_CENTRAL_PLUS_H

#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/network-status.h"
#include "ns3/network-controller-components.h"
#include "ns3/adr-lorawan.h"

namespace ns3 {
namespace lorawan {

////////////////////////////////////////
// LinkAdrRequest commands management //
////////////////////////////////////////

class AdrCentralPlus : public AdrLorawan
{
  
public:
  static TypeId GetTypeId (void);

  //Constructor
  AdrCentralPlus ();
  //Destructor
  virtual ~AdrCentralPlus ();

  
private:
  double ImplementationCore(Ptr<EndDeviceStatus> status) override;

  double GetMedian(std::vector<double> values);
  double GetFirstQuartile(std::vector<double> values);
  double GetThirdQuartile(std::vector<double> values);
  std::vector<double> RemoveOutliers(std::vector<double> values, double q1, double q3);
 

};
}
}

#endif
