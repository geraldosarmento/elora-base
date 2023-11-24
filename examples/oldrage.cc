/*
 * This program creates a simple network which uses an ADR algorithm to set up
 * the Spreading Factors of the devices in the Network.
 */

#include "ns3/building-penetration-loss.h"
#include "ns3/building-allocator.h"
#include "ns3/buildings-helper.h"
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/core-module.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/log.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/lorawan-helper.h"
#include "ns3/lora-net-device.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-module.h"
#include "ns3/network-server-helper.h"
#include "ns3/node-container.h"
//#include "ns3/okumura-hata-propagation-loss-model.h"
#include "ns3/periodic-sender.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/pointer.h"
#include "ns3/position-allocator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rectangle.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>



using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("AdrExample");

bool const NSadrEnabled = true;
bool EDadrEnabled = true;
bool confirmedMode = true;
bool initializeSF = false;
bool buildingAllocation = true;
bool poissonModel = false;
int baseSeed = 0;
double mobileNodeProbability = 0;
int nDevices = 700;
int nGateways = 1;
int nPeriods = 7;
double simulationTime = (24*60*60 * nPeriods);
//double simulationTime = (12*60*60);  //teste
double pktsPerDay = 24;
double pktsPerSecs = pktsPerDay/86400;
double appPeriodSecs = (1/pktsPerSecs); 
int packetSize = 40;  
double radius = 5000;
double edHeight = 1.5;
double gwHeight = 15.0;
double maxRandomLoss = 10;
double minSpeed = 0.5;
double maxSpeed = 1.5;
std::string adrType = "ns3::AdrLorawan";
std::string rootPath = "scratch/output/";     // base path to output data
std::string filename;
std::ofstream outputFile;
bool saveToFile = true;
bool verbose = false;

// Initial Transmission Params. Used in setTransmParams() function
double BW = 125000;                // Bandwidth (Hz)
double TP = 14;                    // Assume devices transmit at 14 dBm
int SF = 12;                        // Spreading Factor 
//int CR = 4;                        // Coding Rate (4/(4+CR))

void setInitialTxParams (NodeContainer endDevices) {
    
for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
    {
    Ptr<LoraNetDevice> loraNetDevice = (*j)->GetDevice (0)->GetObject<LoraNetDevice> ();
    Ptr<EndDeviceLoraPhy> phy = loraNetDevice->GetPhy ()->GetObject<EndDeviceLoraPhy> ();
    Ptr<ClassAEndDeviceLorawanMac> mac = loraNetDevice->GetMac ()->GetObject<ClassAEndDeviceLorawanMac> ();
    //Ptr<BaseEndDeviceLorawanMac> mac = loraNetDevice->GetMac ()->GetObject<BaseEndDeviceLorawanMac> ();
    
    
    mac->SetDataRate ( 12 - SF );  //DR = 12 - SF
    mac->SetTransmissionPower( TP );
    mac->SetBandwidthForDataRate(
    std::vector<double>{BW, BW, BW, BW, BW, BW, BW});

    mac->SetFType (LorawanMacHeader::CONFIRMED_DATA_UP);
    // Enable data rate adaptation in the retransmitting procedure at ED.
    
    //if( (adrType == "ns3::AdrGaussian") || (adrType == "ns3::AdrEMA") )
    mac->SetADRBackoff(EDadrEnabled);    
    }
}

// Trace sources that are called when a node changes its DR or TX power
void
OnDataRateChange(uint8_t oldDr, uint8_t newDr)
{
    NS_LOG_DEBUG("DR" << unsigned(oldDr) << " -> DR" << unsigned(newDr));
}

void
OnTxPowerChange(double oldTxPower, double newTxPower)
{
    NS_LOG_DEBUG(oldTxPower << " dBm -> " << newTxPower << " dBm");
}


double currentTime = 1.0;
double lambda = appPeriodSecs;
unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
std::mt19937 gen(seed);
std::exponential_distribution<double> distribution(lambda);

double getPoissonTime()
{
  double timeToNextPacket = distribution(gen);
  
  currentTime += timeToNextPacket;
  //currentTime = timeToNextPacket;  //temporario - ajustando pra segundos
  //std::cout << "timeToNextPacket: " << timeToNextPacket << std::endl;

  return currentTime;
}


int
main(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.AddValue("verbose", "Whether to print output or not", verbose);
    cmd.AddValue("adrType", "ADR Class [ns3::AdrLorawan, ns3::AdrPlus, ns3::AdrGaussian, ns3::AdrEma]", adrType);
    cmd.AddValue("MultipleGwCombiningMethod", "ns3::AdrComponent::MultipleGwCombiningMethod");
    cmd.AddValue("MultiplePacketsCombiningMethod",
                 "ns3::AdrComponent::MultiplePacketsCombiningMethod");
    cmd.AddValue("HistoryRange", "ns3::AdrComponent::HistoryRange");
    cmd.AddValue("FType", "ns3::BaseEndDeviceLorawanMac::FType");
    cmd.AddValue("confMode", "Whether to use confirmed mode or not", confirmedMode);
    cmd.AddValue("poisson", "Whether to use Poisson model or not", poissonModel);
    cmd.AddValue("ADRBackoff", "ns3::BaseEndDeviceLorawanMac::ADRBackoff");
    cmd.AddValue("ChangeTransmissionPower", "ns3::AdrComponent::ChangeTransmissionPower");
    //cmd.AddValue("NSadrEnabled", "Whether to enable ADR", NSadrEnabled);
    cmd.AddValue("EDadrEnabled", "Whether to enable ADR in ED level", EDadrEnabled);
    cmd.AddValue("nDevices", "Number of devices to simulate", nDevices);
    cmd.AddValue("PeriodsToSimulate", "Number of periods to simulate", nPeriods);
    cmd.AddValue("radius", "The radius of the area to simulate", radius);
    cmd.AddValue("MobileNodeProbability",
                 "Probability of a node being a mobile node",
                 mobileNodeProbability);
    cmd.AddValue("baseSeed", "Which seed value to use on RngSeedManager", baseSeed);
    cmd.AddValue("maxRandomLoss",
                 "Maximum amount in dB of the random loss component",
                 maxRandomLoss);
    //cmd.AddValue("gatewayDistance", "Distance between gateways", gatewayDistance);
    cmd.AddValue("initializeSF", "Whether to initialize the SFs", initializeSF);
    cmd.AddValue("MinSpeed", "Minimum speed for mobile devices", minSpeed);
    cmd.AddValue("MaxSpeed", "Maximum speed for mobile devices", maxSpeed);
    cmd.AddValue("NbTrans", "ns3::BaseEndDeviceLorawanMac::NbTrans");
    cmd.Parse(argc, argv);

    // Setting simulation seed
    if (baseSeed > 0)
        RngSeedManager::SetRun(baseSeed);
    else {
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        RngSeedManager::SetSeed(seed);
    }
    
    // Logging
    //////////

    LogComponentEnable("AdrExample", LOG_LEVEL_ALL);
    
    // Set the EDs to require Data Rate control from the NS
    Config::SetDefault("ns3::BaseEndDeviceLorawanMac::ADRBit", BooleanValue(true));

    Config::SetDefault("ns3::BaseEndDeviceLorawanMac::ADRBackoff", BooleanValue(true));

   /************************
   *  Create the channel  *
   ************************/
  // Create the lora channel object
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    if (buildingAllocation)
    {
        // Create the correlated shadowing component
        Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
            CreateObject<CorrelatedShadowingPropagationLossModel>();

        // Aggregate shadowing to the logdistance loss
        loss->SetNext(shadowing);

        // Add the effect to the channel propagation loss
        Ptr<BuildingPenetrationLoss> buildingLoss = CreateObject<BuildingPenetrationLoss>();

        shadowing->SetNext(buildingLoss);
    }

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

   /************************
   *  Create the helpers  *
   ************************/

    // End Device mobility
    MobilityHelper mobilityEd;
    MobilityHelper mobilityGw;
    mobilityEd.SetPositionAllocator ("ns3::UniformDiscPositionAllocator", "rho", DoubleValue (radius),
                                 "X", DoubleValue (0.0), "Y", DoubleValue (0.0));

    // Gateway mobility
    Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator> ();
    positionAllocGw->Add (Vector (0.0, 0.0, gwHeight));
    // positionAllocGw->Add (Vector (-5000.0, -5000.0, 15.0));
    // positionAllocGw->Add (Vector (-5000.0, 5000.0, 15.0));
    // positionAllocGw->Add (Vector (5000.0, -5000.0, 15.0));
    // positionAllocGw->Add (Vector (5000.0, 5000.0, 15.0));
    mobilityGw.SetPositionAllocator (positionAllocGw);
    mobilityGw.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    

    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LorawanHelper
    LorawanHelper helper = LorawanHelper();
    helper.EnablePacketTracking();

   /************************
   *  Create Gateway  *
   ************************/

    NodeContainer gateways;
    gateways.Create(nGateways);
    mobilityGw.Install(gateways);

    // Create the LoraNetDevices of the gateways
    phyHelper.SetType("ns3::GatewayLoraPhy");
    macHelper.SetType("ns3::GatewayLorawanMac");
    helper.Install(phyHelper, macHelper, gateways);

    /************************
   *  Create End Devices  *
   ************************/

    NodeContainer endDevices;
    endDevices.Create(nDevices);

    // Install mobility model on fixed nodes
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    int fixedPositionNodes = double(nDevices) * (1 - mobileNodeProbability);
    for (int i = 0; i < fixedPositionNodes; ++i)
    {
        mobilityEd.Install(endDevices.Get(i));

        // Adjusting ED height 
        Ptr<MobilityModel> mobility = endDevices.Get (i)->GetObject<MobilityModel> ();
        Vector position = mobility->GetPosition ();
        position.z = edHeight;
        mobility->SetPosition (position);
    }
    // Install mobility model on mobile nodes
    mobilityEd.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds",
        RectangleValue(Rectangle(-radius, radius, -radius, radius)),
        "Distance",
        DoubleValue(1000),
        "Speed",
        PointerValue(CreateObjectWithAttributes<UniformRandomVariable>("Min",
                                                                       DoubleValue(minSpeed),
                                                                       "Max",
                                                                       DoubleValue(maxSpeed))));
    for (int i = fixedPositionNodes; i < (int)endDevices.GetN(); ++i)
    {
        mobilityEd.Install(endDevices.Get(i));

        // Adjusting ED height 
        Ptr<MobilityModel> mobility = endDevices.Get (i)->GetObject<MobilityModel> ();
        Vector position = mobility->GetPosition ();
        position.z = edHeight;
        mobility->SetPosition (position);
    }

    // Create a LoraDeviceAddressGenerator
    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetType("ns3::EndDeviceLoraPhy");
    macHelper.SetType("ns3::ClassAEndDeviceLorawanMac");
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);
    helper.Install(phyHelper, macHelper, endDevices);

    // Set initial transmission parameters
    setInitialTxParams(endDevices);


    /**********************
     *  Handle buildings  *
     **********************/
    /*
    double xLength = 130;
    double deltaX = 32;
    double yLength = 64;
    double deltaY = 17;
    */
    double xLength = 200;
    double deltaX = 150;
    double yLength = 200;
    double deltaY = 150;

    int gridWidth = 2 * radius / (xLength + deltaX);
    int gridHeight = 2 * radius / (yLength + deltaY);
    if (!buildingAllocation)
    {
        gridWidth = 0;
        gridHeight = 0;
    }
    Ptr<GridBuildingAllocator> gridBuildingAllocator;
    gridBuildingAllocator = CreateObject<GridBuildingAllocator>();
    gridBuildingAllocator->SetAttribute("GridWidth", UintegerValue(gridWidth));
    gridBuildingAllocator->SetAttribute("LengthX", DoubleValue(xLength));
    gridBuildingAllocator->SetAttribute("LengthY", DoubleValue(yLength));
    gridBuildingAllocator->SetAttribute("DeltaX", DoubleValue(deltaX));
    gridBuildingAllocator->SetAttribute("DeltaY", DoubleValue(deltaY));
    gridBuildingAllocator->SetAttribute("Height", DoubleValue(6));
    gridBuildingAllocator->SetBuildingAttribute("NRoomsX", UintegerValue(2));
    gridBuildingAllocator->SetBuildingAttribute("NRoomsY", UintegerValue(4));
    gridBuildingAllocator->SetBuildingAttribute("NFloors", UintegerValue(2));
    gridBuildingAllocator->SetAttribute(
        "MinX",
        DoubleValue(-gridWidth * (xLength + deltaX) / 2 + deltaX / 2));
    gridBuildingAllocator->SetAttribute(
        "MinY",
        DoubleValue(-gridHeight * (yLength + deltaY) / 2 + deltaY / 2));
    BuildingContainer bContainer = gridBuildingAllocator->Create(gridWidth * gridHeight);

    BuildingsHelper::Install(endDevices);
    BuildingsHelper::Install(gateways);

    // Print the buildings
    if (verbose)
    {
        std::ofstream myfile;
        myfile.open(rootPath+"buildings.txt");
        std::vector<Ptr<Building>>::const_iterator it;
        int j = 1;
        for (it = bContainer.Begin(); it != bContainer.End(); ++it, ++j)
        {
            Box boundaries = (*it)->GetBoundaries();
            myfile << "set object " << j << " rect from " << boundaries.xMin << ","
                   << boundaries.yMin << " to " << boundaries.xMax << "," << boundaries.yMax
                   << std::endl;
        }
        myfile.close();
    }

    /*********************************************
   *  Install applications on the end devices  *
   *********************************************/

    
    Time appStopTime = Seconds (simulationTime);
    PeriodicSenderHelper appHelper = PeriodicSenderHelper ();
    appHelper.SetPeriod (Seconds (appPeriodSecs));
    appHelper.SetPacketSize (packetSize);    
    ApplicationContainer appContainer = appHelper.Install (endDevices);  
    appContainer.Start (Seconds (0));
    appContainer.Stop (appStopTime);
    
    if (poissonModel) {
        if (verbose)
            std::cout << "Enabling Poisson Arrival Model..." << std::endl;

        
        for (uint i = 0; i < appContainer.GetN(); i++)
        {
            Ptr<Application> app = appContainer.Get(i);
            double t = getPoissonTime();
            app->SetStartTime(Seconds(t));
            /*
            if (verbose)
                std::cout << "Poisson Model. Start time at node #" << app->GetNode()->GetId()
                        << ": " << t << std::endl;
            */
        }        
    }
    
   

    // Do not set spreading factors up: we will wait for the NS to do this
    if (initializeSF)
    {
        macHelper.SetSpreadingFactorsUp(endDevices, gateways, channel);
    }

    ////////////
    // Create NS
    ////////////

    NodeContainer networkServers;
    networkServers.Create(1);

    // PointToPoint links between gateways and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        p2p.Install(networkServers.Get(0), *gw);
    }

    // Install the NetworkServer application on the network server
    NetworkServerHelper networkServerHelper;
    networkServerHelper.SetEndDevices(endDevices);
    networkServerHelper.EnableAdr(NSadrEnabled);
    networkServerHelper.SetAdr(adrType);
    networkServerHelper.Install(networkServers);

    // Install the Forwarder application on the gateways
    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);

    // Connect our traces
    if (verbose) {
        Config::ConnectWithoutContext(
            "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::BaseEndDeviceLorawanMac/TxPower",
            MakeCallback(&OnTxPowerChange));
        Config::ConnectWithoutContext(
            "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::BaseEndDeviceLorawanMac/DataRate",
            MakeCallback(&OnDataRateChange));
    }

    // Activate printing of ED MAC parameters
    std::string deviceStatus, gwPerf, globalPerf;
    LoraPacketTracker& tracker = helper.GetPacketTracker ();
    
    if (saveToFile) {    
        deviceStatus = rootPath + "deviceStatus-";
        gwPerf = rootPath + "gwPerf-";
        globalPerf = rootPath + "globalPerf-";

        deviceStatus += adrType + ".csv";
        gwPerf += adrType + ".csv";
        globalPerf += adrType + ".csv";

        Time stateSamplePeriod = Seconds(60*60);

        // Periodically prints the status of devices in the network
        helper.EnablePeriodicDeviceStatusPrinting(endDevices,gateways,deviceStatus,stateSamplePeriod);
        // Periodically prints PHY-level performance at every gateway
        helper.EnablePeriodicGwsPerformancePrinting(gateways, gwPerf, stateSamplePeriod);
        // Periodically prints global performance
        helper.EnablePeriodicGlobalPerformancePrinting(globalPerf, stateSamplePeriod);

        // *-* deviceStatus header (DoPrintDeviceStatus / CountAllDevicesPackets) *-* 
        // time edID posX poxY posZ gwdist DR Tx snt rcvd maxToA ToA
        // (Note: SF = 12 - DR)

        // *-* gwPerf header (PrintPhyPacketsAllGws / CountPhyPacketsAllGws) *-* 
        // time edID snt rcvd intrfd noMoreRcvrs lostTx underSens unset

        // *-* globalPerf header (PrintPhyPacketsGlobally) *-* 
        // time snt rcvd interfered noPaths(noMoreRcvrs) busyGw(lostTx) unset
    }

    ////////////////
    // Simulation //
    ////////////////

    // Additional tolerance time for the end of the simulation
    simulationTime += 1800;   

    // Start simulation 
    NS_LOG_INFO ("Running " << adrType << " scenario...");
    Simulator::Stop (Seconds(simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();


    ///////////////////////////
    // Print results to file //
    ///////////////////////////
    NS_LOG_INFO ("Computing performance metrics...");

  
    std::cout << "UNCONF: " << tracker.CountMacPacketsGlobally( Seconds(60),
                                                Seconds(simulationTime) ) << std::endl;
    std::cout << "CONF  : " << tracker.CountMacPacketsGloballyCpsr( Seconds(60),
                                                Seconds(simulationTime) ) << std::endl;


    if (saveToFile) {
        filename = rootPath + "GlobalPacketCount-" + adrType + ".csv";
        //outputFile.open(filename.c_str(), std::ofstream::out | std::ofstream::app);
        outputFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
        
        //Snt, Rcvd, PDR - For unconfirmed traffic
        outputFile << tracker.CountMacPacketsGlobally( Seconds(60), Seconds(simulationTime) ) << std::endl;

        //// For printing at each hour - For confirmed traffic
        //for(int i = 0; i < (simulationTime/3600)-1; i++)
        //  outputFile << tracker.CountMacPacketsGlobally( Seconds(i*3600), Seconds( (i+1)*3600 ) )<< std::endl;


        outputFile.close();
    }

    return 0;
}
