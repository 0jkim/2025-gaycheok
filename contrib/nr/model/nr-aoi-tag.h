#ifndef NR_AOI_TAG_H
#define NR_AOI_TAG_H

#include "ns3/tag.h"
#include "ns3/tag-buffer.h"
#include "ns3/type-id.h"
#include "ns3/nstime.h"
#include "cstring"

namespace ns3{
    
class NrAoiTag : public Tag
{
    public:
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializedSize(void) const;
        virtual void Serialize(TagBuffer i) const;
        virtual void Deserialize(TagBuffer i);
        virtual void Print(std::ostream &os) const;

        void SetCTimeStamp(Time time);
        Time GetCTimeStamp(void) const;
    
    private:
        Time m_timeStamp;
};
}

#endif  /* NR_AOI_TAG_H */