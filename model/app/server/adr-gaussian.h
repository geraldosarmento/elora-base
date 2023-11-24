#ifndef ADR_GAUSSIAN_H
#define ADR_GAUSSIAN_H

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

class AdrGaussian : public AdrLorawan
{
  
public:
  static TypeId GetTypeId (void);

  //Constructor
  AdrGaussian ();
  //Destructor
  virtual ~AdrGaussian ();

  
private:
  double ImplementationCore(Ptr<EndDeviceStatus> status) override;

  void BeforeSendingReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus) override;

};
}
}

#endif
