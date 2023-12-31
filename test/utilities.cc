#include "utilities.h"

namespace ns3
{
namespace lorawan
{

Ptr<LoraChannel>
CreateChannel()
{
    // Create the lora channel object
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    return CreateObject<LoraChannel>(loss, delay);
}

NodeContainer
CreateEndDevices(int nDevices, MobilityHelper mobility, Ptr<LoraChannel> channel)
{
    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LorawanHelper
    LorawanHelper helper = LorawanHelper();

    // Create a set of nodes
    NodeContainer endDevices;
    endDevices.Create(nDevices);

    // Assign a mobility model to the node
    mobility.Install(endDevices);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetType("ns3::EndDeviceLoraPhy");
    macHelper.SetType("ns3::ClassAEndDeviceLorawanMac");
    helper.Install(phyHelper, macHelper, endDevices);

    return endDevices;
}

NodeContainer
CreateGateways(int nGateways, MobilityHelper mobility, Ptr<LoraChannel> channel)
{
    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LorawanHelper
    LorawanHelper helper = LorawanHelper();

    // Create the gateways
    NodeContainer gateways;
    gateways.Create(nGateways);

    mobility.Install(gateways);

    // Create a netdevice for each gateway
    phyHelper.SetType("ns3::GatewayLoraPhy");
    macHelper.SetType("ns3::GatewayLorawanMac");
    helper.Install(phyHelper, macHelper, gateways);

    return gateways;
}

Ptr<Node>
CreateNetworkServer(NodeContainer endDevices, NodeContainer gateways)
{
    // Create the NetworkServer node
    Ptr<Node> nsNode = CreateObject<Node>();

    // PointToPoint links between gateways and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        p2p.Install(nsNode, *gw);
    }

    // Install server application
    NetworkServerHelper networkServerHelper = NetworkServerHelper();
    networkServerHelper.SetEndDevices(endDevices);
    networkServerHelper.Install(nsNode);

    // Install a forwarder on the gateways
    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);

    return nsNode;
}

NetworkComponents
InitializeNetwork(int nDevices, int nGateways)
{
    // This function sets up a network with some devices and some gateways, and
    // returns the created nodes through a NetworkComponents struct.

    Ptr<LoraChannel> channel = CreateChannel();

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
                                  "rho",
                                  DoubleValue(1000),
                                  "X",
                                  DoubleValue(0.0),
                                  "Y",
                                  DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    NodeContainer endDevices = CreateEndDevices(nDevices, mobility, channel);

    NodeContainer gateways = CreateGateways(nGateways, mobility, channel);

    LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);

    Ptr<Node> nsNode = CreateNetworkServer(endDevices, gateways);

    return {channel, endDevices, gateways, nsNode};
}

} // namespace lorawan
} // namespace ns3
