/**
 * This scenario simulates a 5G-enabled Industrial IoT (IIoT) factory floor.
 *
 * It is designed to be the foundation for analyzing the trade-off between
 * Age of Information (AoI) and energy efficiency.
 *
 * Scenario Setup:
 * - 1 gNB (base station) at the center of the factory.
 * - A configurable number of static UEs (sensors/actuators) placed in fixed positions.
 * - Channel Model: 3GPP TR 38.901 Indoor Factory (InF-DH).
 */

#include "ns3/antenna-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/nr-eps-bearer-tag.h"
#include "ns3/grid-scenario-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/cc-bwp-helper.h"
#include "ns3/grid-scenario-helper.h"
#include "ns3/nr-ue-rrc.h"
#include "ns3/nr-net-device.h"
#include "ns3/tag.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "ns3/nr-aoi-tag.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IIoTFactoryScenario");
static bool g_rxPdcpCallbackCalled = false;
static bool g_rxRxRlcPDUCallbackCalled = false;

static uint64_t totalPdcpDelay = 0;
static uint32_t totalPdcpPackets = 0;
static double averagePdcpDelayNs = 0.0;

Time g_txPeriod = Seconds(0.1);
Time delay;
std::fstream m_ScenarioFile;

/**
 * TODO: 0jkim
 * Device Sensor Type 정의해야함
 */
enum class SensorType {
    SimpleSensor
};

struct TrafficProfile {
    uint32_t payloadBytes;              // 패킷 바이트
    uint32_t period;                    // 기본 전송 주기
};

/**
 * TODO: 0jkim
 * IIoT Sensor Type 정의 해야함
 */
static TrafficProfile kProfiles[] = {
  {500, 10}
};

class MyModel : public Application
{
  public:
    MyModel();
    virtual ~MyModel();

    void Setup(Ptr<NetDevice> device,
                Address address,
                uint32_t packetSize,
                uint32_t nPackets,
                DataRate dataRate,
                uint8_t period,
                const TrafficProfile &prof,
                SensorType type);

    // DL
    void SendPacketDl();
    void ScheduleTxDl();

    // UL
    void SendPacketUl();
    void ScheduleTxUl(uint8_t period);
    void ScheduleTxUl_Configuration();

  private:
    Ptr<NetDevice> m_device;
    Address m_addr;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    uint8_t m_periodicity;
    uint32_t m_deadline;
    TrafficProfile m_prof;
    SensorType m_type;
};

MyModel::MyModel()
    : m_device(),
      m_addr(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0),
      m_periodicity(0),
      m_deadline(0)
{
}

MyModel::~MyModel()
{
}

void MyModel::Setup(Ptr<NetDevice> device,
                Address address,
                uint32_t packetSize,
                uint32_t nPackets,
                DataRate dataRate,
                uint8_t period,
                const TrafficProfile& prof,
                SensorType type)
{
    m_device = device;
    m_addr = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_running = true;
    m_packetsSent = 0;
    m_periodicity = period;
    m_prof = prof;
    m_type = type;
}

void StartApplicationDl(Ptr<MyModel> model)
{
    model->SendPacketDl();
}

void MyModel::SendPacketDl()
{
    Ptr<Packet> pkt = Create<Packet>(m_packetSize);
    Ipv4Header ipv4Header;
    ipv4Header.SetProtocol(Ipv4L3Protocol::PROT_NUMBER);
    pkt->AddHeader(ipv4Header);

    NrEpsBearerTag tag(1, 1);
    pkt->AddPacketTag(tag);

    m_device->Send(pkt, m_addr, Ipv4L3Protocol::PROT_NUMBER);
    NS_LOG_INFO("Sending DL");

    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTxDl();
    }
}

void MyModel::ScheduleTxDl()
{
    if (m_running)
    {
        Time tNext = MilliSeconds(2);
        m_sendEvent = Simulator::Schedule(tNext, &MyModel::SendPacketDl, this);
    }
}

void StartApplicationUl(Ptr<MyModel> model)
{
    model->SendPacketUl();
}

void MyModel::SendPacketUl()
{
    uint32_t PacketSize = m_prof.payloadBytes;
    Ptr<Packet> pkt = Create<Packet>(PacketSize);

    Ipv4Header ipv4Header;
    ipv4Header.SetProtocol(Ipv4L3Protocol::PROT_NUMBER);
    pkt->AddHeader(ipv4Header);

    /**
     * TODO: 0jkim
     * 패킷 생성 시간 테그에 저장
     */
    NrAoiTag aoiTag;
    Time now = Simulator::Now();
    aoiTag.SetCTimeStamp(now);
    pkt->AddPacketTag(aoiTag);

    m_device->Send(pkt, m_addr, Ipv4L3Protocol::PROT_NUMBER);
    NS_LOG_INFO("Sending UL");

    ScheduleTxUl(m_prof.period);
}

void MyModel::ScheduleTxUl(uint8_t period)
{
    if (m_running)
    {
        Time tNext = MilliSeconds(period);
        m_sendEvent = Simulator::Schedule(tNext, &MyModel::SendPacketUl, this);
    }
}

void MyModel::ScheduleTxUl_Configuration(void)
{
    uint8_t configurationTime = 60;
    Time tNext = MilliSeconds(configurationTime);
    m_sendEvent = Simulator::Schedule(tNext, &MyModel::SendPacketUl, this);
}

void RxRlcPDU(std::string path, uint16_t rnti, uint8_t lcid, uint32_t bytes, uint64_t rlcDelay)
{
    g_rxRxRlcPDUCallbackCalled = true;
    delay = Time::FromInteger(rlcDelay, Time::NS);

    m_ScenarioFile << "\n\n Data received at RLC layer at:" << Simulator::Now() << std::endl;
    m_ScenarioFile << "\n rnti:" << rnti << std::endl;
    m_ScenarioFile << "\n delay :" << rlcDelay << std::endl;
}

void RxPdcpPDU(std::string path, uint16_t rnti, uint8_t lcid, uint32_t bytes, uint64_t pdcpDelay)
{
    g_rxPdcpCallbackCalled = true;

    totalPdcpDelay += pdcpDelay;
    totalPdcpPackets += 1;

    averagePdcpDelayNs = (double)totalPdcpDelay / totalPdcpPackets;
}

void ConnectUlPdcpRlcTraces()
{
    Config::Connect("/NodeList/*/DeviceList/*/NrGnbRrc/UeMap/*/DataRadioBearerMap/*/NrPdcp/RxPDU",
                    MakeCallback(&RxPdcpPDU));

    Config::Connect("/NodeList/*/DeviceList/*/NrGnbRrc/UeMap/*/DataRadioBearerMap/*/NrRlc/RxPDU",
                    MakeCallback(&RxRlcPDU));
    NS_LOG_INFO("Received PDCP RLC UL");
}

int main(int argc, char* argv[])
{
    uint16_t numerologyBwp1 = 0;
    double centralFrequencyBand1 = 700e6;
    double bandwidthBand1 = 20e6;

    uint16_t numGnb = 1;
    uint16_t numUe = 100;    
    bool enableUl = true;
    
    uint32_t sizePacket = 512;
    uint8_t period = 10;
    uint32_t numPacketSampling = 10000;
    
    Time simTime = Seconds(10);

    uint32_t openGymPort = 5555;

    RngSeedManager::SetSeed(45);
    RngSeedManager::SetRun(1);

    // Parsing CommandLine
    CommandLine cmd;
    cmd.AddValue("numerologyBwp1", "The numerology to be used in bandwidth part 1", numerologyBwp1);
    cmd.AddValue("centralFrequencyBand1", "The system frequncy to be used in band 1", centralFrequencyBand1);
    cmd.AddValue("bandwidthBand1", "The system bandwidth to be used in band 1", bandwidthBand1);
    cmd.AddValue("sizePacket", "Size of packet in bytes", sizePacket);
    cmd.AddValue("enableUl", "The scheduler link type", enableUl);
    cmd.AddValue("numGNB", "Number of gNB", numGnb);
    cmd.AddValue("numUE", "Number of UE", numUe);
    cmd.Parse(argc, argv);

    int64_t randomStream = 1;

    /**
     * TODO: 0jkim
     * gNB 및 UE grid 설정 구체적으로 구성해야함
     */
    double areaWidth = 50.0; // 시뮬레이션 영역 가로 (사용자님의 최신 코드 기준)
    double areaHeight = 50.0; // 시뮬레이션 영역 세로 (사용자님의 최신 코드 기준)
    double gnbHeight = 10.0; // gNB 높이 (사용자님의 최신 코드 기준)
    double ueHeight = 1.5; // UE 높이
    uint32_t mobileUeNum = 5; // 이동성 UE 수

    GridScenarioHelper gridHelper;

    gridHelper.SetSectorization(GridScenarioHelper::SINGLE);
    gridHelper.SetRows(5); // 5x6 그리드
    gridHelper.SetColumns(6);
    gridHelper.SetBsNumber(numGnb);
    gridHelper.SetUtNumber(numUe);

    // SetStartingPosition을 하지않으면 기본값 (0,0,0)으로 gNB가 배치됨
    gridHelper.SetScenarioLength(areaWidth);
    gridHelper.SetScenarioHeight(areaHeight);
    gridHelper.SetStartingPosition(Vector(areaWidth / 2, areaHeight / 2, 0)); // 중앙에서 시작
    gridHelper.SetBsHeight(gnbHeight);
    gridHelper.SetUtHeight(ueHeight);
    randomStream += gridHelper.AssignStreams(randomStream);

    // 시나리오를 생성하게 되면 gNB, UE 노드가 생성되고, 모든 노드에 ConstantPositionMobilityModel이 설치됨
    gridHelper.CreateScenario();

    NodeContainer gnbNode = gridHelper.GetBaseStations();
    NodeContainer ueNodes = gridHelper.GetUserTerminals();
    
    NodeContainer mobileUeNodes;
    Ptr<ListPositionAllocator> mobileUePos = CreateObject<ListPositionAllocator>();
    for(uint32_t i=0;i<mobileUeNum;++i)
    {
        mobileUeNodes.Add(ueNodes.Get(i));
        mobileUePos->Add(ueNodes.Get(i)->GetObject<MobilityModel>()->GetPosition());
    }
    MobilityHelper mobilityHelper;
    mobilityHelper.SetPositionAllocator(mobileUePos);
    mobilityHelper.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"),
        "Time", StringValue("2s"),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=30.0]"),
        "Bounds", RectangleValue(Rectangle(0, areaWidth, 0, areaHeight))
        );
    mobilityHelper.Install(mobileUeNodes);

    
    /**
     * Setting helper
     * Beamforming Helper: IdealBeamformingHelper
     * IdealBeamforming은 gNB가 UE의 위치나 채널 경로를 완벽하게 알고 있어서 항상 최적의 빔을 형성할 수 있다고 가정하는 모델
     * 따라서 SRS와 무관하게 채널 모델이 채널 상태 업데이트를 수행할 수 있음
     */
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<NrChannelHelper> nrChannelHelper = CreateObject<NrChannelHelper>();
    CcBwpCreator ccBwpCreator;
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);
    
    
    /**
     * Sounding Reference Signal(SRS)
     * SRS 활성화 시, uplink 채널상태를 보고해서 gNB의 CSI, CQI 계산에 사용된다
     * 단, Beamforming model에 따라서 필요 유무 확인필요
     */
    // nrHelper->SetSchedulerAttribute("SrsSymbols", UintegerValue(1));

    /**
     * HARQ
     * HARQ 재전송 on/off
     */
    nrHelper->SetSchedulerAttribute("EnableHarqReTx", BooleanValue(true));

    /**
     * Setting bwp and spectrums
     * Single cc, bandwidthpart
     */
    const uint8_t numCcPerBand = 1;
    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator::SimpleOperationBandConf bandConf1(
        centralFrequencyBand1,
        bandwidthBand1,
        numCcPerBand
    );
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);

    /**
     * TODO: 0jkim
     * 버전 바뀌면서 nrChannelHelper가 생김 <- 확인
     * Scenario: Indoor Factory(InF)
     * ChannelModel: ThreeGPP
     * Conditions: Default
     * 
     * UpdatePeriod는 ChannelModel(=ThreeGpp)이 100ms마다 채널의 물리적 상태(페이딩, 경로 손실 등)을 다시 계산 하라는 지시
     */
    nrChannelHelper->ConfigureFactories("InF", "Default", "NYU");
    nrChannelHelper->AssignChannelsToBands({band1});
    nrChannelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    
    /**
     * Setting antenna model 
     */
    nrHelper->SetGnbAntennaAttribute("NumRows",UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));

    /**
     * Setting scheduling params
     */
    uint8_t schedAlgo = 1;
    if(schedAlgo == 1)
    {
        nrHelper->SetSchedulerTypeId(NrMacSchedulerOfdmaPF::GetTypeId());
    }
    else if(schedAlgo == 2)
    {
        nrHelper->SetSchedulerTypeId(NrMacSchedulerOfdmaQos::GetTypeId());
    }
    else if(schedAlgo == 3)
    {
        /**
         * TODO: 0jkim
         * EWA 알고리즘 세팅
         * env 객체, EWA 스케줄러 객체를 생성
         * OpenGymInterface을 gymport 5555로 연결
         * env 객체에 EWA 스케줄러, 설정
         * NotifyCurrentIteration으로 콜백 함수 등록
         * ActivateUlEWA 활성화
         */
    }
    nrHelper->SetSchedulerAttribute("FixedMcsDl", BooleanValue(false));
    nrHelper->SetSchedulerAttribute("StartingMcsDl", UintegerValue(4));
    nrHelper->SetSchedulerAttribute("FixedMcsUl", BooleanValue(false));
    nrHelper->SetSchedulerAttribute("StartingMcsUl", UintegerValue(4));

    /**
     * Setting Fading, Shadowing, AMC, Error Model
     */
    nrChannelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(true));
    std::string errorModel = "ns3::NrEesmIrT1";
    nrHelper->SetUlErrorModel(errorModel);
    nrHelper->SetDlErrorModel(errorModel);
    nrHelper->SetGnbDlAmcAttribute(
        "AmcModel",
        EnumValue(NrAmc::ErrorModel)); // NrAmc::ShannonModel or NrAmc::ErrorModel
    nrHelper->SetGnbUlAmcAttribute(
        "AmcModel",
        EnumValue(NrAmc::ErrorModel)); // NrAmc::ShannonModel or NrAmc::ErrorModel
    allBwps = CcBwpCreator::GetAllBwps({band1});
    
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbNode, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);
    
    /**
     * NrGnbPhy Numerology
     */
    nrHelper->GetGnbPhy(gnbNetDev.Get(0),0)->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
    for (auto it = gnbNetDev.Begin(); it != gnbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }
    for (auto it = ueNetDev.Begin(); it != ueNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }
    
    /**
     * Apply randomStream to mobileUeNodes
     */
    randomStream += mobilityHelper.AssignStreams(mobileUeNodes, randomStream);
    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);

    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(23.0));
    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(30.0));
    
    /**
     * Setting Ip
     * Just Simple Core Network <NrPointToPoint>
     */
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpInterface;
    ueIpInterface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));

    /**
     * Uplink Traffic Schedule
     * TODO: 0jkim
     * 센서 타입 정의 한 후, SensorType::으로 설정해주어야함 (현재 임시로 1개)
     */
    std::vector<Ptr<MyModel>> v_modelUl(numUe);
    for(uint32_t i = 0; i<numUe; ++i)
    {
        SensorType type = SensorType::SimpleSensor;
        const TrafficProfile &prof = kProfiles[(int)type];

        Ptr<MyModel> modelUl = CreateObject<MyModel>();
        modelUl->Setup(
            ueNetDev.Get(i),
            gnbNetDev.Get(0)->GetAddress(),
            sizePacket,
            numPacketSampling,
            DataRate("1Mbps"),
            period,
            prof,
            type
        );

        v_modelUl[i] = modelUl;

        Time startDelay = Seconds(1);
        Simulator::Schedule(startDelay, &StartApplicationUl, v_modelUl[i]);
    }

    /**
     * Attach all UEs to closest gNB
     */
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);
    for (uint32_t i = 0; i < ueNetDev.GetN(); ++i)
    {
        auto dev = DynamicCast<NrUeNetDevice>(ueNetDev.Get(i));
        if (dev->GetTargetGnb() == nullptr)
        {
            std::cout << "UE[" << i << "] attach failed!" << std::endl;
        }
        else
        {
            std::cout << "UE[" << i << "] attached to gNB" << std::endl;
        }
    }

    // Enable Tracing
    Simulator::Schedule(Seconds(0.67), &ConnectUlPdcpRlcTraces);
    nrHelper->EnableTraces();



    std::cout << "--- GridScenarioHelper Created Positions ---" << std::endl;
    // GNB 위치 확인
    NodeContainer tempGnbNodes = gridHelper.GetBaseStations();
    if (tempGnbNodes.GetN() > 0)
    {
        Ptr<MobilityModel> gnbMob = tempGnbNodes.Get(0)->GetObject<MobilityModel>();
        if (gnbMob)
        {
            std::cout << "gNB 0 Initial Position: " << gnbMob->GetPosition() << std::endl;
        }
    }
    // UE 위치 확인
    NodeContainer tempUeNodes = gridHelper.GetUserTerminals();
    for (uint32_t i = 0; i < tempUeNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> ueMob = tempUeNodes.Get(i)->GetObject<MobilityModel>();
        if (ueMob)
        {
            std::cout << "UE " << i << " Initial Position: " << ueMob->GetPosition() << std::endl;
        }
        else
        {
            std::cout << "UE " << i << " has no MobilityModel after CreateScenario." << std::endl;
        }
    }
    std::cout << "--------------------------------------------" << std::endl;




    Simulator::Stop(simTime);
    Simulator::Run();

    // --- Print AoI Statistics ---
    Ptr<NrGnbNetDevice> gnbNetDevice = DynamicCast<NrGnbNetDevice>(gnbNetDev.Get(0));
    if (gnbNetDevice)
    {
        Ptr<NrGnbMac> gnbMac = DynamicCast<NrGnbMac>(gnbNetDevice->GetMac(0)); // Cast to NrGnbMac
        if (gnbMac)
        {
            gnbMac->PrintAoiStatistics();
        }
    }

    std::cout << "\nAverage PDCP Delay (ns): " << averagePdcpDelayNs << "\n";
    std::cout << "\n FIN. " << std::endl;

     if (g_rxPdcpCallbackCalled && g_rxRxRlcPDUCallbackCalled)
    {
        return EXIT_SUCCESS;
    }
    else
    {
        return EXIT_FAILURE;
    }

    Simulator::Destroy();
    return 0;
}