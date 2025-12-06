#include "ns3/nr-aoi-tag.h"
#include "ns3/uinteger.h"

namespace ns3 {

TypeId NrAoiTag::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::NrAoiTag")
        .SetParent<Tag>()
        .AddConstructor<NrAoiTag> ();
    return tid;
}

TypeId NrAoiTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t NrAoiTag::GetSerializedSize (void) const
{
  return sizeof(m_timeStamp);
}

void NrAoiTag::Serialize (TagBuffer i) const
{
  i.WriteU64(m_timeStamp.GetNanoSeconds ());
}

void NrAoiTag::Deserialize (TagBuffer i)
{
  m_timeStamp = NanoSeconds (i.ReadU64 ());
}

void NrAoiTag::Print (std::ostream &os) const
{
  os << "Timestamp=" << m_timeStamp;
}

void NrAoiTag::SetCTimeStamp(Time time)
{
  m_timeStamp = time;
}

Time NrAoiTag::GetCTimeStamp(void) const
{
  return m_timeStamp;
}

}