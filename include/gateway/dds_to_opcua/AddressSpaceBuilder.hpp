#pragma once

#include <open62541/server.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "gateway/types/OpcUaHelpers.hpp"

namespace gateway {
namespace dds_to_opcua {

struct InstanceNodeInfo {
    std::string instance_id;
    UA_NodeId instance_node_id;
    UA_NodeId sample_state_node_id;
    UA_NodeId view_state_node_id;
    UA_NodeId instance_state_node_id;
    UA_NodeId source_timestamp_node_id;
    UA_NodeId instance_handle_node_id;
    UA_NodeId valid_data_node_id;
    std::map<std::string, UA_NodeId> field_node_ids;
};

class AddressSpaceBuilder {
public:
    explicit AddressSpaceBuilder(UA_Server* server);
    ~AddressSpaceBuilder();

    // Create Domain Object: Domain_<DomainId> (§9.3.2)
    UA_NodeId createDomainNode(int32_t domainId);

    // Create Topic Object under Domain (§9.3.3)
    UA_NodeId createTopicNode(const UA_NodeId& domainNodeId, 
                             const std::string& topicName, 
                             const std::string& registeredTypeName,
                             bool isKeyed);

    // Create Instance Node under Topic (§9.3.4)
    InstanceNodeInfo createInstanceNode(const UA_NodeId& topicNodeId,
                                        const std::string& topicName,
                                        const std::string& instanceId,
                                        const std::vector<std::pair<std::string, const UA_DataType*>>& fields,
                                        const std::string& keyFieldName = "");

    // Update SampleInfo properties (§9.3.4.2)
    void updateSampleInfo(const InstanceNodeInfo& info,
                          const std::string& sampleState,
                          const std::string& viewState,
                          const std::string& instanceState,
                          int64_t sourceTimestampSec,
                          uint32_t sourceTimestampNano,
                          const std::string& instanceHandle,
                          bool validData);

    // Update field values
    void updateInt32Field(const UA_NodeId& fieldNodeId, int32_t val);
    void updateStringField(const UA_NodeId& fieldNodeId, const std::string& val);
    void updateDoubleField(const UA_NodeId& fieldNodeId, double val);
    void updateBooleanField(const UA_NodeId& fieldNodeId, bool val);

private:
    UA_Server* server_;
};

} // namespace dds_to_opcua
} // namespace gateway
